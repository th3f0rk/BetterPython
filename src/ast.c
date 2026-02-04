#include "ast.h"
#include "util.h"
#include <string.h>

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

Stmt *stmt_new_return(Expr *value, size_t line) {
    Stmt *s = stmt_alloc(ST_RETURN, line);
    s->as.ret.value = value;
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
        case ST_RETURN:
            expr_free(s->as.ret.value);
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
}
