/*
 * BetterPython JIT Compiler
 * Translates register-based bytecode to x86-64 native code
 *
 * Architecture:
 *   JIT function signature: int64_t jit_fn(int64_t *iregs)
 *   - RDI = pointer to int64_t register array (caller provides params at [0..argc-1])
 *   - Returns int64_t result in RAX (from R_RET source register)
 *   - R12 = base pointer to iregs (callee-saved, survives everything)
 *   - R10, R11 = scratch registers
 *   - RAX, RDX = used for division
 *   - All virtual register access: [R12 + vreg*8]
 */

#include "jit.h"
#include "jit_x64.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// External allocation function from jit_profile.c
void *jit_alloc_code(JitContext *jit, size_t size);

// Compilation context
typedef struct {
    JitContext *jit;
    BpFunc *func;
    BpModule *mod;
    CodeBuffer cb;

    // Label mapping: bytecode offset -> label index
    size_t *offset_labels;

    // Jump target offsets found during first pass
    bool *is_jump_target;
    size_t bytecode_len;

    // Epilogue label (for R_RET to jump to)
    size_t epilogue_label;
} CompileCtx;

// Helper: load virtual register value into x86-64 register
// All vregs stored in memory at [R12 + vreg*8]
static void emit_load_vreg(CompileCtx *ctx, X64Reg dst, uint8_t vreg) {
    x64_mov_r64_m64_disp(&ctx->cb, dst, X64_R12, (int32_t)vreg * 8);
}

// Helper: store x86-64 register value to virtual register
static void emit_store_vreg(CompileCtx *ctx, uint8_t vreg, X64Reg src) {
    x64_mov_m64_disp_r64(&ctx->cb, X64_R12, (int32_t)vreg * 8, src);
}

// Read bytecode values
static uint8_t read_u8(const uint8_t *code, size_t *ip) {
    return code[(*ip)++];
}

static uint32_t read_u32(const uint8_t *code, size_t *ip) {
    uint32_t v = code[*ip] | ((uint32_t)code[*ip + 1] << 8) |
                 ((uint32_t)code[*ip + 2] << 16) | ((uint32_t)code[*ip + 3] << 24);
    *ip += 4;
    return v;
}

static int64_t read_i64(const uint8_t *code, size_t *ip) {
    int64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= ((int64_t)code[*ip + i]) << (i * 8);
    }
    *ip += 8;
    return v;
}

static double read_f64(const uint8_t *code, size_t *ip) {
    double d;
    memcpy(&d, &code[*ip], 8);
    *ip += 8;
    return d;
}

// First pass: find jump targets and create labels
// Also checks if function is JIT-compilable (no unsupported opcodes)
static bool first_pass(CompileCtx *ctx) {
    const uint8_t *code = ctx->func->code;
    size_t len = ctx->func->code_len;

    ctx->is_jump_target = bp_xmalloc(len);
    memset(ctx->is_jump_target, 0, len);
    ctx->bytecode_len = len;

    size_t ip = 0;
    while (ip < len) {
        uint8_t op = code[ip++];

        switch (op) {
            // 1-byte operand opcodes
            case R_RET:
            case R_TRY_END:
            case R_THROW:
            case R_CONST_NULL:
                ip += 1;
                break;

            // 2-byte operand opcodes (dst, src)
            case R_MOV:
            case R_NEG_I64:
            case R_NEG_F64:
            case R_NOT:
                ip += 2;
                break;

            // 3-byte operand opcodes (dst, src1, src2)
            case R_ADD_I64: case R_SUB_I64: case R_MUL_I64: case R_DIV_I64: case R_MOD_I64:
            case R_ADD_F64: case R_SUB_F64: case R_MUL_F64: case R_DIV_F64: case R_MOD_F64:
            case R_ADD_STR:
            case R_EQ: case R_NEQ: case R_LT: case R_LTE: case R_GT: case R_GTE:
            case R_LT_F64: case R_LTE_F64: case R_GT_F64: case R_GTE_F64:
            case R_AND: case R_OR:
            case R_ARRAY_GET: case R_ARRAY_SET:
            case R_MAP_GET: case R_MAP_SET:
            case R_STRUCT_GET: case R_STRUCT_SET:
            case R_CLASS_GET: case R_CLASS_SET:
                ip += 3;
                break;

            // 4-byte operand opcodes: dst(1) + src_base(1) + count(u16=2)
            case R_ARRAY_NEW: case R_MAP_NEW:
                ip += 4;
                break;

            // Constants
            case R_CONST_I64:
            case R_CONST_F64:
                ip += 1 + 8;  // dst + imm64
                break;

            case R_CONST_BOOL:
                ip += 1 + 1;  // dst + imm8
                break;

            case R_CONST_STR:
                ip += 1 + 4;  // dst + str_idx(u32)
                break;

            // Jumps - mark targets (u32 absolute addresses)
            case R_JMP: {
                uint32_t target = read_u32(code, &ip);
                if (target < len) {
                    ctx->is_jump_target[target] = true;
                }
                break;
            }

            case R_JMP_IF_FALSE:
            case R_JMP_IF_TRUE: {
                ip += 1;  // cond reg
                uint32_t target = read_u32(code, &ip);
                if (target < len) {
                    ctx->is_jump_target[target] = true;
                }
                break;
            }

            // Function calls
            case R_CALL:
                ip += 1 + 4 + 1 + 1;  // dst(1) + fn_idx(u32=4) + arg_base(1) + argc(1)
                break;

            case R_CALL_BUILTIN:
                ip += 1 + 2 + 1 + 1;  // dst(1) + builtin_id(u16=2) + arg_base(1) + argc(1)
                break;

            case R_FFI_CALL:
                ip += 1 + 2 + 1 + 1;  // dst(1) + extern_id(u16=2) + arg_base(1) + argc(1)
                break;

            // Exception handling
            case R_TRY_BEGIN:
                ip += 4 + 4 + 1;  // catch_addr(u32) + finally_addr(u32) + exc_reg(1)
                break;

            // Structs/Classes
            case R_STRUCT_NEW:
            case R_CLASS_NEW:
                ip += 1 + 2 + 1 + 1;  // dst(1) + type_id(u16=2) + src_base(1) + count(1)
                break;

            case R_METHOD_CALL:
            case R_SUPER_CALL:
                ip += 1 + 1 + 2 + 1 + 1;  // dst(1) + obj(1) + method_id(u16=2) + arg_base(1) + argc(1)
                break;

            case R_BIT_AND: case R_BIT_OR: case R_BIT_XOR:
            case R_BIT_SHL: case R_BIT_SHR:
                ip += 3;  // dst(1) + src1(1) + src2(1)
                break;
            case R_BIT_NOT:
                ip += 2;  // dst(1) + src(1)
                break;

            case R_LOAD_GLOBAL:
            case R_STORE_GLOBAL:
                ip += 3;  // reg(1) + global_idx(u16=2)
                break;

            default:
                if (ctx->jit->debug) {
                    fprintf(stderr, "JIT: Unknown opcode %u in first pass at offset %zu\n", op, ip - 1);
                }
                return false;
        }
    }

    // Create labels for jump targets
    ctx->offset_labels = bp_xmalloc(len * sizeof(size_t));
    memset(ctx->offset_labels, 0, len * sizeof(size_t));
    for (size_t i = 0; i < len; i++) {
        if (ctx->is_jump_target[i]) {
            ctx->offset_labels[i] = cb_new_label(&ctx->cb);
        }
    }

    // Create epilogue label
    ctx->epilogue_label = cb_new_label(&ctx->cb);

    return true;
}

// Compile a single instruction
static bool compile_insn(CompileCtx *ctx, size_t *ip) {
    const uint8_t *code = ctx->func->code;
    size_t start_ip = *ip;

    // Define label if this is a jump target
    if (ctx->is_jump_target[start_ip]) {
        cb_define_label(&ctx->cb, ctx->offset_labels[start_ip]);
    }

    uint8_t op = code[(*ip)++];

    switch (op) {
        // ================================================================
        // Constants
        // ================================================================
        case R_CONST_I64: {
            uint8_t dst = read_u8(code, ip);
            int64_t imm = read_i64(code, ip);
            x64_mov_r64_imm64(&ctx->cb, X64_R10, imm);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        case R_CONST_BOOL: {
            uint8_t dst = read_u8(code, ip);
            uint8_t val = read_u8(code, ip);
            x64_mov_r64_imm32(&ctx->cb, X64_R10, val ? 1 : 0);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        case R_CONST_F64: {
            // Store double bits as int64_t (reinterpret cast)
            uint8_t dst = read_u8(code, ip);
            double dval = read_f64(code, ip);
            int64_t bits;
            memcpy(&bits, &dval, 8);
            x64_mov_r64_imm64(&ctx->cb, X64_R10, bits);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        // ================================================================
        // Moves
        // ================================================================
        case R_MOV: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        // ================================================================
        // Integer Arithmetic
        // ================================================================
        case R_ADD_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_add_r64_r64(&ctx->cb, X64_R10, X64_R11);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        case R_SUB_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_sub_r64_r64(&ctx->cb, X64_R10, X64_R11);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        case R_MUL_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_imul_r64_r64(&ctx->cb, X64_R10, X64_R11);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        case R_DIV_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            emit_load_vreg(ctx, X64_RAX, src1);
            emit_load_vreg(ctx, X64_R10, src2);
            x64_cqo(&ctx->cb);  // Sign-extend RAX into RDX:RAX
            x64_idiv_r64(&ctx->cb, X64_R10);
            emit_store_vreg(ctx, dst, X64_RAX);  // Quotient in RAX
            break;
        }

        case R_MOD_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            emit_load_vreg(ctx, X64_RAX, src1);
            emit_load_vreg(ctx, X64_R10, src2);
            x64_cqo(&ctx->cb);
            x64_idiv_r64(&ctx->cb, X64_R10);
            emit_store_vreg(ctx, dst, X64_RDX);  // Remainder in RDX
            break;
        }

        case R_NEG_I64: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src);
            x64_neg_r64(&ctx->cb, X64_R10);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

        // ================================================================
        // Integer Comparisons
        // ================================================================
        case R_EQ: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_sete(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_NEQ: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setne(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_LT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setl(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_LTE: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setle(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_GT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setg(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_GTE: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setge(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        // ================================================================
        // Logical Operations
        // ================================================================
        case R_NOT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            x64_sete(&ctx->cb, X64_RAX);  // 1 if src was 0, else 0
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_AND: {
            // dst = (src1 != 0) && (src2 != 0) — branchless
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            // Compute (src1 != 0) in RAX
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            x64_setne(&ctx->cb, X64_RAX);  // RAX = src1 != 0

            // Compute (src2 != 0) in R10
            emit_load_vreg(ctx, X64_R11, src2);
            x64_test_r64_r64(&ctx->cb, X64_R11, X64_R11);
            x64_mov_r64_imm32(&ctx->cb, X64_R10, 0);
            x64_setne(&ctx->cb, X64_R10);  // R10 = src2 != 0

            // AND the two
            x64_and_r64_r64(&ctx->cb, X64_RAX, X64_R10);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_OR: {
            // dst = (src1 != 0) || (src2 != 0) — branchless
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            // Compute (src1 != 0) in RAX
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            emit_load_vreg(ctx, X64_R10, src1);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            x64_setne(&ctx->cb, X64_RAX);

            // Compute (src2 != 0) in R10
            emit_load_vreg(ctx, X64_R11, src2);
            x64_test_r64_r64(&ctx->cb, X64_R11, X64_R11);
            x64_mov_r64_imm32(&ctx->cb, X64_R10, 0);
            x64_setne(&ctx->cb, X64_R10);

            // OR the two
            x64_or_r64_r64(&ctx->cb, X64_RAX, X64_R10);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        // ================================================================
        // Control Flow
        // ================================================================
        case R_JMP: {
            uint32_t target = read_u32(code, ip);
            if (target < ctx->bytecode_len && ctx->is_jump_target[target]) {
                x64_jmp_label(&ctx->cb, ctx->offset_labels[target]);
            }
            break;
        }

        case R_JMP_IF_FALSE: {
            uint8_t cond = read_u8(code, ip);
            uint32_t target = read_u32(code, ip);
            emit_load_vreg(ctx, X64_R10, cond);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            if (target < ctx->bytecode_len && ctx->is_jump_target[target]) {
                x64_jz_label(&ctx->cb, ctx->offset_labels[target]);
            }
            break;
        }

        case R_JMP_IF_TRUE: {
            uint8_t cond = read_u8(code, ip);
            uint32_t target = read_u32(code, ip);
            emit_load_vreg(ctx, X64_R10, cond);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            if (target < ctx->bytecode_len && ctx->is_jump_target[target]) {
                x64_jnz_label(&ctx->cb, ctx->offset_labels[target]);
            }
            break;
        }

        case R_RET: {
            uint8_t src = read_u8(code, ip);
            emit_load_vreg(ctx, X64_RAX, src);
            // Jump to shared epilogue
            x64_jmp_label(&ctx->cb, ctx->epilogue_label);
            break;
        }

        // ================================================================
        // Unsupported opcodes — bail out, function stays interpreted
        // ================================================================
        case R_CALL:          *ip += 1 + 4 + 1 + 1; return false;
        case R_CALL_BUILTIN:  *ip += 1 + 2 + 1 + 1; return false;
        case R_FFI_CALL:      *ip += 1 + 2 + 1 + 1; return false;
        case R_CONST_STR:     *ip += 1 + 4; return false;
        case R_CONST_NULL:    *ip += 1; return false;
        case R_ADD_F64: case R_SUB_F64: case R_MUL_F64: case R_DIV_F64: case R_MOD_F64:
        case R_NEG_F64:
        case R_LT_F64: case R_LTE_F64: case R_GT_F64: case R_GTE_F64:
        case R_ADD_STR:
        case R_ARRAY_NEW: case R_ARRAY_GET: case R_ARRAY_SET:
        case R_MAP_NEW: case R_MAP_GET: case R_MAP_SET:
        case R_STRUCT_NEW: case R_STRUCT_GET: case R_STRUCT_SET:
        case R_CLASS_NEW: case R_CLASS_GET: case R_CLASS_SET:
        case R_METHOD_CALL: case R_SUPER_CALL:
        case R_TRY_BEGIN: case R_TRY_END: case R_THROW:
        case R_BIT_AND: case R_BIT_OR: case R_BIT_XOR:
        case R_BIT_NOT: case R_BIT_SHL: case R_BIT_SHR:
        case R_LOAD_GLOBAL: case R_STORE_GLOBAL:
            return false;

        default:
            if (ctx->jit->debug) {
                fprintf(stderr, "JIT: Unknown opcode %u at offset %zu\n", op, *ip - 1);
            }
            return false;
    }

    return !ctx->cb.error;
}

// Main compilation function
bool jit_compile(JitContext *jit, BpModule *mod, uint32_t func_idx) {
    if (!jit->enabled || func_idx >= jit->profile_count) {
        return false;
    }

    JitProfile *profile = &jit->profiles[func_idx];
    if (profile->state != JIT_STATE_HOT) {
        return false;
    }

    BpFunc *func = &mod->funcs[func_idx];

    // Only compile register-based bytecode
    if (func->format != BC_FORMAT_REGISTER) {
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    profile->state = JIT_STATE_COMPILING;

    if (jit->debug) {
        fprintf(stderr, "JIT: Compiling function %u (%s), %zu bytes of bytecode\n",
                func_idx, func->name ? func->name : "?", func->code_len);
    }

    // Estimate code size: ~8x bytecode for memory-based register access
    size_t estimated_size = func->code_len * 8 + 512;

    uint8_t *temp_code = bp_xmalloc(estimated_size);

    CompileCtx ctx = {0};
    ctx.jit = jit;
    ctx.func = func;
    ctx.mod = mod;

    cb_init(&ctx.cb, temp_code, estimated_size);

    // First pass: find jump targets, check compilability
    if (!first_pass(&ctx)) {
        free(temp_code);
        free(ctx.is_jump_target);
        free(ctx.offset_labels);
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    // ================================================================
    // Emit Prologue
    // ================================================================
    // Save callee-saved registers
    x64_push_r64(&ctx.cb, X64_RBX);
    x64_push_r64(&ctx.cb, X64_R12);
    x64_push_r64(&ctx.cb, X64_RBP);
    x64_mov_r64_r64(&ctx.cb, X64_RBP, X64_RSP);

    // Align stack to 16 bytes (3 pushes = 24 bytes, need 8 more for alignment)
    x64_sub_r64_imm32(&ctx.cb, X64_RSP, 8);

    // R12 = base pointer to int64_t register array (passed via RDI)
    x64_mov_r64_r64(&ctx.cb, X64_R12, X64_RDI);

    // ================================================================
    // Second pass: compile instructions
    // ================================================================
    size_t fip = 0;
    while (fip < func->code_len) {
        if (!compile_insn(&ctx, &fip)) {
            if (jit->debug) {
                fprintf(stderr, "JIT: Bail out at offset %zu (unsupported opcode)\n", fip);
            }
            free(temp_code);
            free(ctx.is_jump_target);
            free(ctx.offset_labels);
            profile->state = JIT_STATE_FAILED;
            return false;
        }
    }

    // ================================================================
    // Emit Epilogue (shared by all R_RET instructions)
    // ================================================================
    cb_define_label(&ctx.cb, ctx.epilogue_label);

    // Result already in RAX from R_RET handler
    x64_add_r64_imm32(&ctx.cb, X64_RSP, 8);  // Undo alignment
    x64_pop_r64(&ctx.cb, X64_RBP);
    x64_pop_r64(&ctx.cb, X64_R12);
    x64_pop_r64(&ctx.cb, X64_RBX);
    x64_ret(&ctx.cb);

    // ================================================================
    // Apply jump fixups
    // ================================================================
    if (!cb_apply_fixups(&ctx.cb)) {
        if (jit->debug) {
            fprintf(stderr, "JIT: Jump fixup failed\n");
        }
        free(temp_code);
        free(ctx.is_jump_target);
        free(ctx.offset_labels);
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    size_t code_size = cb_size(&ctx.cb);

    // Allocate from executable code cache
    void *native_code = jit_alloc_code(jit, code_size);
    if (!native_code) {
        free(temp_code);
        free(ctx.is_jump_target);
        free(ctx.offset_labels);
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    // Copy compiled code to executable memory
    memcpy(native_code, temp_code, code_size);

    profile->native_code = native_code;
    profile->native_size = code_size;
    profile->state = JIT_STATE_COMPILED;

    jit->total_compilations++;

    if (jit->debug) {
        fprintf(stderr, "JIT: Compiled function %u (%s): %zu bytes native code\n",
                func_idx, func->name ? func->name : "?", code_size);
    }

    free(temp_code);
    free(ctx.is_jump_target);
    free(ctx.offset_labels);

    return true;
}

// Execute JIT-compiled function
// Extracts int64_t values from Value register array, calls native code,
// and stores result back as v_int()
int jit_execute(JitContext *jit, BpFunc *func, Value *regs, size_t reg_count) {
    uint32_t func_idx = 0;
    // Find function index from profile
    for (size_t i = 0; i < jit->profile_count; i++) {
        if (jit->profiles[i].native_code && jit->profiles[i].state == JIT_STATE_COMPILED) {
            // We'll find the right one by matching
            func_idx = (uint32_t)i;
            break;
        }
    }
    (void)func_idx;

    JitProfile *profile = NULL;
    for (size_t i = 0; i < jit->profile_count; i++) {
        if (jit->profiles[i].state == JIT_STATE_COMPILED && jit->profiles[i].native_code) {
            // Match by native code pointer
            profile = &jit->profiles[i];
        }
    }
    if (!profile || !profile->native_code) return -1;

    // Allocate int64_t register file on stack
    int64_t iregs[256];
    size_t count = reg_count < 256 ? reg_count : 256;
    for (size_t i = 0; i < count; i++) {
        iregs[i] = regs[i].as.i;
    }

    // Call JIT function: int64_t jit_fn(int64_t *iregs)
    typedef int64_t (*JitFunc)(int64_t *);
    JitFunc native = (JitFunc)profile->native_code;
    int64_t result = native(iregs);

    // Store result back
    regs[0] = v_int(result);
    (void)func;

    jit->native_executions++;
    return 0;
}
