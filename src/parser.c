#include "parser.h"
#include "lexer.h"
#include "util.h"
#include <string.h>

typedef struct {
    Lexer *lx;
    Token cur;
} Parser;

static void next(Parser *p) { p->cur = lexer_next(p->lx); }

static void expect(Parser *p, TokenKind k, const char *msg) {
    if (p->cur.kind != k) {
        bp_fatal("parse error at %zu:%zu: %s (got %s)", p->cur.line, p->cur.col, msg, token_kind_name(p->cur.kind));
    }
    next(p);
}

static bool accept(Parser *p, TokenKind k) {
    if (p->cur.kind == k) { next(p); return true; }
    return false;
}

static void skip_newlines(Parser *p) {
    while (p->cur.kind == TOK_NEWLINE) next(p);
}

static Type parse_type(Parser *p) {
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected type name at %zu:%zu", p->cur.line, p->cur.col);
    const char *s = p->cur.lexeme;
    size_t n = p->cur.len;

    Type t = type_void();
    if (n == 3 && memcmp(s, "int", 3) == 0) t = type_int();
    else if (n == 4 && memcmp(s, "bool", 4) == 0) t = type_bool();
    else if (n == 3 && memcmp(s, "str", 3) == 0) t = type_str();
    else if (n == 4 && memcmp(s, "void", 4) == 0) t = type_void();
    else bp_fatal("unknown type '%.*s' at %zu:%zu", (int)n, s, p->cur.line, p->cur.col);

    next(p);
    return t;
}

static char *dup_lexeme(Token t) {
    char *s = bp_xmalloc(t.len + 1);
    memcpy(s, t.lexeme, t.len);
    s[t.len] = '\0';
    return s;
}

static Expr *parse_expr(Parser *p);

static Expr *parse_primary(Parser *p) {
    size_t line = p->cur.line;
    if (p->cur.kind == TOK_INT) {
        int64_t v = p->cur.int_val;
        next(p);
        return expr_new_int(v, line);
    }
    if (p->cur.kind == TOK_STR) {
        char *s = bp_xstrdup(p->cur.str_val);
        free((void *)p->cur.str_val);
        next(p);
        return expr_new_str(s, line);
    }
    if (accept(p, TOK_TRUE)) return expr_new_bool(true, line);
    if (accept(p, TOK_FALSE)) return expr_new_bool(false, line);

    if (p->cur.kind == TOK_IDENT) {
        Token id = p->cur;
        next(p);
        if (accept(p, TOK_LPAREN)) {
            Expr **args = NULL;
            size_t argc = 0, cap = 0;
            if (p->cur.kind != TOK_RPAREN) {
                for (;;) {
                    Expr *a = parse_expr(p);
                    if (argc + 1 > cap) { cap = cap ? cap * 2 : 4; args = bp_xrealloc(args, cap * sizeof(*args)); }
                    args[argc++] = a;
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "expected ')'");
            return expr_new_call(dup_lexeme(id), args, argc, line);
        }
        return expr_new_var(dup_lexeme(id), line);
    }

    if (accept(p, TOK_LPAREN)) {
        Expr *e = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')'");
        return e;
    }

    bp_fatal("unexpected token %s at %zu:%zu", token_kind_name(p->cur.kind), p->cur.line, p->cur.col);
    return NULL;
}

static Expr *parse_unary(Parser *p) {
    size_t line = p->cur.line;
    if (accept(p, TOK_MINUS)) return expr_new_unary(UOP_NEG, parse_unary(p), line);
    if (accept(p, TOK_NOT)) return expr_new_unary(UOP_NOT, parse_unary(p), line);
    return parse_primary(p);
}

static BinaryOp bop_from(TokenKind k) {
    switch (k) {
        case TOK_PLUS: return BOP_ADD;
        case TOK_MINUS: return BOP_SUB;
        case TOK_STAR: return BOP_MUL;
        case TOK_SLASH: return BOP_DIV;
        case TOK_PCT: return BOP_MOD;
        case TOK_EQ: return BOP_EQ;
        case TOK_NEQ: return BOP_NEQ;
        case TOK_LT: return BOP_LT;
        case TOK_LTE: return BOP_LTE;
        case TOK_GT: return BOP_GT;
        case TOK_GTE: return BOP_GTE;
        case TOK_AND: return BOP_AND;
        case TOK_OR: return BOP_OR;
        default: break;
    }
    bp_fatal("internal: bad bop");
    return BOP_ADD;
}

static int precedence(TokenKind k) {
    switch (k) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_EQ: case TOK_NEQ: return 3;
        case TOK_LT: case TOK_LTE: case TOK_GT: case TOK_GTE: return 4;
        case TOK_PLUS: case TOK_MINUS: return 5;
        case TOK_STAR: case TOK_SLASH: case TOK_PCT: return 6;
        default: return 0;
    }
}

static Expr *parse_bin_rhs(Parser *p, int min_prec, Expr *lhs) {
    while (1) {
        int prec = precedence(p->cur.kind);
        if (prec < min_prec) break;

        TokenKind opk = p->cur.kind;
        size_t line = p->cur.line;
        next(p);

        Expr *rhs = parse_unary(p);

        int next_prec = precedence(p->cur.kind);
        if (next_prec > prec) rhs = parse_bin_rhs(p, prec + 1, rhs);

        lhs = expr_new_binary(bop_from(opk), lhs, rhs, line);
    }
    return lhs;
}

static Expr *parse_expr(Parser *p) {
    Expr *lhs = parse_unary(p);
    return parse_bin_rhs(p, 1, lhs);
}

static Stmt *parse_stmt(Parser *p);

static Stmt **parse_block(Parser *p, size_t *out_len) {
    expect(p, TOK_NEWLINE, "expected newline before block");
    expect(p, TOK_INDENT, "expected indent");

    Stmt **stmts = NULL;
    size_t len = 0, cap = 0;

    skip_newlines(p);
    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        Stmt *s = parse_stmt(p);
        if (len + 1 > cap) { cap = cap ? cap * 2 : 8; stmts = bp_xrealloc(stmts, cap * sizeof(*stmts)); }
        stmts[len++] = s;
        skip_newlines(p);
    }
    expect(p, TOK_DEDENT, "expected dedent");
    *out_len = len;
    return stmts;
}

static Stmt *parse_if(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_IF, "expected 'if'");
    Expr *cond = parse_expr(p);
    expect(p, TOK_COLON, "expected ':' after if condition");

    size_t then_len = 0, else_len = 0;
    Stmt **then_s = parse_block(p, &then_len);
    Stmt **else_s = NULL;

    // Handle elif as syntactic sugar for else { if ... }
    if (p->cur.kind == TOK_ELIF) {
        // Replace ELIF token with IF so recursive call works
        p->cur.kind = TOK_IF;
        Stmt *elif_stmt = parse_if(p); // Recursively parse
        else_s = bp_xmalloc(sizeof(Stmt*));
        else_s[0] = elif_stmt;
        else_len = 1;
    }
    else if (accept(p, TOK_ELSE)) {
        expect(p, TOK_COLON, "expected ':' after else");
        else_s = parse_block(p, &else_len);
    }

    return stmt_new_if(cond, then_s, then_len, else_s, else_len, line);
}

static Stmt *parse_while(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_WHILE, "expected 'while'");
    Expr *cond = parse_expr(p);
    expect(p, TOK_COLON, "expected ':' after while condition");

    size_t body_len = 0;
    Stmt **body = parse_block(p, &body_len);
    return stmt_new_while(cond, body, body_len, line);
}

static Stmt *parse_stmt(Parser *p) {
    size_t line = p->cur.line;

    if (p->cur.kind == TOK_IF) return parse_if(p);
    if (p->cur.kind == TOK_WHILE) return parse_while(p);

    if (accept(p, TOK_RETURN)) {
        if (p->cur.kind == TOK_NEWLINE) return stmt_new_return(NULL, line);
        Expr *v = parse_expr(p);
        return stmt_new_return(v, line);
    }

    if (accept(p, TOK_LET)) {
        if (p->cur.kind != TOK_IDENT) bp_fatal("expected identifier after let at %zu:%zu", p->cur.line, p->cur.col);
        char *name = dup_lexeme(p->cur);
        next(p);
        expect(p, TOK_COLON, "expected ':' in let");
        Type t = parse_type(p);
        expect(p, TOK_ASSIGN, "expected '=' in let");
        Expr *init = parse_expr(p);
        return stmt_new_let(name, t, init, line);
    }

    if (p->cur.kind == TOK_IDENT) {
        Token id = p->cur;
        Token peek = lexer_peek(p->lx);
        if (peek.kind == TOK_ASSIGN) {
            next(p);
            expect(p, TOK_ASSIGN, "expected '='");
            Expr *v = parse_expr(p);
            return stmt_new_assign(dup_lexeme(id), v, line);
        }
    }

    Expr *e = parse_expr(p);
    return stmt_new_expr(e, line);
}

static Function parse_function(Parser *p) {
    Function f;
    memset(&f, 0, sizeof(f));

    f.line = p->cur.line;
    expect(p, TOK_DEF, "expected 'def'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected function name at %zu:%zu", p->cur.line, p->cur.col);
    f.name = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_LPAREN, "expected '(' after function name");
    Param *params = NULL;
    size_t paramc = 0, cap = 0;

    if (p->cur.kind != TOK_RPAREN) {
        for (;;) {
            if (p->cur.kind != TOK_IDENT) bp_fatal("expected parameter name at %zu:%zu", p->cur.line, p->cur.col);
            char *pname = dup_lexeme(p->cur);
            next(p);
            expect(p, TOK_COLON, "expected ':' in parameter");
            Type pt = parse_type(p);

            if (paramc + 1 > cap) { cap = cap ? cap * 2 : 4; params = bp_xrealloc(params, cap * sizeof(*params)); }
            params[paramc++] = (Param){ .name = pname, .type = pt };

            if (!accept(p, TOK_COMMA)) break;
        }
    }
    expect(p, TOK_RPAREN, "expected ')' after params");
    expect(p, TOK_ARROW, "expected '->' return type");
    f.ret_type = parse_type(p);
    expect(p, TOK_COLON, "expected ':' after signature");

    size_t body_len = 0;
    Stmt **body = parse_block(p, &body_len);

    f.params = params;
    f.paramc = paramc;
    f.body = body;
    f.body_len = body_len;
    return f;
}

Module parse_module(const char *src) {
    Parser p;
    memset(&p, 0, sizeof(p));
    p.lx = lexer_new(src);
    next(&p);

    Module m;
    memset(&m, 0, sizeof(m));

    size_t cap = 0;
    skip_newlines(&p);

    while (p.cur.kind != TOK_EOF) {
        if (p.cur.kind != TOK_DEF) bp_fatal("expected 'def' at top level (line %zu)", p.cur.line);
        Function f = parse_function(&p);

        if (m.fnc + 1 > cap) { cap = cap ? cap * 2 : 8; m.fns = bp_xrealloc(m.fns, cap * sizeof(*m.fns)); }
        m.fns[m.fnc++] = f;

        skip_newlines(&p);
    }

    lexer_free(p.lx);
    return m;
}
