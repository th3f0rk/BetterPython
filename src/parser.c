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
    // Handle array types: [int], [str], etc.
    if (p->cur.kind == TOK_LBRACKET) {
        next(p);  // consume '['
        Type elem = parse_type(p);  // recursively parse element type
        expect(p, TOK_RBRACKET, "expected ']' after array element type");
        Type t;
        t.kind = TY_ARRAY;
        t.elem_type = type_new(elem.kind);
        t.key_type = NULL;
        if (elem.kind == TY_ARRAY && elem.elem_type) {
            t.elem_type->elem_type = elem.elem_type;
        }
        return t;
    }

    // Handle map types: {str: int}, {str: [int]}, etc.
    if (p->cur.kind == TOK_LBRACE) {
        next(p);  // consume '{'
        Type key = parse_type(p);  // parse key type
        expect(p, TOK_COLON, "expected ':' in map type");
        Type value = parse_type(p);  // parse value type
        expect(p, TOK_RBRACE, "expected '}' after map value type");
        Type t;
        t.kind = TY_MAP;
        t.key_type = type_new(key.kind);
        if (key.kind == TY_ARRAY && key.elem_type) {
            t.key_type->elem_type = key.elem_type;
        }
        t.elem_type = type_new(value.kind);
        if (value.kind == TY_ARRAY && value.elem_type) {
            t.elem_type->elem_type = value.elem_type;
        } else if (value.kind == TY_MAP) {
            t.elem_type->key_type = value.key_type;
            t.elem_type->elem_type = value.elem_type;
        }
        return t;
    }

    if (p->cur.kind != TOK_IDENT) bp_fatal("expected type name at %zu:%zu", p->cur.line, p->cur.col);
    const char *s = p->cur.lexeme;
    size_t n = p->cur.len;

    Type t = type_void();
    if (n == 3 && memcmp(s, "int", 3) == 0) t = type_int();
    else if (n == 5 && memcmp(s, "float", 5) == 0) t = type_float();
    else if (n == 4 && memcmp(s, "bool", 4) == 0) t = type_bool();
    else if (n == 3 && memcmp(s, "str", 3) == 0) t = type_str();
    else if (n == 4 && memcmp(s, "void", 4) == 0) t = type_void();
    else {
        // Assume it's a struct type
        t.kind = TY_STRUCT;
        t.elem_type = NULL;
        t.key_type = NULL;
        char *name = bp_xmalloc(n + 1);
        memcpy(name, s, n);
        name[n] = '\0';
        t.struct_name = name;
    }

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

static Expr *parse_postfix(Parser *p, Expr *primary);

static Expr *parse_primary(Parser *p) {
    size_t line = p->cur.line;
    if (p->cur.kind == TOK_INT) {
        int64_t v = p->cur.int_val;
        next(p);
        return parse_postfix(p, expr_new_int(v, line));
    }
    if (p->cur.kind == TOK_FLOAT) {
        double v = p->cur.float_val;
        next(p);
        return parse_postfix(p, expr_new_float(v, line));
    }
    if (p->cur.kind == TOK_STR) {
        char *s = bp_xstrdup(p->cur.str_val);
        free((void *)p->cur.str_val);
        next(p);
        return parse_postfix(p, expr_new_str(s, line));
    }
    if (accept(p, TOK_TRUE)) return parse_postfix(p, expr_new_bool(true, line));
    if (accept(p, TOK_FALSE)) return parse_postfix(p, expr_new_bool(false, line));

    // Array literal: [expr, expr, ...]
    if (p->cur.kind == TOK_LBRACKET) {
        next(p);  // consume '['
        Expr **elements = NULL;
        size_t len = 0, cap = 0;
        if (p->cur.kind != TOK_RBRACKET) {
            for (;;) {
                Expr *e = parse_expr(p);
                if (len + 1 > cap) { cap = cap ? cap * 2 : 8; elements = bp_xrealloc(elements, cap * sizeof(*elements)); }
                elements[len++] = e;
                if (!accept(p, TOK_COMMA)) break;
            }
        }
        expect(p, TOK_RBRACKET, "expected ']' after array elements");
        return parse_postfix(p, expr_new_array(elements, len, line));
    }

    // Map literal: {key: value, key2: value2, ...}
    if (p->cur.kind == TOK_LBRACE) {
        next(p);  // consume '{'
        Expr **keys = NULL;
        Expr **values = NULL;
        size_t len = 0, cap = 0;
        if (p->cur.kind != TOK_RBRACE) {
            for (;;) {
                Expr *key = parse_expr(p);
                expect(p, TOK_COLON, "expected ':' after map key");
                Expr *value = parse_expr(p);
                if (len + 1 > cap) {
                    cap = cap ? cap * 2 : 8;
                    keys = bp_xrealloc(keys, cap * sizeof(*keys));
                    values = bp_xrealloc(values, cap * sizeof(*values));
                }
                keys[len] = key;
                values[len] = value;
                len++;
                if (!accept(p, TOK_COMMA)) break;
            }
        }
        expect(p, TOK_RBRACE, "expected '}' after map elements");
        return parse_postfix(p, expr_new_map(keys, values, len, line));
    }

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
            return parse_postfix(p, expr_new_call(dup_lexeme(id), args, argc, line));
        }
        // Struct literal: Name{field: value, ...}
        if (accept(p, TOK_LBRACE)) {
            char **field_names = NULL;
            Expr **field_values = NULL;
            size_t field_count = 0, cap = 0;
            if (p->cur.kind != TOK_RBRACE) {
                for (;;) {
                    if (p->cur.kind != TOK_IDENT) {
                        bp_fatal("expected field name in struct literal at %zu:%zu", p->cur.line, p->cur.col);
                    }
                    char *fname = dup_lexeme(p->cur);
                    next(p);
                    expect(p, TOK_COLON, "expected ':' after field name");
                    Expr *fval = parse_expr(p);
                    if (field_count + 1 > cap) {
                        cap = cap ? cap * 2 : 4;
                        field_names = bp_xrealloc(field_names, cap * sizeof(*field_names));
                        field_values = bp_xrealloc(field_values, cap * sizeof(*field_values));
                    }
                    field_names[field_count] = fname;
                    field_values[field_count] = fval;
                    field_count++;
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RBRACE, "expected '}' after struct literal");
            return parse_postfix(p, expr_new_struct_literal(dup_lexeme(id), field_names, field_values, field_count, line));
        }
        return parse_postfix(p, expr_new_var(dup_lexeme(id), line));
    }

    if (accept(p, TOK_LPAREN)) {
        Expr *e = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')'");
        return parse_postfix(p, e);
    }

    bp_fatal("unexpected token %s at %zu:%zu", token_kind_name(p->cur.kind), p->cur.line, p->cur.col);
    return NULL;
}

// Handle postfix operations like array indexing: arr[i][j] and field access: obj.field
static Expr *parse_postfix(Parser *p, Expr *primary) {
    for (;;) {
        size_t line = p->cur.line;
        if (p->cur.kind == TOK_LBRACKET) {
            next(p);  // consume '['
            Expr *index = parse_expr(p);
            expect(p, TOK_RBRACKET, "expected ']' after index");
            primary = expr_new_index(primary, index, line);
        } else if (p->cur.kind == TOK_DOT) {
            next(p);  // consume '.'
            if (p->cur.kind != TOK_IDENT) {
                bp_fatal("expected field name after '.' at %zu:%zu", p->cur.line, p->cur.col);
            }
            char *field_name = dup_lexeme(p->cur);
            next(p);
            primary = expr_new_field_access(primary, field_name, line);
        } else {
            break;
        }
    }
    return primary;
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

// Parses: for <var> in range(<start>, <end>):
static Stmt *parse_for(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_FOR, "expected 'for'");

    if (p->cur.kind != TOK_IDENT) bp_fatal("expected identifier after 'for' at %zu:%zu", p->cur.line, p->cur.col);
    char *var = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_IN, "expected 'in' after for variable");

    // Expect 'range' identifier
    if (p->cur.kind != TOK_IDENT || p->cur.len != 5 || memcmp(p->cur.lexeme, "range", 5) != 0) {
        bp_fatal("expected 'range' after 'in' at %zu:%zu", p->cur.line, p->cur.col);
    }
    next(p);

    expect(p, TOK_LPAREN, "expected '(' after range");
    Expr *start = parse_expr(p);
    expect(p, TOK_COMMA, "expected ',' in range");
    Expr *end = parse_expr(p);
    expect(p, TOK_RPAREN, "expected ')' after range");
    expect(p, TOK_COLON, "expected ':' after for header");

    size_t body_len = 0;
    Stmt **body = parse_block(p, &body_len);
    return stmt_new_for(var, start, end, body, body_len, line);
}

static Stmt *parse_try(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_TRY, "expected 'try'");
    expect(p, TOK_COLON, "expected ':' after try");
    expect(p, TOK_NEWLINE, "expected newline after try:");
    expect(p, TOK_INDENT, "expected indent");

    // Parse try body
    Stmt **try_stmts = NULL;
    size_t try_len = 0, try_cap = 0;
    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT) break;
        Stmt *s = parse_stmt(p);
        if (try_len + 1 > try_cap) { try_cap = try_cap ? try_cap * 2 : 8; try_stmts = bp_xrealloc(try_stmts, try_cap * sizeof(*try_stmts)); }
        try_stmts[try_len++] = s;
        skip_newlines(p);
    }
    expect(p, TOK_DEDENT, "expected dedent after try body");
    skip_newlines(p);

    // Parse catch block
    char *catch_var = NULL;
    Stmt **catch_stmts = NULL;
    size_t catch_len = 0, catch_cap = 0;
    if (p->cur.kind == TOK_CATCH) {
        next(p);  // consume 'catch'
        if (p->cur.kind == TOK_IDENT) {
            catch_var = dup_lexeme(p->cur);
            next(p);
        }
        expect(p, TOK_COLON, "expected ':' after catch");
        expect(p, TOK_NEWLINE, "expected newline after catch:");
        expect(p, TOK_INDENT, "expected indent");

        while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
            skip_newlines(p);
            if (p->cur.kind == TOK_DEDENT) break;
            Stmt *s = parse_stmt(p);
            if (catch_len + 1 > catch_cap) { catch_cap = catch_cap ? catch_cap * 2 : 8; catch_stmts = bp_xrealloc(catch_stmts, catch_cap * sizeof(*catch_stmts)); }
            catch_stmts[catch_len++] = s;
            skip_newlines(p);
        }
        expect(p, TOK_DEDENT, "expected dedent after catch body");
        skip_newlines(p);
    }

    // Parse optional finally block
    Stmt **finally_stmts = NULL;
    size_t finally_len = 0, finally_cap = 0;
    if (p->cur.kind == TOK_FINALLY) {
        next(p);  // consume 'finally'
        expect(p, TOK_COLON, "expected ':' after finally");
        expect(p, TOK_NEWLINE, "expected newline after finally:");
        expect(p, TOK_INDENT, "expected indent");

        while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
            skip_newlines(p);
            if (p->cur.kind == TOK_DEDENT) break;
            Stmt *s = parse_stmt(p);
            if (finally_len + 1 > finally_cap) { finally_cap = finally_cap ? finally_cap * 2 : 8; finally_stmts = bp_xrealloc(finally_stmts, finally_cap * sizeof(*finally_stmts)); }
            finally_stmts[finally_len++] = s;
            skip_newlines(p);
        }
        expect(p, TOK_DEDENT, "expected dedent after finally body");
    }

    return stmt_new_try(try_stmts, try_len, catch_var, catch_stmts, catch_len, finally_stmts, finally_len, line);
}

static Stmt *parse_stmt(Parser *p) {
    size_t line = p->cur.line;

    if (p->cur.kind == TOK_IF) return parse_if(p);
    if (p->cur.kind == TOK_WHILE) return parse_while(p);
    if (p->cur.kind == TOK_FOR) return parse_for(p);
    if (p->cur.kind == TOK_TRY) return parse_try(p);

    if (accept(p, TOK_THROW)) {
        Expr *v = parse_expr(p);
        return stmt_new_throw(v, line);
    }

    if (accept(p, TOK_BREAK)) {
        return stmt_new_break(line);
    }

    if (accept(p, TOK_CONTINUE)) {
        return stmt_new_continue(line);
    }

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
            // Simple variable assignment: x = value
            next(p);
            expect(p, TOK_ASSIGN, "expected '='");
            Expr *v = parse_expr(p);
            return stmt_new_assign(dup_lexeme(id), v, line);
        }
        if (peek.kind == TOK_LBRACKET) {
            // Possible array index assignment: arr[idx] = value
            // Parse the full expression first
            Expr *arr_expr = parse_expr(p);
            if (arr_expr->kind == EX_INDEX && p->cur.kind == TOK_ASSIGN) {
                next(p);  // consume '='
                Expr *v = parse_expr(p);
                // Extract array and index from the index expression
                Expr *arr = arr_expr->as.index.array;
                Expr *idx = arr_expr->as.index.index;
                // Clear the container so we don't double-free
                arr_expr->as.index.array = NULL;
                arr_expr->as.index.index = NULL;
                free(arr_expr);  // Free just the wrapper expression
                return stmt_new_index_assign(arr, idx, v, line);
            }
            // Not an assignment, treat as expression statement
            return stmt_new_expr(arr_expr, line);
        }
    }

    Expr *e = parse_expr(p);
    return stmt_new_expr(e, line);
}

static StructDef parse_struct(Parser *p) {
    StructDef sd;
    memset(&sd, 0, sizeof(sd));

    sd.line = p->cur.line;
    expect(p, TOK_STRUCT, "expected 'struct'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected struct name at %zu:%zu", p->cur.line, p->cur.col);
    sd.name = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_COLON, "expected ':' after struct name");
    expect(p, TOK_NEWLINE, "expected newline after ':'");
    expect(p, TOK_INDENT, "expected indent after struct header");

    char **field_names = NULL;
    Type *field_types = NULL;
    size_t field_count = 0, cap = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        if (p->cur.kind != TOK_IDENT) {
            bp_fatal("expected field name in struct at %zu:%zu", p->cur.line, p->cur.col);
        }
        char *fname = dup_lexeme(p->cur);
        next(p);
        expect(p, TOK_COLON, "expected ':' after field name");
        Type ftype = parse_type(p);

        if (field_count + 1 > cap) {
            cap = cap ? cap * 2 : 4;
            field_names = bp_xrealloc(field_names, cap * sizeof(*field_names));
            field_types = bp_xrealloc(field_types, cap * sizeof(*field_types));
        }
        field_names[field_count] = fname;
        field_types[field_count] = ftype;
        field_count++;

        skip_newlines(p);
    }

    if (p->cur.kind == TOK_DEDENT) next(p);

    sd.field_names = field_names;
    sd.field_types = field_types;
    sd.field_count = field_count;
    return sd;
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

    size_t fn_cap = 0, st_cap = 0;
    skip_newlines(&p);

    while (p.cur.kind != TOK_EOF) {
        if (p.cur.kind == TOK_DEF) {
            Function f = parse_function(&p);
            if (m.fnc + 1 > fn_cap) { fn_cap = fn_cap ? fn_cap * 2 : 8; m.fns = bp_xrealloc(m.fns, fn_cap * sizeof(*m.fns)); }
            m.fns[m.fnc++] = f;
        } else if (p.cur.kind == TOK_STRUCT) {
            StructDef sd = parse_struct(&p);
            if (m.structc + 1 > st_cap) { st_cap = st_cap ? st_cap * 2 : 8; m.structs = bp_xrealloc(m.structs, st_cap * sizeof(*m.structs)); }
            m.structs[m.structc++] = sd;
        } else {
            bp_fatal("expected 'def' or 'struct' at top level (line %zu)", p.cur.line);
        }

        skip_newlines(&p);
    }

    lexer_free(p.lx);
    return m;
}
