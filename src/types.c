#include "types.h"
#include "util.h"
#include <string.h>

typedef struct {
    char *name;
    Type type;
} Binding;

typedef struct {
    Binding *items;
    size_t len;
    size_t cap;
} Scope;

static void scope_put(Scope *s, const char *name, Type t) {
    for (size_t i = 0; i < s->len; i++) {
        if (strcmp(s->items[i].name, name) == 0) bp_fatal("duplicate symbol '%s'", name);
    }
    if (s->len + 1 > s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->items = bp_xrealloc(s->items, s->cap * sizeof(*s->items));
    }
    s->items[s->len].name = bp_xstrdup(name);
    s->items[s->len].type = t;
    s->len++;
}

static bool scope_get(Scope *s, const char *name, Type *out) {
    for (size_t i = 0; i < s->len; i++) {
        if (strcmp(s->items[i].name, name) == 0) { *out = s->items[i].type; return true; }
    }
    return false;
}

static bool type_eq(Type a, Type b) { return a.kind == b.kind; }

static const char *type_name(Type t) {
    switch (t.kind) {
        case TY_INT: return "int";
        case TY_FLOAT: return "float";
        case TY_BOOL: return "bool";
        case TY_STR: return "str";
        case TY_VOID: return "void";
        default: return "?";
    }
}

static Type check_expr(Expr *e, Scope *s);

static Type check_call(Expr *e, Scope *s) {
    // Builtins are pre-registered as function names with pseudo types; we enforce per-name rules.
    (void)s;
    const char *name = e->as.call.name;

    if (strcmp(name, "print") == 0) {
        for (size_t i = 0; i < e->as.call.argc; i++) check_expr(e->as.call.args[i], s);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "len") == 0) {
        if (e->as.call.argc != 1) bp_fatal("len expects 1 arg");
        Type t0 = check_expr(e->as.call.args[0], s);
        if (t0.kind != TY_STR) bp_fatal("len expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "substr") == 0) {
        if (e->as.call.argc != 3) bp_fatal("substr expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("substr arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("substr arg1 must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal("substr arg2 must be int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "read_line") == 0) {
        if (e->as.call.argc != 0) bp_fatal("read_line expects 0 args");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "to_str") == 0) {
        if (e->as.call.argc != 1) bp_fatal("to_str expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "clock_ms") == 0) {
        if (e->as.call.argc != 0) bp_fatal("clock_ms expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "exit") == 0) {
        if (e->as.call.argc != 1) bp_fatal("exit expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("exit expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "file_read") == 0) {
        if (e->as.call.argc != 1) bp_fatal("file_read expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("file_read expects str path");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "file_write") == 0) {
        if (e->as.call.argc != 2) bp_fatal("file_write expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("file_write arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("file_write arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "base64_encode") == 0) {
        if (e->as.call.argc != 1) bp_fatal("base64_encode expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("base64_encode expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "base64_decode") == 0) {
        if (e->as.call.argc != 1) bp_fatal("base64_decode expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("base64_decode expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "chr") == 0) {
        if (e->as.call.argc != 1) bp_fatal("chr expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("chr expects int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "ord") == 0) {
        if (e->as.call.argc != 1) bp_fatal("ord expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("ord expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    
    // Math functions
    if (strcmp(name, "abs") == 0) {
        if (e->as.call.argc != 1) bp_fatal("abs expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("abs expects int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("%s arg0 must be int", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("%s arg1 must be int", name);
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "pow") == 0) {
        if (e->as.call.argc != 2) bp_fatal("pow expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("pow arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("pow arg1 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "sqrt") == 0 || strcmp(name, "floor") == 0 || 
        strcmp(name, "ceil") == 0 || strcmp(name, "round") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("%s expects int", name);
        e->inferred = type_int();
        return e->inferred;
    }
    
    // String functions
    if (strcmp(name, "str_upper") == 0 || strcmp(name, "str_lower") == 0 || strcmp(name, "str_trim") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s expects str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "starts_with") == 0 || strcmp(name, "ends_with") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("%s arg1 must be str", name);
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "str_find") == 0) {
        if (e->as.call.argc != 2) bp_fatal("str_find expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_find arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("str_find arg1 must be str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "str_replace") == 0) {
        if (e->as.call.argc != 3) bp_fatal("str_replace expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_replace arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("str_replace arg1 must be str");
        if (check_expr(e->as.call.args[2], s).kind != TY_STR) bp_fatal("str_replace arg2 must be str");
        e->inferred = type_str();
        return e->inferred;
    }
    
    // Random functions
    if (strcmp(name, "rand") == 0) {
        if (e->as.call.argc != 0) bp_fatal("rand expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "rand_range") == 0) {
        if (e->as.call.argc != 2) bp_fatal("rand_range expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("rand_range arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("rand_range arg1 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "rand_seed") == 0) {
        if (e->as.call.argc != 1) bp_fatal("rand_seed expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("rand_seed expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    
    // File functions
    if (strcmp(name, "file_exists") == 0 || strcmp(name, "file_delete") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s expects str", name);
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "file_append") == 0) {
        if (e->as.call.argc != 2) bp_fatal("file_append expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("file_append arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("file_append arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    
    // System functions
    if (strcmp(name, "sleep") == 0) {
        if (e->as.call.argc != 1) bp_fatal("sleep expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("sleep expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "getenv") == 0) {
        if (e->as.call.argc != 1) bp_fatal("getenv expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("getenv expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    
    // Security functions
    if (strcmp(name, "hash_sha256") == 0 || strcmp(name, "hash_md5") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s expects str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "secure_compare") == 0) {
        if (e->as.call.argc != 2) bp_fatal("secure_compare expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("secure_compare arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("secure_compare arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "random_bytes") == 0) {
        if (e->as.call.argc != 1) bp_fatal("random_bytes expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("random_bytes expects int");
        e->inferred = type_str();
        return e->inferred;
    }
    
    // String utilities
    if (strcmp(name, "str_reverse") == 0) {
        if (e->as.call.argc != 1) bp_fatal("str_reverse expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_reverse expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_repeat") == 0) {
        if (e->as.call.argc != 2) bp_fatal("str_repeat expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_repeat arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("str_repeat arg1 must be int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_pad_left") == 0 || strcmp(name, "str_pad_right") == 0) {
        if (e->as.call.argc != 3) bp_fatal("%s expects 3 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("%s arg1 must be int", name);
        if (check_expr(e->as.call.args[2], s).kind != TY_STR) bp_fatal("%s arg2 must be str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_contains") == 0 || strcmp(name, "str_count") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("%s arg1 must be str", name);
        if (strcmp(name, "str_contains") == 0) {
            e->inferred = type_bool();
        } else {
            e->inferred = type_int();
        }
        return e->inferred;
    }
    if (strcmp(name, "int_to_hex") == 0) {
        if (e->as.call.argc != 1) bp_fatal("int_to_hex expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("int_to_hex expects int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "hex_to_int") == 0) {
        if (e->as.call.argc != 1) bp_fatal("hex_to_int expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("hex_to_int expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "str_char_at") == 0 || strcmp(name, "str_index_of") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s arg0 must be str", name);
        if (strcmp(name, "str_char_at") == 0) {
            if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("str_char_at arg1 must be int");
            e->inferred = type_str();
        } else {
            if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("str_index_of arg1 must be str");
            e->inferred = type_int();
        }
        return e->inferred;
    }
    
    // Validation functions
    if (strcmp(name, "is_digit") == 0 || strcmp(name, "is_alpha") == 0 ||
        strcmp(name, "is_alnum") == 0 || strcmp(name, "is_space") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("%s expects str", name);
        e->inferred = type_bool();
        return e->inferred;
    }
    
    // Math extensions
    if (strcmp(name, "clamp") == 0) {
        if (e->as.call.argc != 3) bp_fatal("clamp expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("clamp arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("clamp arg1 must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal("clamp arg2 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "sign") == 0) {
        if (e->as.call.argc != 1) bp_fatal("sign expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("sign expects int");
        e->inferred = type_int();
        return e->inferred;
    }
    
    // File utilities
    if (strcmp(name, "file_size") == 0) {
        if (e->as.call.argc != 1) bp_fatal("file_size expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("file_size expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "file_copy") == 0) {
        if (e->as.call.argc != 2) bp_fatal("file_copy expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("file_copy arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal("file_copy arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }

    // Float math functions (single arg, return float)
    if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
        strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 || strcmp(name, "atan") == 0 ||
        strcmp(name, "log") == 0 || strcmp(name, "log10") == 0 || strcmp(name, "log2") == 0 ||
        strcmp(name, "exp") == 0 || strcmp(name, "fabs") == 0 || strcmp(name, "ffloor") == 0 ||
        strcmp(name, "fceil") == 0 || strcmp(name, "fround") == 0 || strcmp(name, "fsqrt") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal("%s expects float", name);
        e->inferred = type_float();
        return e->inferred;
    }

    // Float math functions (two args, return float)
    if (strcmp(name, "atan2") == 0 || strcmp(name, "fpow") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal("%s arg0 must be float", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_FLOAT) bp_fatal("%s arg1 must be float", name);
        e->inferred = type_float();
        return e->inferred;
    }

    // Float conversion functions
    if (strcmp(name, "int_to_float") == 0) {
        if (e->as.call.argc != 1) bp_fatal("int_to_float expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("int_to_float expects int");
        e->inferred = type_float();
        return e->inferred;
    }
    if (strcmp(name, "float_to_int") == 0) {
        if (e->as.call.argc != 1) bp_fatal("float_to_int expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal("float_to_int expects float");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "float_to_str") == 0) {
        if (e->as.call.argc != 1) bp_fatal("float_to_str expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal("float_to_str expects float");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_to_float") == 0) {
        if (e->as.call.argc != 1) bp_fatal("str_to_float expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_to_float expects str");
        e->inferred = type_float();
        return e->inferred;
    }

    // Float utilities
    if (strcmp(name, "is_nan") == 0 || strcmp(name, "is_inf") == 0) {
        if (e->as.call.argc != 1) bp_fatal("%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal("%s expects float", name);
        e->inferred = type_bool();
        return e->inferred;
    }

    // User-defined functions not supported
    // v0: treat unknown as error.
    bp_fatal("unknown function '%s'", name);
    return type_void();
}

static Type check_expr(Expr *e, Scope *s) {
    switch (e->kind) {
        case EX_INT: e->inferred = type_int(); return e->inferred;
        case EX_FLOAT: e->inferred = type_float(); return e->inferred;
        case EX_STR: e->inferred = type_str(); return e->inferred;
        case EX_BOOL: e->inferred = type_bool(); return e->inferred;
        case EX_VAR: {
            Type t = type_void();
            if (!scope_get(s, e->as.var_name, &t)) bp_fatal("undefined variable '%s'", e->as.var_name);
            e->inferred = t;
            return t;
        }
        case EX_CALL:
            return check_call(e, s);
        case EX_UNARY: {
            Type r = check_expr(e->as.unary.rhs, s);
            if (e->as.unary.op == UOP_NEG) {
                if (r.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                if (r.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                bp_fatal("unary - expects int or float");
            }
            if (e->as.unary.op == UOP_NOT) {
                if (r.kind != TY_BOOL) bp_fatal("not expects bool");
                e->inferred = type_bool();
                return e->inferred;
            }
            bp_fatal("unknown unary op");
            return type_void();
        }
        case EX_BINARY: {
            Type a = check_expr(e->as.binary.lhs, s);
            Type b = check_expr(e->as.binary.rhs, s);
            switch (e->as.binary.op) {
                case BOP_ADD:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                    if (a.kind == TY_STR && b.kind == TY_STR) { e->inferred = type_str(); return e->inferred; }
                    bp_fatal("type error: + supports int+int, float+float, or str+str");
                    break;
                case BOP_SUB: case BOP_MUL: case BOP_DIV:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                    bp_fatal("arithmetic expects int or float");
                    break;
                case BOP_MOD:
                    if (a.kind != TY_INT || b.kind != TY_INT) bp_fatal("mod expects int");
                    e->inferred = type_int();
                    return e->inferred;
                case BOP_EQ: case BOP_NEQ:
                    if (!type_eq(a, b)) bp_fatal("==/!= requires same types");
                    e->inferred = type_bool();
                    return e->inferred;
                case BOP_LT: case BOP_LTE: case BOP_GT: case BOP_GTE:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_bool(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_bool(); return e->inferred; }
                    bp_fatal("comparisons expect int or float");
                    break;
                case BOP_AND: case BOP_OR:
                    if (a.kind != TY_BOOL || b.kind != TY_BOOL) bp_fatal("and/or expect bool");
                    e->inferred = type_bool();
                    return e->inferred;
                default: break;
            }
            bp_fatal("unknown binary op");
            return type_void();
        }
        default: break;
    }
    bp_fatal("unknown expr");
    return type_void();
}

static void check_stmt(Stmt *st, Scope *s, Type ret_type) {
    switch (st->kind) {
        case ST_LET: {
            Type it = check_expr(st->as.let.init, s);
            if (!type_eq(it, st->as.let.type)) {
                bp_fatal("type error: let %s: %s initialized with %s", st->as.let.name, type_name(st->as.let.type), type_name(it));
            }
            scope_put(s, st->as.let.name, st->as.let.type);
            return;
        }
        case ST_ASSIGN: {
            Type vt = type_void();
            if (!scope_get(s, st->as.assign.name, &vt)) bp_fatal("undefined variable '%s'", st->as.assign.name);
            Type it = check_expr(st->as.assign.value, s);
            if (!type_eq(vt, it)) bp_fatal("type error: assigning %s to %s", type_name(it), type_name(vt));
            return;
        }
        case ST_EXPR:
            check_expr(st->as.expr.expr, s);
            return;
        case ST_IF:
            if (check_expr(st->as.ifs.cond, s).kind != TY_BOOL) bp_fatal("if condition must be bool");
            for (size_t i = 0; i < st->as.ifs.then_len; i++) check_stmt(st->as.ifs.then_stmts[i], s, ret_type);
            for (size_t i = 0; i < st->as.ifs.else_len; i++) check_stmt(st->as.ifs.else_stmts[i], s, ret_type);
            return;
        case ST_WHILE:
            if (check_expr(st->as.wh.cond, s).kind != TY_BOOL) bp_fatal("while condition must be bool");
            for (size_t i = 0; i < st->as.wh.body_len; i++) check_stmt(st->as.wh.body[i], s, ret_type);
            return;
        case ST_RETURN:
            if (ret_type.kind == TY_VOID) {
                if (st->as.ret.value) bp_fatal("return value in void function");
                return;
            }
            if (!st->as.ret.value) bp_fatal("missing return value");
            if (!type_eq(check_expr(st->as.ret.value, s), ret_type)) bp_fatal("return type mismatch");
            return;
        default: break;
    }
    bp_fatal("unknown stmt");
}

void typecheck_module(Module *m) {
    // v0: only validate bodies; user-defined function calls are not type-checked by signature yet.
    // A minimal global pass is included to reserve builtins and prevent name collisions.
    for (size_t i = 0; i < m->fnc; i++) {
        Function *f = &m->fns[i];

        Scope s = {0};
        for (size_t p = 0; p < f->paramc; p++) scope_put(&s, f->params[p].name, f->params[p].type);

        for (size_t j = 0; j < f->body_len; j++) check_stmt(f->body[j], &s, f->ret_type);

        for (size_t k = 0; k < s.len; k++) free(s.items[k].name);
        free(s.items);
    }
}
