#include "ast.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

Type *type_new(TypeKind kind) {
    Type *t = bp_xmalloc(sizeof(*t));
    t->kind = kind;
    t->elem_type = NULL;
    t->key_type = NULL;
    t->struct_name = NULL;
    return t;
}

Type *type_array(Type *elem) {
    Type *t = bp_xmalloc(sizeof(*t));
    t->kind = TY_ARRAY;
    t->elem_type = elem;
    t->key_type = NULL;
    t->struct_name = NULL;
    return t;
}

Type *type_map(Type *key, Type *value) {
    Type *t = bp_xmalloc(sizeof(*t));
    t->kind = TY_MAP;
    t->key_type = key;
    t->elem_type = value;
    t->struct_name = NULL;
    return t;
}

Type *type_struct(char *name) {
    Type *t = bp_xmalloc(sizeof(*t));
    t->kind = TY_STRUCT;
    t->elem_type = NULL;
    t->key_type = NULL;
    t->struct_name = name;
    return t;
}

static Expr *expr_alloc(ExprKind k, size_t line) {
    Expr *e = bp_xmalloc(sizeof(*e));
    memset(e, 0, sizeof(*e));
    e->kind = k;
    e->line = line;
    e->inferred = type_void();
    return e;
}

Expr *expr_new_int(int64_t v, size_t line) {
    Expr *e = expr_alloc(EX_INT, line);
    e->as.int_val = v;
    return e;
}

Expr *expr_new_float(double v, size_t line) {
    Expr *e = expr_alloc(EX_FLOAT, line);
    e->as.float_val = v;
    return e;
}

Expr *expr_new_str(char *s, size_t line) {
    Expr *e = expr_alloc(EX_STR, line);
    e->as.str_val = s;
    return e;
}

Expr *expr_new_bool(bool v, size_t line) {
    Expr *e = expr_alloc(EX_BOOL, line);
    e->as.bool_val = v;
    return e;
}

Expr *expr_new_var(char *name, size_t line) {
    Expr *e = expr_alloc(EX_VAR, line);
    e->as.var_name = name;
    return e;
}

Expr *expr_new_call(char *name, Expr **args, size_t argc, size_t line) {
    Expr *e = expr_alloc(EX_CALL, line);
    e->as.call.name = name;
    e->as.call.args = args;
    e->as.call.argc = argc;
    e->as.call.fn_index = -1;  // -1 = builtin, set by type checker for user functions
    return e;
}

Expr *expr_new_unary(UnaryOp op, Expr *rhs, size_t line) {
    Expr *e = expr_alloc(EX_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.rhs = rhs;
    return e;
}

Expr *expr_new_binary(BinaryOp op, Expr *lhs, Expr *rhs, size_t line) {
    Expr *e = expr_alloc(EX_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.lhs = lhs;
    e->as.binary.rhs = rhs;
    return e;
}

Expr *expr_new_array(Expr **elements, size_t len, size_t line) {
    Expr *e = expr_alloc(EX_ARRAY, line);
    e->as.array.elements = elements;
    e->as.array.len = len;
    return e;
}

Expr *expr_new_index(Expr *array, Expr *index, size_t line) {
    Expr *e = expr_alloc(EX_INDEX, line);
    e->as.index.array = array;
    e->as.index.index = index;
    return e;
}

Expr *expr_new_map(Expr **keys, Expr **values, size_t len, size_t line) {
    Expr *e = expr_alloc(EX_MAP, line);
    e->as.map.keys = keys;
    e->as.map.values = values;
    e->as.map.len = len;
    return e;
}

Expr *expr_new_struct_literal(char *struct_name, char **field_names, Expr **field_values, size_t field_count, size_t line) {
    Expr *e = expr_alloc(EX_STRUCT_LITERAL, line);
    e->as.struct_literal.struct_name = struct_name;
    e->as.struct_literal.field_names = field_names;
    e->as.struct_literal.field_values = field_values;
    e->as.struct_literal.field_count = field_count;
    return e;
}

Expr *expr_new_field_access(Expr *object, char *field_name, size_t line) {
    Expr *e = expr_alloc(EX_FIELD_ACCESS, line);
    e->as.field_access.object = object;
    e->as.field_access.field_name = field_name;
    return e;
}

static Stmt *stmt_alloc(StmtKind k, size_t line) {
    Stmt *s = bp_xmalloc(sizeof(*s));
    memset(s, 0, sizeof(*s));
    s->kind = k;
    s->line = line;
    return s;
}

Stmt *stmt_new_let(char *name, Type t, Expr *init, size_t line) {
    Stmt *s = stmt_alloc(ST_LET, line);
    s->as.let.name = name;
    s->as.let.type = t;
    s->as.let.init = init;
    return s;
}

Stmt *stmt_new_assign(char *name, Expr *value, size_t line) {
    Stmt *s = stmt_alloc(ST_ASSIGN, line);
    s->as.assign.name = name;
    s->as.assign.value = value;
    return s;
}

Stmt *stmt_new_index_assign(Expr *array, Expr *index, Expr *value, size_t line) {
    Stmt *s = stmt_alloc(ST_INDEX_ASSIGN, line);
    s->as.idx_assign.array = array;
    s->as.idx_assign.index = index;
    s->as.idx_assign.value = value;
    return s;
}

Stmt *stmt_new_expr(Expr *e, size_t line) {
    Stmt *s = stmt_alloc(ST_EXPR, line);
    s->as.expr.expr = e;
    return s;
}

Stmt *stmt_new_if(Expr *cond, Stmt **then_s, size_t then_len, Stmt **else_s, size_t else_len, size_t line) {
    Stmt *s = stmt_alloc(ST_IF, line);
    s->as.ifs.cond = cond;
    s->as.ifs.then_stmts = then_s;
    s->as.ifs.then_len = then_len;
    s->as.ifs.else_stmts = else_s;
    s->as.ifs.else_len = else_len;
    return s;
}

Stmt *stmt_new_while(Expr *cond, Stmt **body, size_t body_len, size_t line) {
    Stmt *s = stmt_alloc(ST_WHILE, line);
    s->as.wh.cond = cond;
    s->as.wh.body = body;
    s->as.wh.body_len = body_len;
    return s;
}

Stmt *stmt_new_for(char *var, Expr *start, Expr *end, Stmt **body, size_t body_len, size_t line) {
    Stmt *s = stmt_alloc(ST_FOR, line);
    s->as.forr.var = var;
    s->as.forr.start = start;
    s->as.forr.end = end;
    s->as.forr.body = body;
    s->as.forr.body_len = body_len;
    return s;
}

Stmt *stmt_new_break(size_t line) {
    Stmt *s = stmt_alloc(ST_BREAK, line);
    return s;
}

Stmt *stmt_new_continue(size_t line) {
    Stmt *s = stmt_alloc(ST_CONTINUE, line);
    return s;
}

Stmt *stmt_new_return(Expr *value, size_t line) {
    Stmt *s = stmt_alloc(ST_RETURN, line);
    s->as.ret.value = value;
    return s;
}

Stmt *stmt_new_try(Stmt **try_s, size_t try_len, char *catch_var, Stmt **catch_s, size_t catch_len, Stmt **finally_s, size_t finally_len, size_t line) {
    Stmt *s = stmt_alloc(ST_TRY, line);
    s->as.try_catch.try_stmts = try_s;
    s->as.try_catch.try_len = try_len;
    s->as.try_catch.catch_var = catch_var;
    s->as.try_catch.catch_stmts = catch_s;
    s->as.try_catch.catch_len = catch_len;
    s->as.try_catch.finally_stmts = finally_s;
    s->as.try_catch.finally_len = finally_len;
    return s;
}

Stmt *stmt_new_throw(Expr *value, size_t line) {
    Stmt *s = stmt_alloc(ST_THROW, line);
    s->as.throw.value = value;
    return s;
}

static void expr_free(Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EX_STR: free(e->as.str_val); break;
        case EX_VAR: free(e->as.var_name); break;
        case EX_CALL:
            free(e->as.call.name);
            for (size_t i = 0; i < e->as.call.argc; i++) expr_free(e->as.call.args[i]);
            free(e->as.call.args);
            break;
        case EX_UNARY: expr_free(e->as.unary.rhs); break;
        case EX_BINARY: expr_free(e->as.binary.lhs); expr_free(e->as.binary.rhs); break;
        case EX_ARRAY:
            for (size_t i = 0; i < e->as.array.len; i++) expr_free(e->as.array.elements[i]);
            free(e->as.array.elements);
            break;
        case EX_INDEX:
            expr_free(e->as.index.array);
            expr_free(e->as.index.index);
            break;
        case EX_MAP:
            for (size_t i = 0; i < e->as.map.len; i++) {
                expr_free(e->as.map.keys[i]);
                expr_free(e->as.map.values[i]);
            }
            free(e->as.map.keys);
            free(e->as.map.values);
            break;
        case EX_STRUCT_LITERAL:
            free(e->as.struct_literal.struct_name);
            for (size_t i = 0; i < e->as.struct_literal.field_count; i++) {
                free(e->as.struct_literal.field_names[i]);
                expr_free(e->as.struct_literal.field_values[i]);
            }
            free(e->as.struct_literal.field_names);
            free(e->as.struct_literal.field_values);
            break;
        case EX_FIELD_ACCESS:
            expr_free(e->as.field_access.object);
            free(e->as.field_access.field_name);
            break;
        default: break;
    }
    free(e);
}

static void stmt_free(Stmt *s) {
    if (!s) return;
    switch (s->kind) {
        case ST_LET:
            free(s->as.let.name);
            expr_free(s->as.let.init);
            break;
        case ST_ASSIGN:
            free(s->as.assign.name);
            expr_free(s->as.assign.value);
            break;
        case ST_INDEX_ASSIGN:
            expr_free(s->as.idx_assign.array);
            expr_free(s->as.idx_assign.index);
            expr_free(s->as.idx_assign.value);
            break;
        case ST_EXPR:
            expr_free(s->as.expr.expr);
            break;
        case ST_IF:
            expr_free(s->as.ifs.cond);
            for (size_t i = 0; i < s->as.ifs.then_len; i++) stmt_free(s->as.ifs.then_stmts[i]);
            for (size_t i = 0; i < s->as.ifs.else_len; i++) stmt_free(s->as.ifs.else_stmts[i]);
            free(s->as.ifs.then_stmts);
            free(s->as.ifs.else_stmts);
            break;
        case ST_WHILE:
            expr_free(s->as.wh.cond);
            for (size_t i = 0; i < s->as.wh.body_len; i++) stmt_free(s->as.wh.body[i]);
            free(s->as.wh.body);
            break;
        case ST_FOR:
            free(s->as.forr.var);
            expr_free(s->as.forr.start);
            expr_free(s->as.forr.end);
            for (size_t i = 0; i < s->as.forr.body_len; i++) stmt_free(s->as.forr.body[i]);
            free(s->as.forr.body);
            break;
        case ST_BREAK:
        case ST_CONTINUE:
            break;
        case ST_RETURN:
            expr_free(s->as.ret.value);
            break;
        case ST_TRY:
            for (size_t i = 0; i < s->as.try_catch.try_len; i++) stmt_free(s->as.try_catch.try_stmts[i]);
            free(s->as.try_catch.try_stmts);
            free(s->as.try_catch.catch_var);
            for (size_t i = 0; i < s->as.try_catch.catch_len; i++) stmt_free(s->as.try_catch.catch_stmts[i]);
            free(s->as.try_catch.catch_stmts);
            for (size_t i = 0; i < s->as.try_catch.finally_len; i++) stmt_free(s->as.try_catch.finally_stmts[i]);
            free(s->as.try_catch.finally_stmts);
            break;
        case ST_THROW:
            expr_free(s->as.throw.value);
            break;
    }
    free(s);
}

void module_free(Module *m) {
    if (!m) return;
    for (size_t i = 0; i < m->fnc; i++) {
        Function *f = &m->fns[i];
        free(f->name);
        for (size_t p = 0; p < f->paramc; p++) free(f->params[p].name);
        free(f->params);
        for (size_t s = 0; s < f->body_len; s++) stmt_free(f->body[s]);
        free(f->body);
    }
    free(m->fns);
    m->fns = NULL;
    m->fnc = 0;

    for (size_t i = 0; i < m->structc; i++) {
        StructDef *sd = &m->structs[i];
        free(sd->name);
        for (size_t j = 0; j < sd->field_count; j++) free(sd->field_names[j]);
        free(sd->field_names);
        free(sd->field_types);
    }
    free(m->structs);
    m->structs = NULL;
    m->structc = 0;
}
