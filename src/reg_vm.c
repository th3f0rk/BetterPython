/*
 * BetterPython Register-Based Virtual Machine
 * Executes register-based bytecode (BPR1 format)
 *
 * Key differences from stack VM:
 * - Values stored in register file (r0-r255) instead of stack
 * - 3-address instructions: dst, src1, src2
 * - Arguments passed in consecutive registers
 * - Return value placed in destination register
 */

#include "reg_vm.h"
#include "stdlib.h"
#include "util.h"
#include "jit/jit.h"
#include <string.h>

#define MAX_CALL_FRAMES 256
#define MAX_TRY_HANDLERS 64

// Use computed goto for GCC/Clang (faster than switch)
#if defined(__GNUC__) || defined(__clang__)
#define USE_COMPUTED_GOTO 1
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define USE_COMPUTED_GOTO 0
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// Register call frame
typedef struct {
    BpFunc *fn;
    const uint8_t *code;
    size_t ip;
    size_t reg_base;        // Base index into register file
    uint8_t return_reg;     // Register to store return value in caller
} RegCallFrame;

// Exception handler for try/catch
typedef struct {
    uint32_t catch_addr;
    uint32_t finally_addr;
    uint8_t exc_reg;
    size_t frame_idx;
    size_t reg_base;
} RegTryHandler;

// Read helpers
static uint16_t rd_u16(const uint8_t *code, size_t *ip) {
    uint16_t x;
    memcpy(&x, code + *ip, 2);
    *ip += 2;
    return x;
}
static uint32_t rd_u32(const uint8_t *code, size_t *ip) {
    uint32_t x;
    memcpy(&x, code + *ip, 4);
    *ip += 4;
    return x;
}
static int64_t rd_i64(const uint8_t *code, size_t *ip) {
    int64_t x;
    memcpy(&x, code + *ip, 8);
    *ip += 8;
    return x;
}
static double rd_f64(const uint8_t *code, size_t *ip) {
    double x;
    memcpy(&x, code + *ip, 8);
    *ip += 8;
    return x;
}

// String concatenation helper
static Value add_str(Vm *vm, Value a, Value b) {
    if (a.type != VAL_STR || b.type != VAL_STR) bp_fatal("string add expects str");
    size_t n = a.as.s->len + b.as.s->len;
    char *buf = bp_xmalloc(n + 1);
    memcpy(buf, a.as.s->data, a.as.s->len);
    memcpy(buf + a.as.s->len, b.as.s->data, b.as.s->len);
    buf[n] = '\0';
    BpStr *s = gc_new_str(&vm->gc, buf, n);
    free(buf);
    return v_str(s);
}

// Value comparison
static Value op_eq(Value a, Value b) {
    if (a.type != b.type) return v_bool(false);
    switch (a.type) {
        case VAL_INT: return v_bool(a.as.i == b.as.i);
        case VAL_FLOAT: return v_bool(a.as.f == b.as.f);
        case VAL_BOOL: return v_bool(a.as.b == b.as.b);
        case VAL_NULL: return v_bool(true);
        case VAL_STR:
            if (a.as.s->len != b.as.s->len) return v_bool(false);
            return v_bool(memcmp(a.as.s->data, b.as.s->data, a.as.s->len) == 0);
        default: break;
    }
    return v_bool(false);
}

int reg_vm_run(Vm *vm) {
    if (vm->mod.entry >= vm->mod.fn_len) bp_fatal("no entry function");

    BpFunc *entry_fn = &vm->mod.funcs[vm->mod.entry];

    // Verify this is register-based bytecode
    if (entry_fn->format != BC_FORMAT_REGISTER) {
        bp_fatal("reg_vm_run called on non-register bytecode");
    }

    // Call frame stack
    RegCallFrame frames[MAX_CALL_FRAMES];
    size_t frame_count = 0;

    // Exception handler stack
    RegTryHandler try_handlers[MAX_TRY_HANDLERS];
    size_t try_count = 0;

    // Register file - one contiguous array for all frames
    // Each function's registers start at reg_base
    size_t total_regs_cap = 4096;
    Value *regs = bp_xmalloc(total_regs_cap * sizeof(Value));
    for (size_t i = 0; i < total_regs_cap; i++) regs[i] = v_null();

    // Set up initial frame for main()
    BpFunc *fn = entry_fn;
    frames[frame_count].fn = fn;
    frames[frame_count].code = fn->code;
    frames[frame_count].ip = 0;
    frames[frame_count].reg_base = 0;
    frames[frame_count].return_reg = 0;
    frame_count++;

    const uint8_t *code = fn->code;
    size_t ip = 0;
    size_t reg_base = 0;
    size_t regs_top = fn->reg_count;  // Next available register slot

    // Register access macros
    #define REG(n) regs[reg_base + (n)]

#if USE_COMPUTED_GOTO
    // Dispatch table for register opcodes
    static const void *dispatch_table[256] = {
        [R_CONST_I64] = &&L_R_CONST_I64,
        [R_CONST_F64] = &&L_R_CONST_F64,
        [R_CONST_BOOL] = &&L_R_CONST_BOOL,
        [R_CONST_STR] = &&L_R_CONST_STR,
        [R_CONST_NULL] = &&L_R_CONST_NULL,
        [R_MOV] = &&L_R_MOV,
        [R_ADD_I64] = &&L_R_ADD_I64,
        [R_SUB_I64] = &&L_R_SUB_I64,
        [R_MUL_I64] = &&L_R_MUL_I64,
        [R_DIV_I64] = &&L_R_DIV_I64,
        [R_MOD_I64] = &&L_R_MOD_I64,
        [R_NEG_I64] = &&L_R_NEG_I64,
        [R_ADD_F64] = &&L_R_ADD_F64,
        [R_SUB_F64] = &&L_R_SUB_F64,
        [R_MUL_F64] = &&L_R_MUL_F64,
        [R_DIV_F64] = &&L_R_DIV_F64,
        [R_NEG_F64] = &&L_R_NEG_F64,
        [R_ADD_STR] = &&L_R_ADD_STR,
        [R_EQ] = &&L_R_EQ,
        [R_NEQ] = &&L_R_NEQ,
        [R_LT] = &&L_R_LT,
        [R_LTE] = &&L_R_LTE,
        [R_GT] = &&L_R_GT,
        [R_GTE] = &&L_R_GTE,
        [R_LT_F64] = &&L_R_LT_F64,
        [R_LTE_F64] = &&L_R_LTE_F64,
        [R_GT_F64] = &&L_R_GT_F64,
        [R_GTE_F64] = &&L_R_GTE_F64,
        [R_NOT] = &&L_R_NOT,
        [R_AND] = &&L_R_AND,
        [R_OR] = &&L_R_OR,
        [R_JMP] = &&L_R_JMP,
        [R_JMP_IF_FALSE] = &&L_R_JMP_IF_FALSE,
        [R_JMP_IF_TRUE] = &&L_R_JMP_IF_TRUE,
        [R_CALL] = &&L_R_CALL,
        [R_CALL_BUILTIN] = &&L_R_CALL_BUILTIN,
        [R_RET] = &&L_R_RET,
        [R_ARRAY_NEW] = &&L_R_ARRAY_NEW,
        [R_ARRAY_GET] = &&L_R_ARRAY_GET,
        [R_ARRAY_SET] = &&L_R_ARRAY_SET,
        [R_MAP_NEW] = &&L_R_MAP_NEW,
        [R_MAP_GET] = &&L_R_MAP_GET,
        [R_MAP_SET] = &&L_R_MAP_SET,
        [R_TRY_BEGIN] = &&L_R_TRY_BEGIN,
        [R_TRY_END] = &&L_R_TRY_END,
        [R_THROW] = &&L_R_THROW,
        [R_STRUCT_NEW] = &&L_R_STRUCT_NEW,
        [R_STRUCT_GET] = &&L_R_STRUCT_GET,
        [R_STRUCT_SET] = &&L_R_STRUCT_SET,
        [R_CLASS_NEW] = &&L_R_CLASS_NEW,
        [R_CLASS_GET] = &&L_R_CLASS_GET,
        [R_CLASS_SET] = &&L_R_CLASS_SET,
        [R_METHOD_CALL] = &&L_R_METHOD_CALL,
        [R_SUPER_CALL] = &&L_R_SUPER_CALL,
        [R_FFI_CALL] = &&L_R_FFI_CALL,
    };

    #define VM_DISPATCH() do { \
        if (UNLIKELY(vm->gc.bytes > vm->gc.next_gc)) gc_collect(&vm->gc, regs, regs_top, NULL, 0); \
        if (UNLIKELY(vm->exiting)) goto vm_exit; \
        if (UNLIKELY(ip >= fn->code_len)) goto vm_end_of_function; \
        { \
            uint8_t _op = code[ip++]; \
            const void *_target = dispatch_table[_op]; \
            if (UNLIKELY(!_target)) goto vm_unknown; \
            goto *_target; \
        } \
    } while(0)

    VM_DISPATCH();

vm_unknown:
    bp_fatal("unknown register opcode %u", code[ip-1]);

vm_end_of_function:
    if (frame_count == 1) goto vm_exit;
    frame_count--;
    fn = frames[frame_count - 1].fn;
    code = frames[frame_count - 1].code;
    ip = frames[frame_count - 1].ip;
    reg_base = frames[frame_count - 1].reg_base;
    regs_top = reg_base + fn->reg_count;
    VM_DISPATCH();

L_R_CONST_I64: {
    uint8_t dst = code[ip++];
    int64_t val = rd_i64(code, &ip);
    REG(dst) = v_int(val);
    VM_DISPATCH();
}
L_R_CONST_F64: {
    uint8_t dst = code[ip++];
    double val = rd_f64(code, &ip);
    REG(dst) = v_float(val);
    VM_DISPATCH();
}
L_R_CONST_BOOL: {
    uint8_t dst = code[ip++];
    uint8_t val = code[ip++];
    REG(dst) = v_bool(val != 0);
    VM_DISPATCH();
}
L_R_CONST_STR: {
    uint8_t dst = code[ip++];
    uint32_t local_id = rd_u32(code, &ip);
    if (local_id >= fn->str_const_len) bp_fatal("bad str const");
    uint32_t pool_id = fn->str_const_ids[local_id];
    if (pool_id >= vm->mod.str_len) bp_fatal("bad str pool id");
    const char *s = vm->mod.strings[pool_id];
    BpStr *o = gc_new_str(&vm->gc, s, strlen(s));
    REG(dst) = v_str(o);
    VM_DISPATCH();
}
L_R_CONST_NULL: {
    uint8_t dst = code[ip++];
    REG(dst) = v_null();
    VM_DISPATCH();
}
L_R_MOV: {
    uint8_t dst = code[ip++];
    uint8_t src = code[ip++];
    REG(dst) = REG(src);
    VM_DISPATCH();
}
L_R_ADD_I64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_int(REG(src1).as.i + REG(src2).as.i);
    VM_DISPATCH();
}
L_R_SUB_I64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_int(REG(src1).as.i - REG(src2).as.i);
    VM_DISPATCH();
}
L_R_MUL_I64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_int(REG(src1).as.i * REG(src2).as.i);
    VM_DISPATCH();
}
L_R_DIV_I64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_int(REG(src1).as.i / REG(src2).as.i);
    VM_DISPATCH();
}
L_R_MOD_I64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_int(REG(src1).as.i % REG(src2).as.i);
    VM_DISPATCH();
}
L_R_NEG_I64: {
    uint8_t dst = code[ip++];
    uint8_t src = code[ip++];
    REG(dst) = v_int(-REG(src).as.i);
    VM_DISPATCH();
}
L_R_ADD_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_float(REG(src1).as.f + REG(src2).as.f);
    VM_DISPATCH();
}
L_R_SUB_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_float(REG(src1).as.f - REG(src2).as.f);
    VM_DISPATCH();
}
L_R_MUL_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_float(REG(src1).as.f * REG(src2).as.f);
    VM_DISPATCH();
}
L_R_DIV_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_float(REG(src1).as.f / REG(src2).as.f);
    VM_DISPATCH();
}
L_R_NEG_F64: {
    uint8_t dst = code[ip++];
    uint8_t src = code[ip++];
    REG(dst) = v_float(-REG(src).as.f);
    VM_DISPATCH();
}
L_R_ADD_STR: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = add_str(vm, REG(src1), REG(src2));
    VM_DISPATCH();
}
L_R_EQ: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = op_eq(REG(src1), REG(src2));
    VM_DISPATCH();
}
L_R_NEQ: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    Value eq = op_eq(REG(src1), REG(src2));
    REG(dst) = v_bool(!eq.as.b);
    VM_DISPATCH();
}
L_R_LT: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.i < REG(src2).as.i);
    VM_DISPATCH();
}
L_R_LTE: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.i <= REG(src2).as.i);
    VM_DISPATCH();
}
L_R_GT: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.i > REG(src2).as.i);
    VM_DISPATCH();
}
L_R_GTE: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.i >= REG(src2).as.i);
    VM_DISPATCH();
}
L_R_LT_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.f < REG(src2).as.f);
    VM_DISPATCH();
}
L_R_LTE_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.f <= REG(src2).as.f);
    VM_DISPATCH();
}
L_R_GT_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.f > REG(src2).as.f);
    VM_DISPATCH();
}
L_R_GTE_F64: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(REG(src1).as.f >= REG(src2).as.f);
    VM_DISPATCH();
}
L_R_NOT: {
    uint8_t dst = code[ip++];
    uint8_t src = code[ip++];
    REG(dst) = v_bool(!v_is_truthy(REG(src)));
    VM_DISPATCH();
}
L_R_AND: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(v_is_truthy(REG(src1)) && v_is_truthy(REG(src2)));
    VM_DISPATCH();
}
L_R_OR: {
    uint8_t dst = code[ip++];
    uint8_t src1 = code[ip++];
    uint8_t src2 = code[ip++];
    REG(dst) = v_bool(v_is_truthy(REG(src1)) || v_is_truthy(REG(src2)));
    VM_DISPATCH();
}
L_R_JMP: {
    uint32_t tgt = rd_u32(code, &ip);
    ip = tgt;
    VM_DISPATCH();
}
L_R_JMP_IF_FALSE: {
    uint8_t cond = code[ip++];
    uint32_t tgt = rd_u32(code, &ip);
    if (!v_is_truthy(REG(cond))) ip = tgt;
    VM_DISPATCH();
}
L_R_JMP_IF_TRUE: {
    uint8_t cond = code[ip++];
    uint32_t tgt = rd_u32(code, &ip);
    if (v_is_truthy(REG(cond))) ip = tgt;
    VM_DISPATCH();
}
L_R_CALL: {
    uint8_t dst = code[ip++];
    uint32_t fn_idx = rd_u32(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];

    if (UNLIKELY(fn_idx >= vm->mod.fn_len)) bp_fatal("bad function index");
    BpFunc *callee = &vm->mod.funcs[fn_idx];

    if (UNLIKELY(frame_count >= MAX_CALL_FRAMES)) bp_fatal("call stack overflow");

    // JIT profiling and compilation
    if (vm->jit) {
        jit_record_call(vm->jit, fn_idx);

        // Try to compile hot functions
        if (jit_should_compile(vm->jit, fn_idx)) {
            jit_compile(vm->jit, &vm->mod, fn_idx);
        }

        // Execute native code if available
        if (jit_has_native(vm->jit, fn_idx)) {
            // Set up arguments in temporary register space
            size_t new_reg_base = regs_top;
            size_t needed = new_reg_base + callee->reg_count;
            if (UNLIKELY(needed > total_regs_cap)) {
                while (needed > total_regs_cap) total_regs_cap *= 2;
                regs = bp_xrealloc(regs, total_regs_cap * sizeof(Value));
                for (size_t i = regs_top; i < total_regs_cap; i++) regs[i] = v_null();
            }

            // Copy arguments
            for (uint8_t i = 0; i < argc; i++) {
                regs[new_reg_base + i] = REG(arg_base + i);
            }

            // Get native function pointer and call it
            typedef int64_t (*NativeFunc)(void);
            NativeFunc native = (NativeFunc)jit_get_native(vm->jit, fn_idx);
            int64_t result = native();

            // Store result in destination register
            REG(dst) = v_int(result);
            vm->jit->native_executions++;
            VM_DISPATCH();
        }
        vm->jit->interp_executions++;
    }

    // Save return info
    frames[frame_count - 1].ip = ip;

    // Set up new frame
    size_t new_reg_base = regs_top;
    size_t needed = new_reg_base + callee->reg_count;
    if (UNLIKELY(needed > total_regs_cap)) {
        while (needed > total_regs_cap) total_regs_cap *= 2;
        regs = bp_xrealloc(regs, total_regs_cap * sizeof(Value));
        for (size_t i = regs_top; i < total_regs_cap; i++) regs[i] = v_null();
    }

    // Copy arguments to new frame (parameters are in r0..r(argc-1))
    for (uint8_t i = 0; i < argc; i++) {
        regs[new_reg_base + i] = REG(arg_base + i);
    }
    // Initialize remaining registers to null
    for (size_t i = argc; i < callee->reg_count; i++) {
        regs[new_reg_base + i] = v_null();
    }

    // Push new frame
    frames[frame_count].fn = callee;
    frames[frame_count].code = callee->code;
    frames[frame_count].ip = 0;
    frames[frame_count].reg_base = new_reg_base;
    frames[frame_count].return_reg = dst;
    frame_count++;

    // Switch to new function
    fn = callee;
    code = callee->code;
    ip = 0;
    reg_base = new_reg_base;
    regs_top = new_reg_base + callee->reg_count;
    VM_DISPATCH();
}
L_R_CALL_BUILTIN: {
    uint8_t dst = code[ip++];
    uint16_t id = rd_u16(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];

    // Build argument array from registers
    Value args[256];
    for (uint8_t i = 0; i < argc; i++) {
        args[i] = REG(arg_base + i);
    }

    Value result = stdlib_call((BuiltinId)id, args, argc, &vm->gc, &vm->exit_code, &vm->exiting);
    REG(dst) = result;
    VM_DISPATCH();
}
L_R_RET: {
    uint8_t src = code[ip++];
    Value rv = REG(src);

    if (frame_count == 1) {
        // Returning from main
        free(regs);
        if (vm->exiting) return vm->exit_code;
        return (int)rv.as.i;
    }

    // Pop frame and return to caller
    frame_count--;
    uint8_t return_reg = frames[frame_count].return_reg;
    fn = frames[frame_count - 1].fn;
    code = frames[frame_count - 1].code;
    ip = frames[frame_count - 1].ip;
    reg_base = frames[frame_count - 1].reg_base;
    regs_top = reg_base + fn->reg_count;

    // Store return value in caller's destination register
    REG(return_reg) = rv;
    VM_DISPATCH();
}
L_R_ARRAY_NEW: {
    uint8_t dst = code[ip++];
    uint8_t src_base = code[ip++];
    uint16_t count = rd_u16(code, &ip);
    BpArray *arr = gc_new_array(&vm->gc, count > 0 ? count : 8);
    for (uint16_t i = 0; i < count; i++) {
        gc_array_push(&vm->gc, arr, REG(src_base + i));
    }
    REG(dst) = v_array(arr);
    VM_DISPATCH();
}
L_R_ARRAY_GET: {
    uint8_t dst = code[ip++];
    uint8_t arr_reg = code[ip++];
    uint8_t idx_reg = code[ip++];
    Value arr_val = REG(arr_reg);
    Value idx_val = REG(idx_reg);
    if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
    if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
    int64_t idx = idx_val.as.i;
    if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;
    if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
        bp_fatal("array index out of bounds");
    }
    REG(dst) = gc_array_get(arr_val.as.arr, (size_t)idx);
    VM_DISPATCH();
}
L_R_ARRAY_SET: {
    uint8_t arr_reg = code[ip++];
    uint8_t idx_reg = code[ip++];
    uint8_t val_reg = code[ip++];
    Value arr_val = REG(arr_reg);
    Value idx_val = REG(idx_reg);
    if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
    if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
    int64_t idx = idx_val.as.i;
    if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;
    if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
        bp_fatal("array index out of bounds");
    }
    gc_array_set(arr_val.as.arr, (size_t)idx, REG(val_reg));
    VM_DISPATCH();
}
L_R_MAP_NEW: {
    uint8_t dst = code[ip++];
    uint8_t src_base = code[ip++];
    uint16_t count = rd_u16(code, &ip);
    BpMap *map = gc_new_map(&vm->gc, count > 0 ? count * 2 : 16);
    for (uint16_t i = 0; i < count; i++) {
        Value key = REG(src_base + i * 2);
        Value val = REG(src_base + i * 2 + 1);
        gc_map_set(&vm->gc, map, key, val);
    }
    REG(dst) = v_map(map);
    VM_DISPATCH();
}
L_R_MAP_GET: {
    uint8_t dst = code[ip++];
    uint8_t map_reg = code[ip++];
    uint8_t key_reg = code[ip++];
    Value map_val = REG(map_reg);
    if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
    bool found;
    Value result = gc_map_get(map_val.as.map, REG(key_reg), &found);
    if (!found) bp_fatal("key not found in map");
    REG(dst) = result;
    VM_DISPATCH();
}
L_R_MAP_SET: {
    uint8_t map_reg = code[ip++];
    uint8_t key_reg = code[ip++];
    uint8_t val_reg = code[ip++];
    Value map_val = REG(map_reg);
    if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
    gc_map_set(&vm->gc, map_val.as.map, REG(key_reg), REG(val_reg));
    VM_DISPATCH();
}
L_R_TRY_BEGIN: {
    if (try_count >= MAX_TRY_HANDLERS) bp_fatal("too many nested try blocks");
    uint32_t catch_addr = rd_u32(code, &ip);
    uint32_t finally_addr = rd_u32(code, &ip);
    uint8_t exc_reg = code[ip++];
    try_handlers[try_count].catch_addr = catch_addr;
    try_handlers[try_count].finally_addr = finally_addr;
    try_handlers[try_count].exc_reg = exc_reg;
    try_handlers[try_count].frame_idx = frame_count - 1;
    try_handlers[try_count].reg_base = reg_base;
    try_count++;
    VM_DISPATCH();
}
L_R_TRY_END: {
    if (try_count > 0) try_count--;
    VM_DISPATCH();
}
L_R_THROW: {
    uint8_t src = code[ip++];
    Value exc_val = REG(src);
    if (try_count == 0) {
        if (exc_val.type == VAL_STR) {
            bp_fatal("unhandled exception: %s", exc_val.as.s->data);
        } else {
            bp_fatal("unhandled exception");
        }
    }
    try_count--;
    RegTryHandler *h = &try_handlers[try_count];
    frame_count = h->frame_idx + 1;
    fn = frames[frame_count - 1].fn;
    code = frames[frame_count - 1].code;
    reg_base = h->reg_base;
    regs_top = reg_base + fn->reg_count;
    REG(h->exc_reg) = exc_val;
    ip = h->catch_addr;
    VM_DISPATCH();
}
L_R_STRUCT_NEW: {
    uint8_t dst = code[ip++];
    uint16_t type_id = rd_u16(code, &ip);
    uint8_t src_base = code[ip++];
    uint8_t field_count = code[ip++];
    BpStruct *st = gc_new_struct(&vm->gc, type_id, field_count);
    for (uint8_t i = 0; i < field_count; i++) {
        st->fields[i] = REG(src_base + i);
    }
    REG(dst) = v_struct(st);
    VM_DISPATCH();
}
L_R_STRUCT_GET: {
    uint8_t dst = code[ip++];
    uint8_t struct_reg = code[ip++];
    uint16_t field_idx = rd_u16(code, &ip);
    Value st_val = REG(struct_reg);
    if (st_val.type != VAL_STRUCT) bp_fatal("cannot access field on non-struct");
    if (field_idx >= st_val.as.st->field_count) {
        bp_fatal("struct field index out of bounds");
    }
    REG(dst) = gc_struct_get(st_val.as.st, field_idx);
    VM_DISPATCH();
}
L_R_STRUCT_SET: {
    uint8_t struct_reg = code[ip++];
    uint16_t field_idx = rd_u16(code, &ip);
    uint8_t val_reg = code[ip++];
    Value st_val = REG(struct_reg);
    if (st_val.type != VAL_STRUCT) bp_fatal("cannot access field on non-struct");
    if (field_idx >= st_val.as.st->field_count) {
        bp_fatal("struct field index out of bounds");
    }
    gc_struct_set(st_val.as.st, field_idx, REG(val_reg));
    VM_DISPATCH();
}
L_R_CLASS_NEW: {
    uint8_t dst = code[ip++];
    uint16_t class_id = rd_u16(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];
    BP_UNUSED(arg_base);
    BP_UNUSED(argc);
    size_t field_count = 0;
    if (class_id < vm->mod.class_type_len) {
        field_count = vm->mod.class_types[class_id].field_count;
    }
    BpClass *cls = gc_new_class(&vm->gc, class_id, field_count);
    REG(dst) = v_class(cls);
    VM_DISPATCH();
}
L_R_CLASS_GET: {
    uint8_t dst = code[ip++];
    uint8_t obj_reg = code[ip++];
    uint16_t field_idx = rd_u16(code, &ip);
    Value cls_val = REG(obj_reg);
    if (cls_val.type != VAL_CLASS) bp_fatal("cannot access field on non-class");
    if (field_idx >= cls_val.as.cls->field_count) {
        bp_fatal("class field index out of bounds");
    }
    REG(dst) = gc_class_get(cls_val.as.cls, field_idx);
    VM_DISPATCH();
}
L_R_CLASS_SET: {
    uint8_t obj_reg = code[ip++];
    uint16_t field_idx = rd_u16(code, &ip);
    uint8_t val_reg = code[ip++];
    Value cls_val = REG(obj_reg);
    if (cls_val.type != VAL_CLASS) bp_fatal("cannot access field on non-class");
    if (field_idx >= cls_val.as.cls->field_count) {
        bp_fatal("class field index out of bounds");
    }
    gc_class_set(cls_val.as.cls, field_idx, REG(val_reg));
    VM_DISPATCH();
}
L_R_METHOD_CALL: {
    uint8_t dst = code[ip++];
    uint8_t obj_reg = code[ip++];
    uint16_t method_id = rd_u16(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];
    BP_UNUSED(obj_reg);
    BP_UNUSED(method_id);
    BP_UNUSED(arg_base);
    BP_UNUSED(argc);
    REG(dst) = v_null();
    VM_DISPATCH();
}
L_R_SUPER_CALL: {
    uint8_t dst = code[ip++];
    uint16_t method_id = rd_u16(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];
    BP_UNUSED(method_id);
    BP_UNUSED(arg_base);
    BP_UNUSED(argc);
    REG(dst) = v_null();
    VM_DISPATCH();
}
L_R_FFI_CALL: {
    uint8_t dst = code[ip++];
    uint16_t extern_id = rd_u16(code, &ip);
    uint8_t arg_base = code[ip++];
    uint8_t argc = code[ip++];
    BP_UNUSED(extern_id);
    BP_UNUSED(arg_base);
    BP_UNUSED(argc);
    REG(dst) = v_null();
    VM_DISPATCH();
}

vm_exit:
    free(regs);
    return vm->exiting ? vm->exit_code : 0;

#else
    // Standard switch-based dispatch (fallback for non-GCC/Clang)
    while (ip < fn->code_len || frame_count > 0) {
        if (ip >= fn->code_len) {
            if (frame_count == 1) break;
            frame_count--;
            fn = frames[frame_count - 1].fn;
            code = frames[frame_count - 1].code;
            ip = frames[frame_count - 1].ip;
            reg_base = frames[frame_count - 1].reg_base;
            regs_top = reg_base + fn->reg_count;
            continue;
        }

        uint8_t op = code[ip++];
        switch (op) {
            case R_CONST_I64: {
                uint8_t dst = code[ip++];
                int64_t val = rd_i64(code, &ip);
                REG(dst) = v_int(val);
                break;
            }
            case R_CONST_F64: {
                uint8_t dst = code[ip++];
                double val = rd_f64(code, &ip);
                REG(dst) = v_float(val);
                break;
            }
            case R_CONST_BOOL: {
                uint8_t dst = code[ip++];
                uint8_t val = code[ip++];
                REG(dst) = v_bool(val != 0);
                break;
            }
            case R_CONST_STR: {
                uint8_t dst = code[ip++];
                uint32_t local_id = rd_u32(code, &ip);
                if (local_id >= fn->str_const_len) bp_fatal("bad str const");
                uint32_t pool_id = fn->str_const_ids[local_id];
                if (pool_id >= vm->mod.str_len) bp_fatal("bad str pool id");
                const char *s = vm->mod.strings[pool_id];
                BpStr *o = gc_new_str(&vm->gc, s, strlen(s));
                REG(dst) = v_str(o);
                break;
            }
            case R_CONST_NULL: {
                uint8_t dst = code[ip++];
                REG(dst) = v_null();
                break;
            }
            case R_MOV: {
                uint8_t dst = code[ip++];
                uint8_t src = code[ip++];
                REG(dst) = REG(src);
                break;
            }
            case R_ADD_I64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_int(REG(src1).as.i + REG(src2).as.i);
                break;
            }
            case R_SUB_I64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_int(REG(src1).as.i - REG(src2).as.i);
                break;
            }
            case R_MUL_I64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_int(REG(src1).as.i * REG(src2).as.i);
                break;
            }
            case R_DIV_I64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_int(REG(src1).as.i / REG(src2).as.i);
                break;
            }
            case R_MOD_I64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_int(REG(src1).as.i % REG(src2).as.i);
                break;
            }
            case R_NEG_I64: {
                uint8_t dst = code[ip++];
                uint8_t src = code[ip++];
                REG(dst) = v_int(-REG(src).as.i);
                break;
            }
            case R_ADD_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_float(REG(src1).as.f + REG(src2).as.f);
                break;
            }
            case R_SUB_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_float(REG(src1).as.f - REG(src2).as.f);
                break;
            }
            case R_MUL_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_float(REG(src1).as.f * REG(src2).as.f);
                break;
            }
            case R_DIV_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_float(REG(src1).as.f / REG(src2).as.f);
                break;
            }
            case R_NEG_F64: {
                uint8_t dst = code[ip++];
                uint8_t src = code[ip++];
                REG(dst) = v_float(-REG(src).as.f);
                break;
            }
            case R_ADD_STR: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = add_str(vm, REG(src1), REG(src2));
                break;
            }
            case R_EQ: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = op_eq(REG(src1), REG(src2));
                break;
            }
            case R_NEQ: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                Value eq = op_eq(REG(src1), REG(src2));
                REG(dst) = v_bool(!eq.as.b);
                break;
            }
            case R_LT: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.i < REG(src2).as.i);
                break;
            }
            case R_LTE: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.i <= REG(src2).as.i);
                break;
            }
            case R_GT: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.i > REG(src2).as.i);
                break;
            }
            case R_GTE: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.i >= REG(src2).as.i);
                break;
            }
            case R_LT_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.f < REG(src2).as.f);
                break;
            }
            case R_LTE_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.f <= REG(src2).as.f);
                break;
            }
            case R_GT_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.f > REG(src2).as.f);
                break;
            }
            case R_GTE_F64: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(REG(src1).as.f >= REG(src2).as.f);
                break;
            }
            case R_NOT: {
                uint8_t dst = code[ip++];
                uint8_t src = code[ip++];
                REG(dst) = v_bool(!v_is_truthy(REG(src)));
                break;
            }
            case R_AND: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(v_is_truthy(REG(src1)) && v_is_truthy(REG(src2)));
                break;
            }
            case R_OR: {
                uint8_t dst = code[ip++];
                uint8_t src1 = code[ip++];
                uint8_t src2 = code[ip++];
                REG(dst) = v_bool(v_is_truthy(REG(src1)) || v_is_truthy(REG(src2)));
                break;
            }
            case R_JMP: {
                uint32_t tgt = rd_u32(code, &ip);
                ip = tgt;
                break;
            }
            case R_JMP_IF_FALSE: {
                uint8_t cond = code[ip++];
                uint32_t tgt = rd_u32(code, &ip);
                if (!v_is_truthy(REG(cond))) ip = tgt;
                break;
            }
            case R_JMP_IF_TRUE: {
                uint8_t cond = code[ip++];
                uint32_t tgt = rd_u32(code, &ip);
                if (v_is_truthy(REG(cond))) ip = tgt;
                break;
            }
            case R_CALL: {
                uint8_t dst = code[ip++];
                uint32_t fn_idx = rd_u32(code, &ip);
                uint8_t arg_base = code[ip++];
                uint8_t argc = code[ip++];

                if (fn_idx >= vm->mod.fn_len) bp_fatal("bad function index");
                BpFunc *callee = &vm->mod.funcs[fn_idx];

                if (frame_count >= MAX_CALL_FRAMES) bp_fatal("call stack overflow");

                // JIT profiling and compilation
                if (vm->jit) {
                    jit_record_call(vm->jit, fn_idx);

                    // Try to compile hot functions
                    if (jit_should_compile(vm->jit, fn_idx)) {
                        jit_compile(vm->jit, &vm->mod, fn_idx);
                    }

                    // Execute native code if available
                    if (jit_has_native(vm->jit, fn_idx)) {
                        size_t new_reg_base = regs_top;
                        size_t needed = new_reg_base + callee->reg_count;
                        if (needed > total_regs_cap) {
                            while (needed > total_regs_cap) total_regs_cap *= 2;
                            regs = bp_xrealloc(regs, total_regs_cap * sizeof(Value));
                            for (size_t i = regs_top; i < total_regs_cap; i++) regs[i] = v_null();
                        }

                        for (uint8_t i = 0; i < argc; i++) {
                            regs[new_reg_base + i] = REG(arg_base + i);
                        }

                        typedef int64_t (*NativeFunc)(void);
                        NativeFunc native = (NativeFunc)jit_get_native(vm->jit, fn_idx);
                        int64_t result = native();

                        REG(dst) = v_int(result);
                        vm->jit->native_executions++;
                        break;
                    }
                    vm->jit->interp_executions++;
                }

                frames[frame_count - 1].ip = ip;

                size_t new_reg_base = regs_top;
                size_t needed = new_reg_base + callee->reg_count;
                if (needed > total_regs_cap) {
                    while (needed > total_regs_cap) total_regs_cap *= 2;
                    regs = bp_xrealloc(regs, total_regs_cap * sizeof(Value));
                    for (size_t i = regs_top; i < total_regs_cap; i++) regs[i] = v_null();
                }

                for (uint8_t i = 0; i < argc; i++) {
                    regs[new_reg_base + i] = REG(arg_base + i);
                }
                for (size_t i = argc; i < callee->reg_count; i++) {
                    regs[new_reg_base + i] = v_null();
                }

                frames[frame_count].fn = callee;
                frames[frame_count].code = callee->code;
                frames[frame_count].ip = 0;
                frames[frame_count].reg_base = new_reg_base;
                frames[frame_count].return_reg = dst;
                frame_count++;

                fn = callee;
                code = callee->code;
                ip = 0;
                reg_base = new_reg_base;
                regs_top = new_reg_base + callee->reg_count;
                break;
            }
            case R_CALL_BUILTIN: {
                uint8_t dst = code[ip++];
                uint16_t id = rd_u16(code, &ip);
                uint8_t arg_base = code[ip++];
                uint8_t argc = code[ip++];

                Value args[256];
                for (uint8_t i = 0; i < argc; i++) {
                    args[i] = REG(arg_base + i);
                }

                Value result = stdlib_call((BuiltinId)id, args, argc, &vm->gc, &vm->exit_code, &vm->exiting);
                REG(dst) = result;
                break;
            }
            case R_RET: {
                uint8_t src = code[ip++];
                Value rv = REG(src);

                if (frame_count == 1) {
                    free(regs);
                    if (vm->exiting) return vm->exit_code;
                    return (int)rv.as.i;
                }

                frame_count--;
                uint8_t return_reg = frames[frame_count].return_reg;
                fn = frames[frame_count - 1].fn;
                code = frames[frame_count - 1].code;
                ip = frames[frame_count - 1].ip;
                reg_base = frames[frame_count - 1].reg_base;
                regs_top = reg_base + fn->reg_count;

                REG(return_reg) = rv;
                break;
            }
            case R_ARRAY_NEW: {
                uint8_t dst = code[ip++];
                uint8_t src_base = code[ip++];
                uint16_t count = rd_u16(code, &ip);
                BpArray *arr = gc_new_array(&vm->gc, count > 0 ? count : 8);
                for (uint16_t i = 0; i < count; i++) {
                    gc_array_push(&vm->gc, arr, REG(src_base + i));
                }
                REG(dst) = v_array(arr);
                break;
            }
            case R_ARRAY_GET: {
                uint8_t dst = code[ip++];
                uint8_t arr_reg = code[ip++];
                uint8_t idx_reg = code[ip++];
                Value arr_val = REG(arr_reg);
                Value idx_val = REG(idx_reg);
                if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
                if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
                int64_t idx = idx_val.as.i;
                if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;
                if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
                    bp_fatal("array index out of bounds");
                }
                REG(dst) = gc_array_get(arr_val.as.arr, (size_t)idx);
                break;
            }
            case R_ARRAY_SET: {
                uint8_t arr_reg = code[ip++];
                uint8_t idx_reg = code[ip++];
                uint8_t val_reg = code[ip++];
                Value arr_val = REG(arr_reg);
                Value idx_val = REG(idx_reg);
                if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
                if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
                int64_t idx = idx_val.as.i;
                if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;
                if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
                    bp_fatal("array index out of bounds");
                }
                gc_array_set(arr_val.as.arr, (size_t)idx, REG(val_reg));
                break;
            }
            case R_MAP_NEW: {
                uint8_t dst = code[ip++];
                uint8_t src_base = code[ip++];
                uint16_t count = rd_u16(code, &ip);
                BpMap *map = gc_new_map(&vm->gc, count > 0 ? count * 2 : 16);
                for (uint16_t i = 0; i < count; i++) {
                    Value key = REG(src_base + i * 2);
                    Value val = REG(src_base + i * 2 + 1);
                    gc_map_set(&vm->gc, map, key, val);
                }
                REG(dst) = v_map(map);
                break;
            }
            case R_MAP_GET: {
                uint8_t dst = code[ip++];
                uint8_t map_reg = code[ip++];
                uint8_t key_reg = code[ip++];
                Value map_val = REG(map_reg);
                if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
                bool found;
                Value result = gc_map_get(map_val.as.map, REG(key_reg), &found);
                if (!found) bp_fatal("key not found in map");
                REG(dst) = result;
                break;
            }
            case R_MAP_SET: {
                uint8_t map_reg = code[ip++];
                uint8_t key_reg = code[ip++];
                uint8_t val_reg = code[ip++];
                Value map_val = REG(map_reg);
                if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
                gc_map_set(&vm->gc, map_val.as.map, REG(key_reg), REG(val_reg));
                break;
            }
            case R_TRY_BEGIN: {
                if (try_count >= MAX_TRY_HANDLERS) bp_fatal("too many nested try blocks");
                uint32_t catch_addr = rd_u32(code, &ip);
                uint32_t finally_addr = rd_u32(code, &ip);
                uint8_t exc_reg = code[ip++];
                try_handlers[try_count].catch_addr = catch_addr;
                try_handlers[try_count].finally_addr = finally_addr;
                try_handlers[try_count].exc_reg = exc_reg;
                try_handlers[try_count].frame_idx = frame_count - 1;
                try_handlers[try_count].reg_base = reg_base;
                try_count++;
                break;
            }
            case R_TRY_END: {
                if (try_count > 0) try_count--;
                break;
            }
            case R_THROW: {
                uint8_t src = code[ip++];
                Value exc_val = REG(src);
                if (try_count == 0) {
                    if (exc_val.type == VAL_STR) {
                        bp_fatal("unhandled exception: %s", exc_val.as.s->data);
                    } else {
                        bp_fatal("unhandled exception");
                    }
                }
                try_count--;
                RegTryHandler *h = &try_handlers[try_count];
                frame_count = h->frame_idx + 1;
                fn = frames[frame_count - 1].fn;
                code = frames[frame_count - 1].code;
                reg_base = h->reg_base;
                regs_top = reg_base + fn->reg_count;
                REG(h->exc_reg) = exc_val;
                ip = h->catch_addr;
                break;
            }
            case R_STRUCT_NEW: {
                uint8_t dst = code[ip++];
                uint16_t type_id = rd_u16(code, &ip);
                uint8_t src_base = code[ip++];
                uint8_t field_count = code[ip++];
                BpStruct *st = gc_new_struct(&vm->gc, type_id, field_count);
                for (uint8_t i = 0; i < field_count; i++) {
                    st->fields[i] = REG(src_base + i);
                }
                REG(dst) = v_struct(st);
                break;
            }
            case R_STRUCT_GET: {
                uint8_t dst = code[ip++];
                uint8_t struct_reg = code[ip++];
                uint16_t field_idx = rd_u16(code, &ip);
                Value st_val = REG(struct_reg);
                if (st_val.type != VAL_STRUCT) bp_fatal("cannot access field on non-struct");
                if (field_idx >= st_val.as.st->field_count) {
                    bp_fatal("struct field index out of bounds");
                }
                REG(dst) = gc_struct_get(st_val.as.st, field_idx);
                break;
            }
            case R_STRUCT_SET: {
                uint8_t struct_reg = code[ip++];
                uint16_t field_idx = rd_u16(code, &ip);
                uint8_t val_reg = code[ip++];
                Value st_val = REG(struct_reg);
                if (st_val.type != VAL_STRUCT) bp_fatal("cannot access field on non-struct");
                if (field_idx >= st_val.as.st->field_count) {
                    bp_fatal("struct field index out of bounds");
                }
                gc_struct_set(st_val.as.st, field_idx, REG(val_reg));
                break;
            }
            case R_CLASS_NEW: {
                uint8_t dst = code[ip++];
                uint16_t class_id = rd_u16(code, &ip);
                (void)code[ip++]; // arg_base
                (void)code[ip++]; // argc
                size_t field_count = 0;
                if (class_id < vm->mod.class_type_len) {
                    field_count = vm->mod.class_types[class_id].field_count;
                }
                BpClass *cls = gc_new_class(&vm->gc, class_id, field_count);
                REG(dst) = v_class(cls);
                break;
            }
            case R_CLASS_GET: {
                uint8_t dst = code[ip++];
                uint8_t obj_reg = code[ip++];
                uint16_t field_idx = rd_u16(code, &ip);
                Value cls_val = REG(obj_reg);
                if (cls_val.type != VAL_CLASS) bp_fatal("cannot access field on non-class");
                if (field_idx >= cls_val.as.cls->field_count) {
                    bp_fatal("class field index out of bounds");
                }
                REG(dst) = gc_class_get(cls_val.as.cls, field_idx);
                break;
            }
            case R_CLASS_SET: {
                uint8_t obj_reg = code[ip++];
                uint16_t field_idx = rd_u16(code, &ip);
                uint8_t val_reg = code[ip++];
                Value cls_val = REG(obj_reg);
                if (cls_val.type != VAL_CLASS) bp_fatal("cannot access field on non-class");
                if (field_idx >= cls_val.as.cls->field_count) {
                    bp_fatal("class field index out of bounds");
                }
                gc_class_set(cls_val.as.cls, field_idx, REG(val_reg));
                break;
            }
            case R_METHOD_CALL: {
                uint8_t dst = code[ip++];
                (void)code[ip++]; // obj_reg
                rd_u16(code, &ip); // method_id
                (void)code[ip++]; // arg_base
                (void)code[ip++]; // argc
                REG(dst) = v_null();
                break;
            }
            case R_SUPER_CALL: {
                uint8_t dst = code[ip++];
                rd_u16(code, &ip); // method_id
                (void)code[ip++]; // arg_base
                (void)code[ip++]; // argc
                REG(dst) = v_null();
                break;
            }
            case R_FFI_CALL: {
                uint8_t dst = code[ip++];
                rd_u16(code, &ip); // extern_id
                (void)code[ip++]; // arg_base
                (void)code[ip++]; // argc
                REG(dst) = v_null();
                break;
            }
            default:
                bp_fatal("unknown register opcode %u", op);
        }

        if (vm->gc.bytes > vm->gc.next_gc) gc_collect(&vm->gc, regs, regs_top, NULL, 0);
        if (vm->exiting) {
            free(regs);
            return vm->exit_code;
        }
    }

    free(regs);
    return 0;
#endif
}
