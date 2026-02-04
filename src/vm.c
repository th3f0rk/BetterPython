#include "vm.h"
#include "stdlib.h"
#include "util.h"
#include <string.h>

#define MAX_CALL_FRAMES 256

// Call frame for function calls
typedef struct {
    BpFunc *fn;
    const uint8_t *code;
    size_t ip;
    size_t locals_base;  // Index into locals array where this frame's locals start
} CallFrame;

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

void vm_init(Vm *vm, BpModule mod) {
    memset(vm, 0, sizeof(*vm));
    vm->mod = mod;
    gc_init(&vm->gc);
    vm->sp = 0;
    vm->locals = NULL;
    vm->localc = 0;
    vm->exit_code = 0;
    vm->exiting = false;
}

void vm_free(Vm *vm) {
    gc_free_all(&vm->gc);
    free(vm->locals);
    bc_module_free(&vm->mod);
}

static void push(Vm *vm, Value v) {
    if (vm->sp >= sizeof(vm->stack) / sizeof(vm->stack[0])) bp_fatal("stack overflow");
    vm->stack[vm->sp++] = v;
}
static Value pop(Vm *vm) {
    if (!vm->sp) bp_fatal("stack underflow");
    return vm->stack[--vm->sp];
}

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

static Value op_lt(Value a, Value b) { return v_bool(a.as.i < b.as.i); }
static Value op_lte(Value a, Value b) { return v_bool(a.as.i <= b.as.i); }
static Value op_gt(Value a, Value b) { return v_bool(a.as.i > b.as.i); }
static Value op_gte(Value a, Value b) { return v_bool(a.as.i >= b.as.i); }

int vm_run(Vm *vm) {
    if (vm->mod.entry >= vm->mod.fn_len) bp_fatal("no entry function");

    // Call frame stack
    CallFrame frames[MAX_CALL_FRAMES];
    size_t frame_count = 0;

    // Calculate total locals needed across all frames (generous initial size)
    size_t total_locals_cap = 1024;
    vm->locals = bp_xmalloc(total_locals_cap * sizeof(Value));
    for (size_t i = 0; i < total_locals_cap; i++) vm->locals[i] = v_null();

    // Set up initial frame for main()
    BpFunc *fn = &vm->mod.funcs[vm->mod.entry];
    frames[frame_count].fn = fn;
    frames[frame_count].code = fn->code;
    frames[frame_count].ip = 0;
    frames[frame_count].locals_base = 0;
    frame_count++;

    vm->localc = fn->locals;
    size_t locals_top = fn->locals;  // Next available local slot

    const uint8_t *code = fn->code;
    size_t ip = 0;
    size_t locals_base = 0;

    while (ip < fn->code_len || frame_count > 0) {
        if (ip >= fn->code_len) {
            // Implicit return at end of function
            if (frame_count == 1) break;  // End of main
            frame_count--;
            fn = frames[frame_count - 1].fn;
            code = frames[frame_count - 1].code;
            ip = frames[frame_count - 1].ip;
            locals_base = frames[frame_count - 1].locals_base;
            locals_top = locals_base + fn->locals;
            push(vm, v_int(0));  // Default return value
            continue;
        }

        uint8_t op = code[ip++];
        switch (op) {
            case OP_CONST_I64: {
                int64_t x = rd_i64(code, &ip);
                push(vm, v_int(x));
                break;
            }
            case OP_CONST_F64: {
                double x = rd_f64(code, &ip);
                push(vm, v_float(x));
                break;
            }
            case OP_CONST_BOOL: {
                uint8_t x = code[ip++];
                push(vm, v_bool(x != 0));
                break;
            }
            case OP_CONST_STR: {
                uint32_t local_id = rd_u32(code, &ip);
                if (local_id >= fn->str_const_len) bp_fatal("bad str const");
                uint32_t pool_id = fn->str_const_ids[local_id];
                if (pool_id >= vm->mod.str_len) bp_fatal("bad str pool id");
                const char *s = vm->mod.strings[pool_id];
                BpStr *o = gc_new_str(&vm->gc, s, strlen(s));
                push(vm, v_str(o));
                break;
            }
            case OP_POP:
                (void)pop(vm);
                break;
            case OP_LOAD_LOCAL: {
                uint16_t slot = rd_u16(code, &ip);
                size_t abs_slot = locals_base + slot;
                if (abs_slot >= total_locals_cap) bp_fatal("bad local");
                push(vm, vm->locals[abs_slot]);
                break;
            }
            case OP_STORE_LOCAL: {
                uint16_t slot = rd_u16(code, &ip);
                size_t abs_slot = locals_base + slot;
                if (abs_slot >= total_locals_cap) bp_fatal("bad local");
                vm->locals[abs_slot] = pop(vm);
                break;
            }
            case OP_ADD_I64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_int(a.as.i + b.as.i));
                break;
            }
            case OP_SUB_I64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_int(a.as.i - b.as.i));
                break;
            }
            case OP_MUL_I64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_int(a.as.i * b.as.i));
                break;
            }
            case OP_DIV_I64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_int(a.as.i / b.as.i));
                break;
            }
            case OP_MOD_I64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_int(a.as.i % b.as.i));
                break;
            }
            case OP_ADD_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_float(a.as.f + b.as.f));
                break;
            }
            case OP_SUB_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_float(a.as.f - b.as.f));
                break;
            }
            case OP_MUL_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_float(a.as.f * b.as.f));
                break;
            }
            case OP_DIV_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_float(a.as.f / b.as.f));
                break;
            }
            case OP_NEG_F64: {
                Value a = pop(vm);
                push(vm, v_float(-a.as.f));
                break;
            }
            case OP_ADD_STR: {
                Value b = pop(vm), a = pop(vm);
                push(vm, add_str(vm, a, b));
                break;
            }
            case OP_EQ: {
                Value b = pop(vm), a = pop(vm);
                push(vm, op_eq(a, b));
                break;
            }
            case OP_NEQ: {
                Value b = pop(vm), a = pop(vm);
                Value eq = op_eq(a, b);
                push(vm, v_bool(!eq.as.b));
                break;
            }
            case OP_LT: {
                Value b = pop(vm), a = pop(vm);
                push(vm, op_lt(a, b));
                break;
            }
            case OP_LTE: {
                Value b = pop(vm), a = pop(vm);
                push(vm, op_lte(a, b));
                break;
            }
            case OP_GT: {
                Value b = pop(vm), a = pop(vm);
                push(vm, op_gt(a, b));
                break;
            }
            case OP_GTE: {
                Value b = pop(vm), a = pop(vm);
                push(vm, op_gte(a, b));
                break;
            }
            case OP_LT_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(a.as.f < b.as.f));
                break;
            }
            case OP_LTE_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(a.as.f <= b.as.f));
                break;
            }
            case OP_GT_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(a.as.f > b.as.f));
                break;
            }
            case OP_GTE_F64: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(a.as.f >= b.as.f));
                break;
            }
            case OP_NOT: {
                Value a = pop(vm);
                push(vm, v_bool(!v_is_truthy(a)));
                break;
            }
            case OP_AND: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(v_is_truthy(a) && v_is_truthy(b)));
                break;
            }
            case OP_OR: {
                Value b = pop(vm), a = pop(vm);
                push(vm, v_bool(v_is_truthy(a) || v_is_truthy(b)));
                break;
            }
            case OP_NEG: {
                Value a = pop(vm);
                push(vm, v_int(-a.as.i));
                break;
            }
            case OP_JMP: {
                uint32_t tgt = rd_u32(code, &ip);
                ip = tgt;
                break;
            }
            case OP_JMP_IF_FALSE: {
                uint32_t tgt = rd_u32(code, &ip);
                Value c = pop(vm);
                if (!v_is_truthy(c)) ip = tgt;
                break;
            }
            case OP_CALL_BUILTIN: {
                uint16_t id = rd_u16(code, &ip);
                uint16_t argc = rd_u16(code, &ip);
                if (argc > vm->sp) bp_fatal("bad argc");
                Value *args = &vm->stack[vm->sp - argc];
                Value r = stdlib_call((BuiltinId)id, args, argc, &vm->gc, &vm->exit_code, &vm->exiting);
                vm->sp -= argc;
                push(vm, r);
                break;
            }
            case OP_CALL: {
                uint32_t fn_idx = rd_u32(code, &ip);
                uint16_t argc = rd_u16(code, &ip);

                if (fn_idx >= vm->mod.fn_len) bp_fatal("bad function index");
                if (frame_count >= MAX_CALL_FRAMES) bp_fatal("call stack overflow");

                BpFunc *callee = &vm->mod.funcs[fn_idx];

                // Save current frame
                frames[frame_count - 1].ip = ip;

                // Set up new frame
                size_t new_locals_base = locals_top;

                // Ensure we have enough locals space
                size_t needed = new_locals_base + callee->locals;
                if (needed > total_locals_cap) {
                    while (needed > total_locals_cap) total_locals_cap *= 2;
                    vm->locals = bp_xrealloc(vm->locals, total_locals_cap * sizeof(Value));
                    for (size_t i = locals_top; i < total_locals_cap; i++) vm->locals[i] = v_null();
                }

                // Copy arguments to new frame's locals (parameters are first locals)
                if (argc > vm->sp) bp_fatal("bad argc");
                for (uint16_t i = 0; i < argc; i++) {
                    vm->locals[new_locals_base + i] = vm->stack[vm->sp - argc + i];
                }
                vm->sp -= argc;

                // Initialize remaining locals to null
                for (size_t i = argc; i < callee->locals; i++) {
                    vm->locals[new_locals_base + i] = v_null();
                }

                // Push new frame
                frames[frame_count].fn = callee;
                frames[frame_count].code = callee->code;
                frames[frame_count].ip = 0;
                frames[frame_count].locals_base = new_locals_base;
                frame_count++;

                // Switch to new function
                fn = callee;
                code = callee->code;
                ip = 0;
                locals_base = new_locals_base;
                locals_top = new_locals_base + callee->locals;
                break;
            }
            case OP_RET: {
                Value rv = pop(vm);

                if (frame_count == 1) {
                    // Returning from main
                    if (vm->exiting) return vm->exit_code;
                    return (int)rv.as.i;
                }

                // Pop frame and return to caller
                frame_count--;
                fn = frames[frame_count - 1].fn;
                code = frames[frame_count - 1].code;
                ip = frames[frame_count - 1].ip;
                locals_base = frames[frame_count - 1].locals_base;
                locals_top = locals_base + fn->locals;

                // Push return value
                push(vm, rv);
                break;
            }
            case OP_ARRAY_NEW: {
                uint32_t count = rd_u32(code, &ip);
                BpArray *arr = gc_new_array(&vm->gc, count > 0 ? count : 8);
                // Pop elements from stack (they're in order, so first pushed = first element)
                // But we pop in reverse order, so collect then reverse
                if (count > vm->sp) bp_fatal("stack underflow in array creation");
                for (uint32_t i = 0; i < count; i++) {
                    // Pop from top of stack
                    Value v = vm->stack[vm->sp - count + i];
                    gc_array_push(&vm->gc, arr, v);
                }
                vm->sp -= count;
                push(vm, v_array(arr));
                break;
            }
            case OP_ARRAY_GET: {
                Value idx_val = pop(vm);
                Value arr_val = pop(vm);
                if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
                if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
                int64_t idx = idx_val.as.i;
                if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;  // Support negative indexing
                if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
                    bp_fatal("array index out of bounds: %lld (len %zu)", (long long)idx, arr_val.as.arr->len);
                }
                push(vm, gc_array_get(arr_val.as.arr, (size_t)idx));
                break;
            }
            case OP_ARRAY_SET: {
                Value val = pop(vm);
                Value idx_val = pop(vm);
                Value arr_val = pop(vm);
                if (arr_val.type != VAL_ARRAY) bp_fatal("cannot index non-array");
                if (idx_val.type != VAL_INT) bp_fatal("array index must be int");
                int64_t idx = idx_val.as.i;
                if (idx < 0) idx = (int64_t)arr_val.as.arr->len + idx;
                if (idx < 0 || (size_t)idx >= arr_val.as.arr->len) {
                    bp_fatal("array index out of bounds: %lld (len %zu)", (long long)idx, arr_val.as.arr->len);
                }
                gc_array_set(arr_val.as.arr, (size_t)idx, val);
                break;
            }
            case OP_MAP_NEW: {
                uint32_t count = rd_u32(code, &ip);
                BpMap *map = gc_new_map(&vm->gc, count > 0 ? count * 2 : 16);
                // Stack has key, value, key, value, ... (in order)
                // Pop them all and insert into map
                if (count * 2 > vm->sp) bp_fatal("stack underflow in map creation");
                for (uint32_t i = 0; i < count; i++) {
                    Value key = vm->stack[vm->sp - count * 2 + i * 2];
                    Value val = vm->stack[vm->sp - count * 2 + i * 2 + 1];
                    gc_map_set(&vm->gc, map, key, val);
                }
                vm->sp -= count * 2;
                push(vm, v_map(map));
                break;
            }
            case OP_MAP_GET: {
                Value key = pop(vm);
                Value map_val = pop(vm);
                if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
                bool found;
                Value result = gc_map_get(map_val.as.map, key, &found);
                if (!found) bp_fatal("key not found in map");
                push(vm, result);
                break;
            }
            case OP_MAP_SET: {
                Value val = pop(vm);
                Value key = pop(vm);
                Value map_val = pop(vm);
                if (map_val.type != VAL_MAP) bp_fatal("cannot index non-map");
                gc_map_set(&vm->gc, map_val.as.map, key, val);
                break;
            }
            default:
                bp_fatal("unknown opcode %u", op);
        }

        if (vm->gc.bytes > vm->gc.next_gc) gc_collect(&vm->gc, vm->stack, vm->sp, vm->locals, locals_top);
        if (vm->exiting) return vm->exit_code;
    }

    return 0;
}
