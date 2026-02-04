#include "vm.h"
#include "stdlib.h"
#include "util.h"
#include <string.h>

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
    BpFunc *fn = &vm->mod.funcs[vm->mod.entry];

    vm->localc = fn->locals;
    vm->locals = bp_xmalloc(vm->localc * sizeof(Value));
    for (size_t i = 0; i < vm->localc; i++) vm->locals[i] = v_null();

    // preload string constants referenced by this function are loaded lazily via OP_CONST_STR using module pool id.
    const uint8_t *code = fn->code;
    size_t ip = 0;

    while (ip < fn->code_len) {
        uint8_t op = code[ip++];
        switch (op) {
            case OP_CONST_I64: {
                int64_t x = rd_i64(code, &ip);
                push(vm, v_int(x));
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
                if (slot >= vm->localc) bp_fatal("bad local");
                push(vm, vm->locals[slot]);
                break;
            }
            case OP_STORE_LOCAL: {
                uint16_t slot = rd_u16(code, &ip);
                if (slot >= vm->localc) bp_fatal("bad local");
                vm->locals[slot] = pop(vm);
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
            case OP_RET: {
                Value rv = pop(vm);
                (void)rv;
                if (vm->exiting) return vm->exit_code;
                return (int)rv.as.i;
            }
            default:
                bp_fatal("unknown opcode %u", op);
        }

        if (vm->gc.bytes > vm->gc.next_gc) gc_collect(&vm->gc, vm->stack, vm->sp, vm->locals, vm->localc);
        if (vm->exiting) return vm->exit_code;
    }

    return 0;
}
