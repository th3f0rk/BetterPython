#include "compiler.h"
#include "util.h"
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_put(Buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        while (b->len + n > b->cap) b->cap *= 2;
        b->data = bp_xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static void buf_u8(Buf *b, uint8_t x) { buf_put(b, &x, 1); }
static void buf_u16(Buf *b, uint16_t x) { buf_put(b, &x, 2); }
static void buf_u32(Buf *b, uint32_t x) { buf_put(b, &x, 4); }
static void buf_i64(Buf *b, int64_t x) { buf_put(b, &x, 8); }
static void buf_f64(Buf *b, double x) { buf_put(b, &x, 8); }

typedef struct {
    char *name;
    uint16_t slot;
} Local;

typedef struct {
    Local *items;
    size_t len;
    size_t cap;
} Locals;

static uint16_t locals_add(Locals *ls, const char *name) {
    for (size_t i = 0; i < ls->len; i++) {
        if (strcmp(ls->items[i].name, name) == 0) bp_fatal("duplicate local '%s'", name);
    }
    if (ls->len + 1 > ls->cap) {
        ls->cap = ls->cap ? ls->cap * 2 : 16;
        ls->items = bp_xrealloc(ls->items, ls->cap * sizeof(*ls->items));
    }
    uint16_t slot = (uint16_t)ls->len;
    ls->items[ls->len].name = bp_xstrdup(name);
    ls->items[ls->len].slot = slot;
    ls->len++;
    return slot;
}

static uint16_t locals_get(Locals *ls, const char *name) {
    for (size_t i = 0; i < ls->len; i++) {
        if (strcmp(ls->items[i].name, name) == 0) return ls->items[i].slot;
    }
    bp_fatal("unknown local '%s'", name);
    return 0;
}

static void locals_free(Locals *ls) {
    for (size_t i = 0; i < ls->len; i++) free(ls->items[i].name);
    free(ls->items);
    memset(ls, 0, sizeof(*ls));
}

typedef struct {
    char **strings;
    size_t len;
    size_t cap;
} StrPool;

static uint32_t strpool_add(StrPool *sp, const char *s) {
    for (size_t i = 0; i < sp->len; i++) {
        if (strcmp(sp->strings[i], s) == 0) return (uint32_t)i;
    }
    if (sp->len + 1 > sp->cap) {
        sp->cap = sp->cap ? sp->cap * 2 : 32;
        sp->strings = bp_xrealloc(sp->strings, sp->cap * sizeof(*sp->strings));
    }
    sp->strings[sp->len] = bp_xstrdup(s);
    return (uint32_t)sp->len++;
}

static BuiltinId builtin_id(const char *name) {
    if (strcmp(name, "print") == 0) return BI_PRINT;
    if (strcmp(name, "len") == 0) return BI_LEN;
    if (strcmp(name, "substr") == 0) return BI_SUBSTR;
    if (strcmp(name, "read_line") == 0) return BI_READ_LINE;
    if (strcmp(name, "to_str") == 0) return BI_TO_STR;
    if (strcmp(name, "clock_ms") == 0) return BI_CLOCK_MS;
    if (strcmp(name, "exit") == 0) return BI_EXIT;
    if (strcmp(name, "file_read") == 0) return BI_FILE_READ;
    if (strcmp(name, "file_write") == 0) return BI_FILE_WRITE;
    if (strcmp(name, "base64_encode") == 0) return BI_BASE64_ENCODE;
    if (strcmp(name, "base64_decode") == 0) return BI_BASE64_DECODE;
    if (strcmp(name, "chr") == 0) return BI_CHR;
    if (strcmp(name, "ord") == 0) return BI_ORD;
    
    // Math
    if (strcmp(name, "abs") == 0) return BI_ABS;
    if (strcmp(name, "min") == 0) return BI_MIN;
    if (strcmp(name, "max") == 0) return BI_MAX;
    if (strcmp(name, "pow") == 0) return BI_POW;
    if (strcmp(name, "sqrt") == 0) return BI_SQRT;
    if (strcmp(name, "floor") == 0) return BI_FLOOR;
    if (strcmp(name, "ceil") == 0) return BI_CEIL;
    if (strcmp(name, "round") == 0) return BI_ROUND;
    
    // Strings
    if (strcmp(name, "str_upper") == 0) return BI_STR_UPPER;
    if (strcmp(name, "str_lower") == 0) return BI_STR_LOWER;
    if (strcmp(name, "str_trim") == 0) return BI_STR_TRIM;
    if (strcmp(name, "starts_with") == 0) return BI_STR_STARTS_WITH;
    if (strcmp(name, "ends_with") == 0) return BI_STR_ENDS_WITH;
    if (strcmp(name, "str_find") == 0) return BI_STR_FIND;
    if (strcmp(name, "str_replace") == 0) return BI_STR_REPLACE;
    
    // Random
    if (strcmp(name, "rand") == 0) return BI_RAND;
    if (strcmp(name, "rand_range") == 0) return BI_RAND_RANGE;
    if (strcmp(name, "rand_seed") == 0) return BI_RAND_SEED;
    
    // File
    if (strcmp(name, "file_exists") == 0) return BI_FILE_EXISTS;
    if (strcmp(name, "file_delete") == 0) return BI_FILE_DELETE;
    if (strcmp(name, "file_append") == 0) return BI_FILE_APPEND;
    
    // System
    if (strcmp(name, "sleep") == 0) return BI_SLEEP;
    if (strcmp(name, "getenv") == 0) return BI_GETENV;
    
    // Security
    if (strcmp(name, "hash_sha256") == 0) return BI_HASH_SHA256;
    if (strcmp(name, "hash_md5") == 0) return BI_HASH_MD5;
    if (strcmp(name, "secure_compare") == 0) return BI_SECURE_COMPARE;
    if (strcmp(name, "random_bytes") == 0) return BI_RANDOM_BYTES;
    
    // String utilities
    if (strcmp(name, "str_reverse") == 0) return BI_STR_REVERSE;
    if (strcmp(name, "str_repeat") == 0) return BI_STR_REPEAT;
    if (strcmp(name, "str_pad_left") == 0) return BI_STR_PAD_LEFT;
    if (strcmp(name, "str_pad_right") == 0) return BI_STR_PAD_RIGHT;
    if (strcmp(name, "str_contains") == 0) return BI_STR_CONTAINS;
    if (strcmp(name, "str_count") == 0) return BI_STR_COUNT;
    if (strcmp(name, "int_to_hex") == 0) return BI_INT_TO_HEX;
    if (strcmp(name, "hex_to_int") == 0) return BI_HEX_TO_INT;
    if (strcmp(name, "str_char_at") == 0) return BI_STR_CHAR_AT;
    if (strcmp(name, "str_index_of") == 0) return BI_STR_INDEX_OF;
    
    // Validation
    if (strcmp(name, "is_digit") == 0) return BI_IS_DIGIT;
    if (strcmp(name, "is_alpha") == 0) return BI_IS_ALPHA;
    if (strcmp(name, "is_alnum") == 0) return BI_IS_ALNUM;
    if (strcmp(name, "is_space") == 0) return BI_IS_SPACE;
    
    // Math extensions
    if (strcmp(name, "clamp") == 0) return BI_CLAMP;
    if (strcmp(name, "sign") == 0) return BI_SIGN;
    
    // File utilities
    if (strcmp(name, "file_size") == 0) return BI_FILE_SIZE;
    if (strcmp(name, "file_copy") == 0) return BI_FILE_COPY;

    // Float math functions
    if (strcmp(name, "sin") == 0) return BI_SIN;
    if (strcmp(name, "cos") == 0) return BI_COS;
    if (strcmp(name, "tan") == 0) return BI_TAN;
    if (strcmp(name, "asin") == 0) return BI_ASIN;
    if (strcmp(name, "acos") == 0) return BI_ACOS;
    if (strcmp(name, "atan") == 0) return BI_ATAN;
    if (strcmp(name, "atan2") == 0) return BI_ATAN2;
    if (strcmp(name, "log") == 0) return BI_LOG;
    if (strcmp(name, "log10") == 0) return BI_LOG10;
    if (strcmp(name, "log2") == 0) return BI_LOG2;
    if (strcmp(name, "exp") == 0) return BI_EXP;
    if (strcmp(name, "fabs") == 0) return BI_FABS;
    if (strcmp(name, "ffloor") == 0) return BI_FFLOOR;
    if (strcmp(name, "fceil") == 0) return BI_FCEIL;
    if (strcmp(name, "fround") == 0) return BI_FROUND;
    if (strcmp(name, "fsqrt") == 0) return BI_FSQRT;
    if (strcmp(name, "fpow") == 0) return BI_FPOW;

    // Float conversion functions
    if (strcmp(name, "int_to_float") == 0) return BI_INT_TO_FLOAT;
    if (strcmp(name, "float_to_int") == 0) return BI_FLOAT_TO_INT;
    if (strcmp(name, "float_to_str") == 0) return BI_FLOAT_TO_STR;
    if (strcmp(name, "str_to_float") == 0) return BI_STR_TO_FLOAT;

    // Float utilities
    if (strcmp(name, "is_nan") == 0) return BI_IS_NAN;
    if (strcmp(name, "is_inf") == 0) return BI_IS_INF;

    bp_fatal("unknown builtin '%s'", name);
    return BI_PRINT;
}

static void emit_jump_patch(Buf *b, size_t at, uint32_t target) {
    memcpy(b->data + at, &target, 4);
}

typedef struct {
    Buf code;
    uint32_t *str_ids;
    size_t str_len;
    size_t str_cap;
    Locals locals;
    StrPool *pool;
} FnEmit;

static void fn_add_strref(FnEmit *fe, uint32_t sid) {
    if (fe->str_len + 1 > fe->str_cap) {
        fe->str_cap = fe->str_cap ? fe->str_cap * 2 : 16;
        fe->str_ids = bp_xrealloc(fe->str_ids, fe->str_cap * sizeof(uint32_t));
    }
    fe->str_ids[fe->str_len++] = sid;
}

static uint32_t fn_str_index(FnEmit *fe, uint32_t pool_id) {
    for (size_t i = 0; i < fe->str_len; i++) {
        if (fe->str_ids[i] == pool_id) return (uint32_t)i;
    }
    fn_add_strref(fe, pool_id);
    return (uint32_t)(fe->str_len - 1);
}

static void emit_expr(FnEmit *fe, const Expr *e);

static void emit_stmt(FnEmit *fe, const Stmt *s) {
    switch (s->kind) {
        case ST_LET: {
            emit_expr(fe, s->as.let.init);
            uint16_t slot = locals_add(&fe->locals, s->as.let.name);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, slot);
            return;
        }
        case ST_ASSIGN: {
            emit_expr(fe, s->as.assign.value);
            uint16_t slot = locals_get(&fe->locals, s->as.assign.name);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, slot);
            return;
        }
        case ST_EXPR: {
            emit_expr(fe, s->as.expr.expr);
            buf_u8(&fe->code, OP_POP);
            return;
        }
        case ST_RETURN: {
            if (s->as.ret.value) emit_expr(fe, s->as.ret.value);
            else { buf_u8(&fe->code, OP_CONST_BOOL); buf_u8(&fe->code, 0); } // placeholder "null" via bool false? (VM will ignore for void)
            buf_u8(&fe->code, OP_RET);
            return;
        }
        case ST_IF: {
            emit_expr(fe, s->as.ifs.cond);
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_false_at = fe->code.len;
            buf_u32(&fe->code, 0);

            for (size_t i = 0; i < s->as.ifs.then_len; i++) emit_stmt(fe, s->as.ifs.then_stmts[i]);

            buf_u8(&fe->code, OP_JMP);
            size_t jmp_end_at = fe->code.len;
            buf_u32(&fe->code, 0);

            emit_jump_patch(&fe->code, jmp_false_at, (uint32_t)fe->code.len);

            for (size_t i = 0; i < s->as.ifs.else_len; i++) emit_stmt(fe, s->as.ifs.else_stmts[i]);

            emit_jump_patch(&fe->code, jmp_end_at, (uint32_t)fe->code.len);
            return;
        }
        case ST_WHILE: {
            uint32_t loop_start = (uint32_t)fe->code.len;

            emit_expr(fe, s->as.wh.cond);
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);

            for (size_t i = 0; i < s->as.wh.body_len; i++) emit_stmt(fe, s->as.wh.body[i]);

            buf_u8(&fe->code, OP_JMP);
            buf_u32(&fe->code, loop_start);

            emit_jump_patch(&fe->code, jmp_out_at, (uint32_t)fe->code.len);
            return;
        }
        default: break;
    }
    bp_fatal("unknown stmt");
}

static void emit_expr(FnEmit *fe, const Expr *e) {
    switch (e->kind) {
        case EX_INT:
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, e->as.int_val);
            return;
        case EX_FLOAT:
            buf_u8(&fe->code, OP_CONST_F64);
            buf_f64(&fe->code, e->as.float_val);
            return;
        case EX_BOOL:
            buf_u8(&fe->code, OP_CONST_BOOL);
            buf_u8(&fe->code, e->as.bool_val ? 1 : 0);
            return;
        case EX_STR: {
            uint32_t sid = strpool_add(fe->pool, e->as.str_val);
            uint32_t local_id = fn_str_index(fe, sid);
            buf_u8(&fe->code, OP_CONST_STR);
            buf_u32(&fe->code, local_id);
            return;
        }
        case EX_VAR: {
            uint16_t slot = locals_get(&fe->locals, e->as.var_name);
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, slot);
            return;
        }
        case EX_CALL: {
            for (size_t i = 0; i < e->as.call.argc; i++) emit_expr(fe, e->as.call.args[i]);
            BuiltinId id = builtin_id(e->as.call.name);
            buf_u8(&fe->code, OP_CALL_BUILTIN);
            buf_u16(&fe->code, (uint16_t)id);
            buf_u16(&fe->code, (uint16_t)e->as.call.argc);
            return;
        }
        case EX_UNARY: {
            emit_expr(fe, e->as.unary.rhs);
            if (e->as.unary.op == UOP_NEG) {
                if (e->inferred.kind == TY_FLOAT) buf_u8(&fe->code, OP_NEG_F64);
                else buf_u8(&fe->code, OP_NEG);
            } else {
                buf_u8(&fe->code, OP_NOT);
            }
            return;
        }
        case EX_BINARY: {
            // Boolean AND/OR operators
            if (e->as.binary.op == BOP_AND || e->as.binary.op == BOP_OR) {
                emit_expr(fe, e->as.binary.lhs);
                emit_expr(fe, e->as.binary.rhs);
                buf_u8(&fe->code, (e->as.binary.op == BOP_AND) ? OP_AND : OP_OR);
                return;
            }
            emit_expr(fe, e->as.binary.lhs);
            emit_expr(fe, e->as.binary.rhs);
            bool is_float = (e->inferred.kind == TY_FLOAT);
            switch (e->as.binary.op) {
                case BOP_ADD:
                    if (e->inferred.kind == TY_STR) buf_u8(&fe->code, OP_ADD_STR);
                    else if (is_float) buf_u8(&fe->code, OP_ADD_F64);
                    else buf_u8(&fe->code, OP_ADD_I64);
                    return;
                case BOP_SUB:
                    buf_u8(&fe->code, is_float ? OP_SUB_F64 : OP_SUB_I64);
                    return;
                case BOP_MUL:
                    buf_u8(&fe->code, is_float ? OP_MUL_F64 : OP_MUL_I64);
                    return;
                case BOP_DIV:
                    buf_u8(&fe->code, is_float ? OP_DIV_F64 : OP_DIV_I64);
                    return;
                case BOP_MOD:
                    buf_u8(&fe->code, OP_MOD_I64);
                    return;
                case BOP_EQ: buf_u8(&fe->code, OP_EQ); return;
                case BOP_NEQ: buf_u8(&fe->code, OP_NEQ); return;
                case BOP_LT:
                    buf_u8(&fe->code, is_float ? OP_LT_F64 : OP_LT);
                    return;
                case BOP_LTE:
                    buf_u8(&fe->code, is_float ? OP_LTE_F64 : OP_LTE);
                    return;
                case BOP_GT:
                    buf_u8(&fe->code, is_float ? OP_GT_F64 : OP_GT);
                    return;
                case BOP_GTE:
                    buf_u8(&fe->code, is_float ? OP_GTE_F64 : OP_GTE);
                    return;
                default: break;
            }
            bp_fatal("unsupported binary op");
            return;
        }
        default: break;
    }
    bp_fatal("unknown expr");
}

BpModule compile_module(const Module *m) {
    BpModule out;
    memset(&out, 0, sizeof(out));

    StrPool pool = {0};

    out.fn_len = m->fnc;
    out.funcs = bp_xmalloc(out.fn_len * sizeof(*out.funcs));
    memset(out.funcs, 0, out.fn_len * sizeof(*out.funcs));

    out.entry = 0;
    for (size_t i = 0; i < m->fnc; i++) {
        if (strcmp(m->fns[i].name, "main") == 0) out.entry = (uint32_t)i;
    }

    for (size_t i = 0; i < m->fnc; i++) {
        const Function *f = &m->fns[i];

        FnEmit fe;
        memset(&fe, 0, sizeof(fe));
        fe.pool = &pool;

        // params occupy first locals
        for (size_t p = 0; p < f->paramc; p++) locals_add(&fe.locals, f->params[p].name);

        for (size_t s = 0; s < f->body_len; s++) emit_stmt(&fe, f->body[s]);

        // implicit return if missing
        buf_u8(&fe.code, OP_CONST_I64);
        buf_i64(&fe.code, 0);
        buf_u8(&fe.code, OP_RET);

        out.funcs[i].name = bp_xstrdup(f->name);
        out.funcs[i].arity = (uint16_t)f->paramc;
        out.funcs[i].locals = (uint16_t)fe.locals.len;
        out.funcs[i].code = fe.code.data;
        out.funcs[i].code_len = fe.code.len;
        out.funcs[i].str_const_ids = fe.str_ids;
        out.funcs[i].str_const_len = fe.str_len;

        locals_free(&fe.locals);
    }

    out.str_len = pool.len;
    out.strings = pool.strings;

    return out;
}
