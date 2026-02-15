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

// Deep-copy a Type into a heap-allocated Type*
static Type *type_clone(Type src) {
    Type *t = type_new(src.kind);
    if (src.struct_name) t->struct_name = bp_xstrdup(src.struct_name);
    if (src.elem_type) { t->elem_type = bp_xmalloc(sizeof(Type)); *t->elem_type = *src.elem_type; if (src.elem_type->struct_name) t->elem_type->struct_name = bp_xstrdup(src.elem_type->struct_name); }
    if (src.key_type) { t->key_type = bp_xmalloc(sizeof(Type)); *t->key_type = *src.key_type; if (src.key_type->struct_name) t->key_type->struct_name = bp_xstrdup(src.key_type->struct_name); }
    if (src.return_type) { t->return_type = bp_xmalloc(sizeof(Type)); *t->return_type = *src.return_type; }
    t->param_types = src.param_types;
    t->param_count = src.param_count;
    t->tuple_types = src.tuple_types;
    t->tuple_len = src.tuple_len;
    return t;
}

static Type parse_type(Parser *p) {
    // Handle array types: [int], [str], etc.
    if (p->cur.kind == TOK_LBRACKET) {
        next(p);  // consume '['
        Type elem = parse_type(p);  // recursively parse element type
        expect(p, TOK_RBRACKET, "expected ']' after array element type");
        Type t;
        memset(&t, 0, sizeof(t));
        t.kind = TY_ARRAY;
        t.elem_type = type_clone(elem);
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
        memset(&t, 0, sizeof(t));
        t.kind = TY_MAP;
        t.key_type = type_clone(key);
        t.elem_type = type_clone(value);
        return t;
    }

    // Handle tuple types: (int, str, bool)
    if (p->cur.kind == TOK_LPAREN) {
        next(p);  // consume '('
        Type **tuple_types = NULL;
        size_t tuple_len = 0, cap = 0;
        if (p->cur.kind != TOK_RPAREN) {
            for (;;) {
                Type elem = parse_type(p);
                if (tuple_len + 1 > cap) { cap = cap ? cap * 2 : 4; tuple_types = bp_xrealloc(tuple_types, cap * sizeof(*tuple_types)); }
                Type *elem_copy = type_new(elem.kind);
                *elem_copy = elem;
                tuple_types[tuple_len++] = elem_copy;
                if (!accept(p, TOK_COMMA)) break;
            }
        }
        expect(p, TOK_RPAREN, "expected ')' after tuple types");
        return *type_tuple(tuple_types, tuple_len);
    }

    // Handle function types: fn(int, str) -> bool
    if (p->cur.kind == TOK_FN) {
        next(p);  // consume 'fn'
        expect(p, TOK_LPAREN, "expected '(' after fn");
        Type **param_types = NULL;
        size_t param_count = 0, cap = 0;
        if (p->cur.kind != TOK_RPAREN) {
            for (;;) {
                Type param = parse_type(p);
                if (param_count + 1 > cap) { cap = cap ? cap * 2 : 4; param_types = bp_xrealloc(param_types, cap * sizeof(*param_types)); }
                Type *param_copy = type_new(param.kind);
                *param_copy = param;
                param_types[param_count++] = param_copy;
                if (!accept(p, TOK_COMMA)) break;
            }
        }
        expect(p, TOK_RPAREN, "expected ')' after fn params");
        expect(p, TOK_ARROW, "expected '->' after fn params");
        Type ret = parse_type(p);
        Type *ret_copy = type_new(ret.kind);
        *ret_copy = ret;
        return *type_func(param_types, param_count, ret_copy);
    }

    // Handle ptr keyword type
    if (p->cur.kind == TOK_PTR) {
        Type t;
        t.kind = TY_PTR;
        t.elem_type = NULL;
        t.key_type = NULL;
        t.struct_name = NULL;
        t.tuple_types = NULL;
        t.tuple_len = 0;
        t.param_types = NULL;
        t.param_count = 0;
        t.return_type = NULL;
        next(p);
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
    // Fixed-width signed integers
    else if (n == 2 && memcmp(s, "i8", 2) == 0) t = type_i8();
    else if (n == 3 && memcmp(s, "i16", 3) == 0) t = type_i16();
    else if (n == 3 && memcmp(s, "i32", 3) == 0) t = type_i32();
    else if (n == 3 && memcmp(s, "i64", 3) == 0) t = type_i64();
    // Fixed-width unsigned integers
    else if (n == 2 && memcmp(s, "u8", 2) == 0) t = type_u8();
    else if (n == 3 && memcmp(s, "u16", 3) == 0) t = type_u16();
    else if (n == 3 && memcmp(s, "u32", 3) == 0) t = type_u32();
    else if (n == 3 && memcmp(s, "u64", 3) == 0) t = type_u64();
    else {
        // Assume it's a struct or enum type
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
    if (accept(p, TOK_NULL)) return parse_postfix(p, expr_new_null(line));

    // Function reference: &func_name
    if (p->cur.kind == TOK_AMP) {
        next(p);  // consume '&'
        if (p->cur.kind != TOK_IDENT) bp_fatal("expected function name after '&' at %zu:%zu", p->cur.line, p->cur.col);
        char *name = dup_lexeme(p->cur);
        next(p);
        return expr_new_func_ref(name, line);
    }

    // self keyword
    if (accept(p, TOK_SELF)) {
        return parse_postfix(p, expr_new_var(bp_xstrdup("self"), line));
    }

    // new expression: new ClassName(args)
    if (accept(p, TOK_NEW)) {
        if (p->cur.kind != TOK_IDENT) bp_fatal("expected class name after 'new' at %zu:%zu", p->cur.line, p->cur.col);
        char *class_name = dup_lexeme(p->cur);
        next(p);
        expect(p, TOK_LPAREN, "expected '(' after class name");
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
        expect(p, TOK_RPAREN, "expected ')' after constructor args");
        return parse_postfix(p, expr_new_new(class_name, args, argc, line));
    }

    // super expression: super.method(args) or super().__init__(args)
    if (accept(p, TOK_SUPER)) {
        if (accept(p, TOK_LPAREN)) {
            // super() call - shorthand for parent constructor
            expect(p, TOK_RPAREN, "expected ')' after super()");
            return parse_postfix(p, expr_new_super_call(NULL, NULL, 0, line));
        }
        if (accept(p, TOK_DOT)) {
            if (p->cur.kind != TOK_IDENT) bp_fatal("expected method name after 'super.' at %zu:%zu", p->cur.line, p->cur.col);
            char *method_name = dup_lexeme(p->cur);
            next(p);
            expect(p, TOK_LPAREN, "expected '(' after super.method");
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
            expect(p, TOK_RPAREN, "expected ')' after super.method args");
            return parse_postfix(p, expr_new_super_call(method_name, args, argc, line));
        }
        bp_fatal("expected '.' or '()' after 'super' at %zu:%zu", p->cur.line, p->cur.col);
    }

    // F-string: f"Hello {name}!" - parse with expression interpolation
    if (p->cur.kind == TOK_FSTRING) {
        char *raw = bp_xstrdup(p->cur.str_val);
        free((void *)p->cur.str_val);
        next(p);

        Expr **parts = NULL;
        size_t partc = 0, cap = 0;
        char *s = raw;

        while (*s) {
            // Find next '{'
            char *brace = NULL;
            for (char *c = s; *c; c++) {
                if (*c == '{') { brace = c; break; }
            }

            if (!brace) {
                // Rest is literal string
                if (partc + 1 > cap) { cap = cap ? cap * 2 : 8; parts = bp_xrealloc(parts, cap * sizeof(Expr*)); }
                parts[partc++] = expr_new_str(bp_xstrdup(s), line);
                break;
            }

            // Add literal part before '{'
            if (brace > s) {
                char saved = *brace;
                *brace = '\0';
                if (partc + 1 > cap) { cap = cap ? cap * 2 : 8; parts = bp_xrealloc(parts, cap * sizeof(Expr*)); }
                parts[partc++] = expr_new_str(bp_xstrdup(s), line);
                *brace = saved;
            }

            // Find matching '}' (handle nested braces for dict literals etc.)
            int depth = 1;
            char *end = brace + 1;
            while (*end && depth > 0) {
                if (*end == '{') depth++;
                else if (*end == '}') depth--;
                if (depth > 0) end++;
            }
            if (depth != 0) bp_fatal("unterminated {} in f-string at line %zu", line);

            // Extract expression text between { and }
            size_t expr_len = (size_t)(end - (brace + 1));
            char *expr_text = bp_xmalloc(expr_len + 1);
            memcpy(expr_text, brace + 1, expr_len);
            expr_text[expr_len] = '\0';

            // Parse expression with a sub-lexer
            Lexer *saved_lx = p->lx;
            Token saved_cur = p->cur;

            Lexer *sub_lx = lexer_new(expr_text);
            p->lx = sub_lx;
            next(p);  // load first token from sub-lexer

            Expr *expr = parse_expr(p);

            lexer_free(sub_lx);
            free(expr_text);

            // Restore parser state
            p->lx = saved_lx;
            p->cur = saved_cur;

            if (partc + 1 > cap) { cap = cap ? cap * 2 : 8; parts = bp_xrealloc(parts, cap * sizeof(Expr*)); }
            parts[partc++] = expr;

            s = end + 1;
        }

        free(raw);

        // Handle empty f-string
        if (partc == 0) {
            parts = bp_xmalloc(sizeof(Expr*));
            parts[0] = expr_new_str(bp_xstrdup(""), line);
            partc = 1;
        }

        return parse_postfix(p, expr_new_fstring(parts, partc, line));
    }

    // Lambda: fn(x: int) -> int: x + 1
    if (p->cur.kind == TOK_FN) {
        next(p);  // consume 'fn'
        expect(p, TOK_LPAREN, "expected '(' after fn");

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
        expect(p, TOK_RPAREN, "expected ')' after lambda params");
        expect(p, TOK_ARROW, "expected '->' for lambda return type");
        Type ret_type = parse_type(p);
        expect(p, TOK_COLON, "expected ':' before lambda body");
        Expr *body = parse_expr(p);
        return parse_postfix(p, expr_new_lambda(params, paramc, body, ret_type, line));
    }

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

    // Parenthesized expression or tuple: (expr) or (expr1, expr2, ...)
    if (accept(p, TOK_LPAREN)) {
        if (p->cur.kind == TOK_RPAREN) {
            // Empty tuple: ()
            next(p);
            return parse_postfix(p, expr_new_tuple(NULL, 0, line));
        }
        Expr *first = parse_expr(p);
        if (accept(p, TOK_COMMA)) {
            // It's a tuple
            Expr **elements = NULL;
            size_t len = 0, cap = 0;
            // Add first element
            if (len + 1 > cap) { cap = cap ? cap * 2 : 4; elements = bp_xrealloc(elements, cap * sizeof(*elements)); }
            elements[len++] = first;
            // Add remaining elements
            if (p->cur.kind != TOK_RPAREN) {
                for (;;) {
                    Expr *e = parse_expr(p);
                    if (len + 1 > cap) { cap = cap ? cap * 2 : 4; elements = bp_xrealloc(elements, cap * sizeof(*elements)); }
                    elements[len++] = e;
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "expected ')' after tuple elements");
            return parse_postfix(p, expr_new_tuple(elements, len, line));
        }
        // Just a parenthesized expression
        expect(p, TOK_RPAREN, "expected ')'");
        return parse_postfix(p, first);
    }

    bp_fatal("unexpected token %s at %zu:%zu", token_kind_name(p->cur.kind), p->cur.line, p->cur.col);
    return NULL;
}

// Handle postfix operations like array indexing: arr[i][j], field access: obj.field, and method calls: obj.method()
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
            // Check if it's a method call
            if (p->cur.kind == TOK_LPAREN) {
                next(p);  // consume '('
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
                expect(p, TOK_RPAREN, "expected ')' after method arguments");
                primary = expr_new_method_call(primary, field_name, args, argc, line);
            } else {
                primary = expr_new_field_access(primary, field_name, line);
            }
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
    if (accept(p, TOK_TILDE)) return expr_new_unary(UOP_BIT_NOT, parse_unary(p), line);
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
        case TOK_AMP: return BOP_BIT_AND;
        case TOK_PIPE: return BOP_BIT_OR;
        case TOK_CARET: return BOP_BIT_XOR;
        case TOK_SHL: return BOP_BIT_SHL;
        case TOK_SHR: return BOP_BIT_SHR;
        default: break;
    }
    bp_fatal("internal: bad bop");
    return BOP_ADD;
}

static int precedence(TokenKind k) {
    switch (k) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_PIPE: return 3;       // bitwise OR
        case TOK_CARET: return 4;      // bitwise XOR
        case TOK_AMP: return 5;        // bitwise AND
        case TOK_EQ: case TOK_NEQ: return 6;
        case TOK_LT: case TOK_LTE: case TOK_GT: case TOK_GTE: return 7;
        case TOK_SHL: case TOK_SHR: return 8;   // shifts
        case TOK_PLUS: case TOK_MINUS: return 9;
        case TOK_STAR: case TOK_SLASH: case TOK_PCT: return 10;
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
//     or: for <var> in <collection>:
static Stmt *parse_for(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_FOR, "expected 'for'");

    if (p->cur.kind != TOK_IDENT) bp_fatal("expected identifier after 'for' at %zu:%zu", p->cur.line, p->cur.col);
    char *var = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_IN, "expected 'in' after for variable");

    // Check if it's range(start, end) or a collection expression
    if (p->cur.kind == TOK_IDENT && p->cur.len == 5 && memcmp(p->cur.lexeme, "range", 5) == 0) {
        // range-based for loop: for i in range(start, end):
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

    // Collection-based for loop: for x in <expr>:
    Expr *collection = parse_expr(p);
    expect(p, TOK_COLON, "expected ':' after for-in collection");

    size_t body_len = 0;
    Stmt **body = parse_block(p, &body_len);
    return stmt_new_for_in(var, collection, body, body_len, line);
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

static Stmt *parse_match(Parser *p) {
    size_t line = p->cur.line;
    expect(p, TOK_MATCH, "expected 'match'");
    Expr *expr = parse_expr(p);
    expect(p, TOK_COLON, "expected ':' after match expression");
    expect(p, TOK_NEWLINE, "expected newline after match:");
    expect(p, TOK_INDENT, "expected indent after match:");

    Expr **case_values = NULL;
    Stmt ***case_bodies = NULL;
    size_t *case_body_lens = NULL;
    size_t case_count = 0, case_cap = 0;
    bool has_default = false;
    size_t default_idx = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        Expr *case_val = NULL;
        if (accept(p, TOK_DEFAULT)) {
            if (has_default) bp_fatal("duplicate default case in match at %zu:%zu", p->cur.line, p->cur.col);
            has_default = true;
            default_idx = case_count;
        } else {
            expect(p, TOK_CASE, "expected 'case' or 'default' in match block");
            case_val = parse_expr(p);
        }
        expect(p, TOK_COLON, "expected ':' after case value");

        size_t body_len = 0;
        Stmt **body = parse_block(p, &body_len);

        if (case_count + 1 > case_cap) {
            case_cap = case_cap ? case_cap * 2 : 8;
            case_values = bp_xrealloc(case_values, case_cap * sizeof(*case_values));
            case_bodies = bp_xrealloc(case_bodies, case_cap * sizeof(*case_bodies));
            case_body_lens = bp_xrealloc(case_body_lens, case_cap * sizeof(*case_body_lens));
        }
        case_values[case_count] = case_val;
        case_bodies[case_count] = body;
        case_body_lens[case_count] = body_len;
        case_count++;

        skip_newlines(p);
    }

    if (p->cur.kind == TOK_DEDENT) next(p);

    return stmt_new_match(expr, case_values, case_bodies, case_body_lens, case_count, has_default, default_idx, line);
}

static Stmt *parse_stmt(Parser *p) {
    size_t line = p->cur.line;

    if (p->cur.kind == TOK_MATCH) return parse_match(p);
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
    // Check for field assignment: expr.field = value
    if (p->cur.kind == TOK_ASSIGN && e->kind == EX_FIELD_ACCESS) {
        next(p);  // consume '='
        Expr *value = parse_expr(p);
        return stmt_new_field_assign(e, value, line);
    }
    return stmt_new_expr(e, line);
}

static StructDef parse_struct(Parser *p, bool is_packed) {
    StructDef sd;
    memset(&sd, 0, sizeof(sd));

    sd.line = p->cur.line;
    sd.is_packed = is_packed;
    expect(p, TOK_STRUCT, "expected 'struct'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected struct name at %zu:%zu", p->cur.line, p->cur.col);
    sd.name = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_COLON, "expected ':' after struct name");
    expect(p, TOK_NEWLINE, "expected newline after ':'");
    expect(p, TOK_INDENT, "expected indent after struct header");

    char **field_names = NULL;
    Type *field_types = NULL;
    size_t field_count = 0, field_cap = 0;

    MethodDef *methods = NULL;
    size_t method_count = 0, method_cap = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        // Check for method definition (def inside struct)
        if (p->cur.kind == TOK_DEF) {
            MethodDef md;
            memset(&md, 0, sizeof(md));
            md.line = p->cur.line;
            next(p);  // consume 'def'
            if (p->cur.kind != TOK_IDENT) bp_fatal("expected method name at %zu:%zu", p->cur.line, p->cur.col);
            md.name = dup_lexeme(p->cur);
            next(p);

            expect(p, TOK_LPAREN, "expected '(' after method name");

            Param *params = NULL;
            size_t paramc = 0, param_cap = 0;

            // First param should be 'self' for methods
            if (p->cur.kind != TOK_RPAREN) {
                for (;;) {
                    if (p->cur.kind == TOK_SELF) {
                        next(p);
                        // self doesn't need a type annotation - it's implicit
                        Type self_type;
                        memset(&self_type, 0, sizeof(self_type));
                        self_type.kind = TY_STRUCT;
                        self_type.struct_name = bp_xstrdup(sd.name);
                        if (paramc + 1 > param_cap) { param_cap = param_cap ? param_cap * 2 : 4; params = bp_xrealloc(params, param_cap * sizeof(*params)); }
                        params[paramc++] = (Param){ .name = bp_xstrdup("self"), .type = self_type };
                    } else {
                        if (p->cur.kind != TOK_IDENT) bp_fatal("expected parameter name at %zu:%zu", p->cur.line, p->cur.col);
                        char *pname = dup_lexeme(p->cur);
                        next(p);
                        expect(p, TOK_COLON, "expected ':' in parameter");
                        Type pt = parse_type(p);
                        if (paramc + 1 > param_cap) { param_cap = param_cap ? param_cap * 2 : 4; params = bp_xrealloc(params, param_cap * sizeof(*params)); }
                        params[paramc++] = (Param){ .name = pname, .type = pt };
                    }
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "expected ')' after method params");
            expect(p, TOK_ARROW, "expected '->' return type");
            md.ret_type = parse_type(p);
            expect(p, TOK_COLON, "expected ':' after method signature");

            size_t body_len = 0;
            Stmt **body = parse_block(p, &body_len);

            md.params = params;
            md.paramc = paramc;
            md.body = body;
            md.body_len = body_len;

            if (method_count + 1 > method_cap) {
                method_cap = method_cap ? method_cap * 2 : 4;
                methods = bp_xrealloc(methods, method_cap * sizeof(*methods));
            }
            methods[method_count++] = md;
        } else if (p->cur.kind == TOK_IDENT) {
            // Field definition
            char *fname = dup_lexeme(p->cur);
            next(p);
            expect(p, TOK_COLON, "expected ':' after field name");
            Type ftype = parse_type(p);

            if (field_count + 1 > field_cap) {
                field_cap = field_cap ? field_cap * 2 : 4;
                field_names = bp_xrealloc(field_names, field_cap * sizeof(*field_names));
                field_types = bp_xrealloc(field_types, field_cap * sizeof(*field_types));
            }
            field_names[field_count] = fname;
            field_types[field_count] = ftype;
            field_count++;
        } else {
            bp_fatal("expected field or method definition in struct at %zu:%zu", p->cur.line, p->cur.col);
        }

        skip_newlines(p);
    }

    if (p->cur.kind == TOK_DEDENT) next(p);

    sd.field_names = field_names;
    sd.field_types = field_types;
    sd.field_count = field_count;
    sd.methods = methods;
    sd.method_count = method_count;
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

static EnumDef parse_enum(Parser *p) {
    EnumDef ed;
    memset(&ed, 0, sizeof(ed));

    ed.line = p->cur.line;
    expect(p, TOK_ENUM, "expected 'enum'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected enum name at %zu:%zu", p->cur.line, p->cur.col);
    ed.name = dup_lexeme(p->cur);
    next(p);

    expect(p, TOK_COLON, "expected ':' after enum name");
    expect(p, TOK_NEWLINE, "expected newline after ':'");
    expect(p, TOK_INDENT, "expected indent after enum header");

    char **variant_names = NULL;
    int64_t *variant_values = NULL;
    size_t variant_count = 0, cap = 0;
    int64_t next_value = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        if (p->cur.kind != TOK_IDENT) {
            bp_fatal("expected variant name in enum at %zu:%zu", p->cur.line, p->cur.col);
        }
        char *vname = dup_lexeme(p->cur);
        next(p);

        int64_t value = next_value;
        if (accept(p, TOK_ASSIGN)) {
            if (p->cur.kind != TOK_INT) {
                bp_fatal("expected integer value for enum variant at %zu:%zu", p->cur.line, p->cur.col);
            }
            value = p->cur.int_val;
            next(p);
        }
        next_value = value + 1;

        if (variant_count + 1 > cap) {
            cap = cap ? cap * 2 : 4;
            variant_names = bp_xrealloc(variant_names, cap * sizeof(*variant_names));
            variant_values = bp_xrealloc(variant_values, cap * sizeof(*variant_values));
        }
        variant_names[variant_count] = vname;
        variant_values[variant_count] = value;
        variant_count++;

        skip_newlines(p);
    }

    if (p->cur.kind == TOK_DEDENT) next(p);

    ed.variant_names = variant_names;
    ed.variant_values = variant_values;
    ed.variant_count = variant_count;
    return ed;
}

static ImportDef parse_import(Parser *p) {
    ImportDef id;
    memset(&id, 0, sizeof(id));

    id.line = p->cur.line;
    expect(p, TOK_IMPORT, "expected 'import'");

    if (p->cur.kind != TOK_IDENT) bp_fatal("expected module name at %zu:%zu", p->cur.line, p->cur.col);
    id.module_name = dup_lexeme(p->cur);
    next(p);

    // Optional alias: import foo as bar
    if (accept(p, TOK_AS)) {
        if (p->cur.kind != TOK_IDENT) bp_fatal("expected alias name at %zu:%zu", p->cur.line, p->cur.col);
        id.alias = dup_lexeme(p->cur);
        next(p);
    }

    // Optional selective import: import foo (bar, baz)
    if (accept(p, TOK_LPAREN)) {
        char **names = NULL;
        size_t count = 0, cap = 0;
        if (p->cur.kind != TOK_RPAREN) {
            for (;;) {
                if (p->cur.kind != TOK_IDENT) bp_fatal("expected import name at %zu:%zu", p->cur.line, p->cur.col);
                char *name = dup_lexeme(p->cur);
                next(p);
                if (count + 1 > cap) { cap = cap ? cap * 2 : 4; names = bp_xrealloc(names, cap * sizeof(*names)); }
                names[count++] = name;
                if (!accept(p, TOK_COMMA)) break;
            }
        }
        expect(p, TOK_RPAREN, "expected ')' after import names");
        id.import_names = names;
        id.import_count = count;
    }

    return id;
}

// Parse class definition:
// class ClassName(ParentClass):
//     field: type
//     def method(self, args) -> type:
//         body
static ClassDef parse_class(Parser *p) {
    ClassDef cd;
    memset(&cd, 0, sizeof(cd));

    cd.line = p->cur.line;
    expect(p, TOK_CLASS, "expected 'class'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected class name at %zu:%zu", p->cur.line, p->cur.col);
    cd.name = dup_lexeme(p->cur);
    next(p);

    // Optional parent class: class Child(Parent):
    if (accept(p, TOK_LPAREN)) {
        if (p->cur.kind != TOK_IDENT) bp_fatal("expected parent class name at %zu:%zu", p->cur.line, p->cur.col);
        cd.parent_name = dup_lexeme(p->cur);
        next(p);
        expect(p, TOK_RPAREN, "expected ')' after parent class name");
    }

    expect(p, TOK_COLON, "expected ':' after class name");
    expect(p, TOK_NEWLINE, "expected newline after ':'");
    expect(p, TOK_INDENT, "expected indent after class header");

    char **field_names = NULL;
    Type *field_types = NULL;
    size_t field_count = 0, field_cap = 0;

    MethodDef *methods = NULL;
    size_t method_count = 0, method_cap = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        // Method definition (def inside class)
        if (p->cur.kind == TOK_DEF) {
            MethodDef md;
            memset(&md, 0, sizeof(md));
            md.line = p->cur.line;
            next(p);  // consume 'def'
            if (p->cur.kind != TOK_IDENT) bp_fatal("expected method name at %zu:%zu", p->cur.line, p->cur.col);
            md.name = dup_lexeme(p->cur);
            next(p);

            expect(p, TOK_LPAREN, "expected '(' after method name");

            Param *params = NULL;
            size_t paramc = 0, param_cap = 0;

            // First param should be 'self' for methods
            if (p->cur.kind != TOK_RPAREN) {
                for (;;) {
                    if (p->cur.kind == TOK_SELF) {
                        next(p);
                        // self doesn't need a type annotation - it's implicit
                        Type self_type;
                        memset(&self_type, 0, sizeof(self_type));
                        self_type.kind = TY_CLASS;
                        self_type.struct_name = bp_xstrdup(cd.name);
                        if (paramc + 1 > param_cap) { param_cap = param_cap ? param_cap * 2 : 4; params = bp_xrealloc(params, param_cap * sizeof(*params)); }
                        params[paramc++] = (Param){ .name = bp_xstrdup("self"), .type = self_type };
                    } else {
                        if (p->cur.kind != TOK_IDENT) bp_fatal("expected parameter name at %zu:%zu", p->cur.line, p->cur.col);
                        char *pname = dup_lexeme(p->cur);
                        next(p);
                        expect(p, TOK_COLON, "expected ':' in parameter");
                        Type pt = parse_type(p);
                        if (paramc + 1 > param_cap) { param_cap = param_cap ? param_cap * 2 : 4; params = bp_xrealloc(params, param_cap * sizeof(*params)); }
                        params[paramc++] = (Param){ .name = pname, .type = pt };
                    }
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "expected ')' after method params");
            expect(p, TOK_ARROW, "expected '->' return type");
            md.ret_type = parse_type(p);
            expect(p, TOK_COLON, "expected ':' after method signature");

            size_t body_len = 0;
            Stmt **body = parse_block(p, &body_len);

            md.params = params;
            md.paramc = paramc;
            md.body = body;
            md.body_len = body_len;

            if (method_count + 1 > method_cap) {
                method_cap = method_cap ? method_cap * 2 : 4;
                methods = bp_xrealloc(methods, method_cap * sizeof(*methods));
            }
            methods[method_count++] = md;
        } else if (p->cur.kind == TOK_IDENT) {
            // Field definition
            char *fname = dup_lexeme(p->cur);
            next(p);
            expect(p, TOK_COLON, "expected ':' after field name");
            Type ftype = parse_type(p);

            if (field_count + 1 > field_cap) {
                field_cap = field_cap ? field_cap * 2 : 4;
                field_names = bp_xrealloc(field_names, field_cap * sizeof(*field_names));
                field_types = bp_xrealloc(field_types, field_cap * sizeof(*field_types));
            }
            field_names[field_count] = fname;
            field_types[field_count] = ftype;
            field_count++;
        } else {
            bp_fatal("expected field or method definition in class at %zu:%zu", p->cur.line, p->cur.col);
        }

        skip_newlines(p);
    }

    if (p->cur.kind == TOK_DEDENT) next(p);

    cd.field_names = field_names;
    cd.field_types = field_types;
    cd.field_count = field_count;
    cd.methods = methods;
    cd.method_count = method_count;
    return cd;
}

// Parse extern C function declaration:
// extern fn printf(fmt: str) -> int from "libc.so.6"
// extern fn custom_add(a: int, b: int) -> int from "libcustom.so" as "add"
static ExternDef parse_extern(Parser *p) {
    ExternDef ed;
    memset(&ed, 0, sizeof(ed));

    ed.line = p->cur.line;
    expect(p, TOK_EXTERN, "expected 'extern'");
    expect(p, TOK_FN, "expected 'fn' after extern");

    if (p->cur.kind != TOK_IDENT) bp_fatal("expected function name at %zu:%zu", p->cur.line, p->cur.col);
    ed.name = dup_lexeme(p->cur);
    ed.c_name = bp_xstrdup(ed.name);  // Default to same name
    next(p);

    expect(p, TOK_LPAREN, "expected '(' after extern function name");

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
    expect(p, TOK_RPAREN, "expected ')' after extern params");
    expect(p, TOK_ARROW, "expected '->' for return type");
    ed.ret_type = parse_type(p);

    // Required: from "library.so"
    expect(p, TOK_FROM, "expected 'from' after extern signature");
    if (p->cur.kind != TOK_STR) bp_fatal("expected library path string at %zu:%zu", p->cur.line, p->cur.col);
    ed.library = bp_xstrdup(p->cur.str_val);
    free((void *)p->cur.str_val);
    next(p);

    // Optional: as "c_function_name"
    if (accept(p, TOK_AS)) {
        if (p->cur.kind != TOK_STR) bp_fatal("expected C function name string at %zu:%zu", p->cur.line, p->cur.col);
        free(ed.c_name);  // Free the default
        ed.c_name = bp_xstrdup(p->cur.str_val);
        free((void *)p->cur.str_val);
        next(p);
    }

    ed.params = params;
    ed.paramc = paramc;
    return ed;
}

// Parse union definition:
// union Expr:
//     IntLit(value: int)
//     BinOp(op: int, lhs: Expr, rhs: Expr)
static UnionDef parse_union(Parser *p) {
    UnionDef ud;
    memset(&ud, 0, sizeof(ud));
    ud.line = p->cur.line;
    expect(p, TOK_UNION, "expected 'union'");
    if (p->cur.kind != TOK_IDENT) bp_fatal("expected union name at %zu:%zu", p->cur.line, p->cur.col);
    ud.name = dup_lexeme(p->cur);
    next(p);
    expect(p, TOK_COLON, "expected ':' after union name");
    expect(p, TOK_NEWLINE, "expected newline after ':'");
    expect(p, TOK_INDENT, "expected indent after union header");

    UnionVariant *variants = NULL;
    size_t vc = 0, vcap = 0;

    while (p->cur.kind != TOK_DEDENT && p->cur.kind != TOK_EOF) {
        skip_newlines(p);
        if (p->cur.kind == TOK_DEDENT || p->cur.kind == TOK_EOF) break;

        if (p->cur.kind != TOK_IDENT) bp_fatal("expected variant name at %zu:%zu", p->cur.line, p->cur.col);
        UnionVariant v;
        memset(&v, 0, sizeof(v));
        v.name = dup_lexeme(p->cur);
        next(p);

        // Parse variant fields: VariantName(field: type, ...)
        char **fnames = NULL;
        Type *ftypes = NULL;
        size_t fc = 0, fcap = 0;
        if (accept(p, TOK_LPAREN)) {
            if (p->cur.kind != TOK_RPAREN) {
                for (;;) {
                    if (p->cur.kind != TOK_IDENT) bp_fatal("expected field name in variant at %zu:%zu", p->cur.line, p->cur.col);
                    char *fname = dup_lexeme(p->cur);
                    next(p);
                    expect(p, TOK_COLON, "expected ':' after variant field name");
                    Type ft = parse_type(p);
                    if (fc + 1 > fcap) { fcap = fcap ? fcap * 2 : 4; fnames = bp_xrealloc(fnames, fcap * sizeof(*fnames)); ftypes = bp_xrealloc(ftypes, fcap * sizeof(*ftypes)); }
                    fnames[fc] = fname;
                    ftypes[fc] = ft;
                    fc++;
                    if (!accept(p, TOK_COMMA)) break;
                }
            }
            expect(p, TOK_RPAREN, "expected ')' after variant fields");
        }
        v.field_names = fnames;
        v.field_types = ftypes;
        v.field_count = fc;

        if (vc + 1 > vcap) { vcap = vcap ? vcap * 2 : 4; variants = bp_xrealloc(variants, vcap * sizeof(*variants)); }
        variants[vc++] = v;

        skip_newlines(p);
    }
    if (p->cur.kind == TOK_DEDENT) next(p);

    ud.variants = variants;
    ud.variant_count = vc;
    return ud;
}

Module parse_module(const char *src) {
    Parser p;
    memset(&p, 0, sizeof(p));
    p.lx = lexer_new(src);
    next(&p);

    Module m;
    memset(&m, 0, sizeof(m));

    size_t fn_cap = 0, st_cap = 0, en_cap = 0, im_cap = 0, cl_cap = 0, ex_cap = 0, gv_cap = 0, un_cap = 0;
    skip_newlines(&p);

    while (p.cur.kind != TOK_EOF) {
        // Handle @packed decorator
        bool is_packed = false;
        bool is_export = false;

        // Check for decorators
        while (p.cur.kind == TOK_AT) {
            next(&p);  // consume '@'
            if (p.cur.kind != TOK_IDENT) bp_fatal("expected decorator name at %zu:%zu", p.cur.line, p.cur.col);
            if (p.cur.len == 6 && memcmp(p.cur.lexeme, "packed", 6) == 0) {
                is_packed = true;
            } else {
                bp_fatal("unknown decorator '%.*s' at %zu:%zu", (int)p.cur.len, p.cur.lexeme, p.cur.line, p.cur.col);
            }
            next(&p);
            skip_newlines(&p);
        }

        // Handle export keyword
        if (accept(&p, TOK_EXPORT)) {
            is_export = true;
        }

        if (p.cur.kind == TOK_DEF) {
            Function f = parse_function(&p);
            f.is_export = is_export;
            if (m.fnc + 1 > fn_cap) { fn_cap = fn_cap ? fn_cap * 2 : 8; m.fns = bp_xrealloc(m.fns, fn_cap * sizeof(*m.fns)); }
            m.fns[m.fnc++] = f;
        } else if (p.cur.kind == TOK_STRUCT) {
            StructDef sd = parse_struct(&p, is_packed);
            sd.is_export = is_export;
            if (m.structc + 1 > st_cap) { st_cap = st_cap ? st_cap * 2 : 8; m.structs = bp_xrealloc(m.structs, st_cap * sizeof(*m.structs)); }
            m.structs[m.structc++] = sd;
        } else if (p.cur.kind == TOK_ENUM) {
            EnumDef ed = parse_enum(&p);
            ed.is_export = is_export;
            if (m.enumc + 1 > en_cap) { en_cap = en_cap ? en_cap * 2 : 8; m.enums = bp_xrealloc(m.enums, en_cap * sizeof(*m.enums)); }
            m.enums[m.enumc++] = ed;
        } else if (p.cur.kind == TOK_IMPORT) {
            ImportDef id = parse_import(&p);
            if (m.importc + 1 > im_cap) { im_cap = im_cap ? im_cap * 2 : 8; m.imports = bp_xrealloc(m.imports, im_cap * sizeof(*m.imports)); }
            m.imports[m.importc++] = id;
        } else if (p.cur.kind == TOK_CLASS) {
            ClassDef cd = parse_class(&p);
            cd.is_export = is_export;
            if (m.classc + 1 > cl_cap) { cl_cap = cl_cap ? cl_cap * 2 : 8; m.classes = bp_xrealloc(m.classes, cl_cap * sizeof(*m.classes)); }
            m.classes[m.classc++] = cd;
        } else if (p.cur.kind == TOK_EXTERN) {
            ExternDef ed = parse_extern(&p);
            if (m.externc + 1 > ex_cap) { ex_cap = ex_cap ? ex_cap * 2 : 8; m.externs = bp_xrealloc(m.externs, ex_cap * sizeof(*m.externs)); }
            m.externs[m.externc++] = ed;
        } else if (p.cur.kind == TOK_UNION) {
            UnionDef ud = parse_union(&p);
            ud.is_export = is_export;
            if (m.unionc + 1 > un_cap) { un_cap = un_cap ? un_cap * 2 : 8; m.unions = bp_xrealloc(m.unions, un_cap * sizeof(*m.unions)); }
            m.unions[m.unionc++] = ud;
        } else if (p.cur.kind == TOK_LET) {
            Stmt *gv = parse_stmt(&p);
            if (m.global_varc + 1 > gv_cap) { gv_cap = gv_cap ? gv_cap * 2 : 8; m.global_vars = bp_xrealloc(m.global_vars, gv_cap * sizeof(*m.global_vars)); }
            m.global_vars[m.global_varc++] = gv;
        } else {
            bp_fatal("expected 'def', 'struct', 'enum', 'class', 'union', 'import', 'extern', 'let', or 'export' at top level (line %zu)", p.cur.line);
        }

        skip_newlines(&p);
    }

    lexer_free(p.lx);
    return m;
}
