/*
 * BetterPython JIT Compiler
 * Translates register-based bytecode to x86-64 native code
 */

#include "jit.h"
#include "jit_x64.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// External allocation function from jit_profile.c
void *jit_alloc_code(JitContext *jit, size_t size);

// Register mapping: BetterPython virtual registers to x86-64
// r0-r7 map directly to CPU registers for performance
// r8+ are spilled to the stack frame

// Available registers (avoiding RSP, RBP which we use for frame)
// Using callee-saved registers where possible for stability
static const X64Reg reg_map[] = {
    X64_RAX,    // r0 - return value
    X64_RCX,    // r1
    X64_RDX,    // r2
    X64_RBX,    // r3 (callee-saved)
    X64_RSI,    // r4
    X64_RDI,    // r5
    X64_R8,     // r6
    X64_R9,     // r7
    // r8+ are spilled
};
#define NUM_MAPPED_REGS 8

// Compilation context
typedef struct {
    JitContext *jit;
    BpFunc *func;
    BpModule *mod;
    CodeBuffer cb;

    // Label mapping: bytecode offset -> label index
    size_t *offset_labels;
    size_t label_count;

    // Jump target offsets found during first pass
    bool *is_jump_target;
    size_t bytecode_len;

    // Frame layout
    int32_t frame_size;         // Total stack frame size
    int32_t spill_base;         // Offset to spilled registers
} CompileCtx;

// Get x86-64 register for virtual register
static X64Reg get_x64_reg(uint8_t vreg) {
    if (vreg < NUM_MAPPED_REGS) {
        return reg_map[vreg];
    }
    return X64_RAX;  // Spilled registers need load/store
}

// Check if virtual register is spilled
static bool is_spilled(uint8_t vreg) {
    return vreg >= NUM_MAPPED_REGS;
}

// Get stack offset for spilled register
static int32_t spill_offset(CompileCtx *ctx, uint8_t vreg) {
    return ctx->spill_base - (vreg - NUM_MAPPED_REGS + 1) * 8;
}

// Load spilled register into temp register
static void load_spilled(CompileCtx *ctx, X64Reg dst, uint8_t vreg) {
    x64_mov_r64_m64_disp(&ctx->cb, dst, X64_RBP, spill_offset(ctx, vreg));
}

// Store temp register to spilled location
static void store_spilled(CompileCtx *ctx, uint8_t vreg, X64Reg src) {
    x64_mov_m64_disp_r64(&ctx->cb, X64_RBP, spill_offset(ctx, vreg), src);
}

// Emit code to load virtual register into x86-64 register
static void emit_load_vreg(CompileCtx *ctx, X64Reg dst, uint8_t vreg) {
    if (is_spilled(vreg)) {
        load_spilled(ctx, dst, vreg);
    } else if (get_x64_reg(vreg) != dst) {
        x64_mov_r64_r64(&ctx->cb, dst, get_x64_reg(vreg));
    }
}

// Emit code to store x86-64 register to virtual register
static void emit_store_vreg(CompileCtx *ctx, uint8_t vreg, X64Reg src) {
    if (is_spilled(vreg)) {
        store_spilled(ctx, vreg, src);
    } else if (get_x64_reg(vreg) != src) {
        x64_mov_r64_r64(&ctx->cb, get_x64_reg(vreg), src);
    }
}

// Read bytecode values
static uint8_t read_u8(const uint8_t *code, size_t *ip) {
    return code[(*ip)++];
}

static uint16_t read_u16(const uint8_t *code, size_t *ip) {
    uint16_t v = code[*ip] | ((uint16_t)code[*ip + 1] << 8);
    *ip += 2;
    return v;
}

static int16_t read_i16(const uint8_t *code, size_t *ip) {
    return (int16_t)read_u16(code, ip);
}

static int64_t read_i64(const uint8_t *code, size_t *ip) {
    int64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= ((int64_t)code[*ip + i]) << (i * 8);
    }
    *ip += 8;
    return v;
}

// First pass: find jump targets and create labels
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
            // 1-byte opcodes
            case R_RET:
            case R_TRY_END:
                ip += 1;  // src reg
                break;

            // 2-byte opcodes (dst, src)
            case R_MOV:
            case R_NEG_I64:
            case R_NEG_F64:
            case R_NOT:
                ip += 2;
                break;

            // 3-byte opcodes (dst, src1, src2)
            case R_ADD_I64:
            case R_SUB_I64:
            case R_MUL_I64:
            case R_DIV_I64:
            case R_MOD_I64:
            case R_ADD_F64:
            case R_SUB_F64:
            case R_MUL_F64:
            case R_DIV_F64:
            case R_ADD_STR:
            case R_EQ:
            case R_NEQ:
            case R_LT:
            case R_LTE:
            case R_GT:
            case R_GTE:
            case R_LT_F64:
            case R_LTE_F64:
            case R_GT_F64:
            case R_GTE_F64:
            case R_AND:
            case R_OR:
            case R_ARRAY_GET:
            case R_ARRAY_SET:
            case R_MAP_GET:
            case R_MAP_SET:
                ip += 3;
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
                ip += 1 + 4;  // dst + str_idx
                break;

            case R_CONST_NULL:
                ip += 1;  // dst
                break;

            // Jumps - mark targets
            case R_JMP: {
                int16_t offset = read_i16(code, &ip);
                size_t target = ip + offset;
                if (target < len) {
                    ctx->is_jump_target[target] = true;
                }
                break;
            }

            case R_JMP_IF_FALSE:
            case R_JMP_IF_TRUE: {
                ip += 1;  // cond reg
                int16_t offset = read_i16(code, &ip);
                size_t target = ip + offset;
                if (target < len) {
                    ctx->is_jump_target[target] = true;
                }
                break;
            }

            // Function calls
            case R_CALL:
            case R_CALL_BUILTIN:
                ip += 4;  // dst, fn_idx/builtin_id, arg_base, argc
                break;

            // Arrays
            case R_ARRAY_NEW:
                ip += 3;  // dst, src_base, count
                break;

            // Maps
            case R_MAP_NEW:
                ip += 3;  // dst, src_base, count
                break;

            // Exception handling
            case R_TRY_BEGIN:
                ip += 2 + 2 + 1;  // catch_offset, finally_offset, exc_reg
                break;

            case R_THROW:
                ip += 1;  // src
                break;

            // Structs
            case R_STRUCT_NEW:
                ip += 4;  // dst, type_id, src_base, field_count
                break;

            case R_STRUCT_GET:
            case R_STRUCT_SET:
                ip += 3;
                break;

            // Classes
            case R_CLASS_NEW:
                ip += 4;
                break;

            case R_CLASS_GET:
            case R_CLASS_SET:
                ip += 3;
                break;

            case R_METHOD_CALL:
            case R_SUPER_CALL:
                ip += 5;
                break;

            case R_FFI_CALL:
                ip += 4;
                break;

            default:
                fprintf(stderr, "JIT: Unknown opcode %u in first pass\n", op);
                return false;
        }
    }

    // Count and create labels
    ctx->label_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (ctx->is_jump_target[i]) {
            ctx->label_count++;
        }
    }

    ctx->offset_labels = bp_xmalloc(len * sizeof(size_t));
    for (size_t i = 0; i < len; i++) {
        if (ctx->is_jump_target[i]) {
            ctx->offset_labels[i] = cb_new_label(&ctx->cb);
        }
    }

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
        case R_CONST_I64: {
            uint8_t dst = read_u8(code, ip);
            int64_t imm = read_i64(code, ip);
            if (is_spilled(dst)) {
                x64_mov_r64_imm64(&ctx->cb, X64_R10, imm);
                store_spilled(ctx, dst, X64_R10);
            } else {
                x64_mov_r64_imm64(&ctx->cb, get_x64_reg(dst), imm);
            }
            break;
        }

        case R_CONST_BOOL: {
            uint8_t dst = read_u8(code, ip);
            uint8_t val = read_u8(code, ip);
            if (is_spilled(dst)) {
                x64_mov_r64_imm32(&ctx->cb, X64_R10, val ? 1 : 0);
                store_spilled(ctx, dst, X64_R10);
            } else {
                x64_mov_r64_imm32(&ctx->cb, get_x64_reg(dst), val ? 1 : 0);
            }
            break;
        }

        case R_MOV: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);
            emit_load_vreg(ctx, X64_R10, src);
            emit_store_vreg(ctx, dst, X64_R10);
            break;
        }

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
            x64_cqo(&ctx->cb);
            x64_idiv_r64(&ctx->cb, X64_R10);
            emit_store_vreg(ctx, dst, X64_RAX);
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

        case R_EQ: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_sete(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_NEQ: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setne(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_LT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setl(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_LTE: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setle(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_GT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setg(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_GTE: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src1 = read_u8(code, ip);
            uint8_t src2 = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src1);
            emit_load_vreg(ctx, X64_R11, src2);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_cmp_r64_r64(&ctx->cb, X64_R10, X64_R11);
            x64_setge(&ctx->cb, X64_RAX);
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_NOT: {
            uint8_t dst = read_u8(code, ip);
            uint8_t src = read_u8(code, ip);

            emit_load_vreg(ctx, X64_R10, src);
            x64_xor_r64_r64(&ctx->cb, X64_RAX, X64_RAX);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            x64_sete(&ctx->cb, X64_RAX);  // Set to 1 if src was 0
            emit_store_vreg(ctx, dst, X64_RAX);
            break;
        }

        case R_JMP: {
            int16_t offset = read_i16(code, ip);
            size_t target = *ip + offset;
            if (target < ctx->bytecode_len && ctx->is_jump_target[target]) {
                x64_jmp_label(&ctx->cb, ctx->offset_labels[target]);
            }
            break;
        }

        case R_JMP_IF_FALSE: {
            uint8_t cond = read_u8(code, ip);
            int16_t offset = read_i16(code, ip);
            size_t target = *ip + offset;

            emit_load_vreg(ctx, X64_R10, cond);
            x64_test_r64_r64(&ctx->cb, X64_R10, X64_R10);
            if (target < ctx->bytecode_len && ctx->is_jump_target[target]) {
                x64_jz_label(&ctx->cb, ctx->offset_labels[target]);
            }
            break;
        }

        case R_JMP_IF_TRUE: {
            uint8_t cond = read_u8(code, ip);
            int16_t offset = read_i16(code, ip);
            size_t target = *ip + offset;

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

            // Epilogue: restore callee-saved registers and return
            x64_mov_r64_r64(&ctx->cb, X64_RSP, X64_RBP);
            x64_pop_r64(&ctx->cb, X64_RBP);
            x64_pop_r64(&ctx->cb, X64_RBX);
            x64_ret(&ctx->cb);
            break;
        }

        // For now, fall back to interpreter for complex operations
        case R_CALL:
        case R_CALL_BUILTIN:
        case R_ARRAY_NEW:
        case R_ARRAY_GET:
        case R_ARRAY_SET:
        case R_MAP_NEW:
        case R_MAP_GET:
        case R_MAP_SET:
        case R_TRY_BEGIN:
        case R_TRY_END:
        case R_THROW:
        case R_STRUCT_NEW:
        case R_STRUCT_GET:
        case R_STRUCT_SET:
        case R_CLASS_NEW:
        case R_CLASS_GET:
        case R_CLASS_SET:
        case R_METHOD_CALL:
        case R_SUPER_CALL:
        case R_FFI_CALL:
        case R_CONST_F64:
        case R_CONST_STR:
        case R_CONST_NULL:
        case R_ADD_F64:
        case R_SUB_F64:
        case R_MUL_F64:
        case R_DIV_F64:
        case R_NEG_F64:
        case R_ADD_STR:
        case R_LT_F64:
        case R_LTE_F64:
        case R_GT_F64:
        case R_GTE_F64:
        case R_AND:
        case R_OR:
            // These operations require runtime support
            // For now, we don't JIT functions that use them
            return false;

        default:
            fprintf(stderr, "JIT: Unknown opcode %u\n", op);
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
        if (jit->debug) {
            fprintf(stderr, "JIT: Function %u is not register format\n", func_idx);
        }
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    profile->state = JIT_STATE_COMPILING;

    if (jit->debug) {
        fprintf(stderr, "JIT: Compiling function %u (%s)\n", func_idx, func->name);
    }

    // Estimate code size (4x bytecode size is usually enough)
    size_t estimated_size = func->code_len * 4 + 256;  // Extra for prologue/epilogue

    // Allocate temporary buffer
    uint8_t *temp_code = bp_xmalloc(estimated_size);

    CompileCtx ctx = {0};
    ctx.jit = jit;
    ctx.func = func;
    ctx.mod = mod;

    cb_init(&ctx.cb, temp_code, estimated_size);

    // Calculate frame size for spilled registers
    int num_spilled = func->reg_count > NUM_MAPPED_REGS
                          ? func->reg_count - NUM_MAPPED_REGS
                          : 0;
    ctx.frame_size = num_spilled * 8;
    ctx.frame_size = (ctx.frame_size + 15) & ~15;  // Align to 16
    ctx.spill_base = -16;  // After saved RBP

    // First pass: find jump targets
    if (!first_pass(&ctx)) {
        free(temp_code);
        free(ctx.is_jump_target);
        free(ctx.offset_labels);
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    // Emit prologue
    x64_push_r64(&ctx.cb, X64_RBX);  // Save callee-saved
    x64_push_r64(&ctx.cb, X64_RBP);
    x64_mov_r64_r64(&ctx.cb, X64_RBP, X64_RSP);
    if (ctx.frame_size > 0) {
        x64_sub_r64_imm32(&ctx.cb, X64_RSP, ctx.frame_size);
    }

    // Second pass: compile instructions
    size_t ip = 0;
    while (ip < func->code_len) {
        if (!compile_insn(&ctx, &ip)) {
            if (jit->debug) {
                fprintf(stderr, "JIT: Compilation failed at offset %zu\n", ip);
            }
            free(temp_code);
            free(ctx.is_jump_target);
            free(ctx.offset_labels);
            profile->state = JIT_STATE_FAILED;
            return false;
        }
    }

    // Apply jump fixups
    if (!cb_apply_fixups(&ctx.cb)) {
        free(temp_code);
        free(ctx.is_jump_target);
        free(ctx.offset_labels);
        profile->state = JIT_STATE_FAILED;
        return false;
    }

    size_t code_size = cb_size(&ctx.cb);

    // Allocate from code cache
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
        fprintf(stderr, "JIT: Compiled function %u: %zu bytes\n", func_idx, code_size);
    }

    free(temp_code);
    free(ctx.is_jump_target);
    free(ctx.offset_labels);

    return true;
}

// Execute JIT-compiled function
int jit_execute(JitContext *jit, BpFunc *func, Value *regs, size_t reg_count) {
    (void)func;
    (void)regs;
    (void)reg_count;

    // For now, we don't directly execute JIT code
    // The reg_vm will check for JIT code and call it
    jit->native_executions++;
    return 0;
}
