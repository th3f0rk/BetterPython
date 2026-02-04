#include "lexer.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

struct Lexer {
    const char *src;
    size_t i;
    size_t line;
    size_t col;

    bool has_peek;
    Token peek_tok;

    int indent_stack[256];
    int indent_top;

    int pending_dedents;
    bool at_line_start;
};

static Token tok_make(TokenKind k, const char *lex, size_t len, size_t line, size_t col) {
    Token t;
    t.kind = k;
    t.lexeme = lex;
    t.len = len;
    t.int_val = 0;
    t.float_val = 0.0;
    t.str_val = NULL;
    t.line = line;
    t.col = col;
    return t;
}

static char cur(Lexer *lx) { return lx->src[lx->i]; }
static char nxt(Lexer *lx) { return lx->src[lx->i + 1]; }

static void adv(Lexer *lx) {
    if (cur(lx) == '\n') {
        lx->line++;
        lx->col = 1;
        lx->i++;
        lx->at_line_start = true;
        return;
    }
    lx->i++;
    lx->col++;
}

static void skip_ws_inline(Lexer *lx) {
    while (cur(lx) == ' ' || cur(lx) == '\t' || cur(lx) == '\r') adv(lx);
}

static bool is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool is_ident(char c) { return isalnum((unsigned char)c) || c == '_'; }

static Token lex_string(Lexer *lx) {
    size_t line = lx->line, col = lx->col;
    adv(lx); // "
    size_t start = lx->i;

    char *buf = bp_xmalloc(1);
    size_t cap = 1, len = 0;

    while (cur(lx) && cur(lx) != '"') {
        char c = cur(lx);
        if (c == '\\') {
            adv(lx);
            c = cur(lx);
            if (!c) bp_fatal("unterminated string at %zu:%zu", line, col);
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                default: break;
            }
        }
        if (len + 1 >= cap) { cap *= 2; if (cap < 16) cap = 16; buf = bp_xrealloc(buf, cap); }
        buf[len++] = c;
        adv(lx);
    }
    if (cur(lx) != '"') bp_fatal("unterminated string at %zu:%zu", line, col);
    adv(lx);

    buf = bp_xrealloc(buf, len + 1);
    buf[len] = '\0';

    Token t = tok_make(TOK_STR, lx->src + start - 1, (lx->i - (start - 1)), line, col);
    t.str_val = buf;
    return t;
}

static Token lex_number(Lexer *lx) {
    size_t line = lx->line, col = lx->col;
    size_t start = lx->i;
    bool is_float = false;

    // Parse integer part
    while (isdigit((unsigned char)cur(lx))) {
        adv(lx);
    }

    // Check for decimal point
    if (cur(lx) == '.' && isdigit((unsigned char)nxt(lx))) {
        is_float = true;
        adv(lx); // consume '.'
        while (isdigit((unsigned char)cur(lx))) {
            adv(lx);
        }
    }

    // Check for exponent (scientific notation)
    if (cur(lx) == 'e' || cur(lx) == 'E') {
        is_float = true;
        adv(lx); // consume 'e' or 'E'
        if (cur(lx) == '+' || cur(lx) == '-') {
            adv(lx); // consume sign
        }
        if (!isdigit((unsigned char)cur(lx))) {
            bp_fatal("invalid number: expected digit after exponent at %zu:%zu", line, col);
        }
        while (isdigit((unsigned char)cur(lx))) {
            adv(lx);
        }
    }

    size_t len = lx->i - start;
    char *num_str = bp_xmalloc(len + 1);
    memcpy(num_str, lx->src + start, len);
    num_str[len] = '\0';

    Token t;
    if (is_float) {
        t = tok_make(TOK_FLOAT, lx->src + start, len, line, col);
        t.float_val = strtod(num_str, NULL);
    } else {
        t = tok_make(TOK_INT, lx->src + start, len, line, col);
        t.int_val = strtoll(num_str, NULL, 10);
    }
    free(num_str);
    return t;
}

static TokenKind keyword_kind(const char *s, size_t n) {
    struct { const char *k; TokenKind t; } m[] = {
        {"let", TOK_LET}, {"def", TOK_DEF}, {"if", TOK_IF}, {"elif", TOK_ELIF}, {"else", TOK_ELSE},
        {"while", TOK_WHILE}, {"for", TOK_FOR}, {"in", TOK_IN}, {"break", TOK_BREAK}, {"continue", TOK_CONTINUE},
        {"return", TOK_RETURN}, {"true", TOK_TRUE}, {"false", TOK_FALSE},
        {"and", TOK_AND}, {"or", TOK_OR}, {"not", TOK_NOT},
        {"import", TOK_IMPORT}, {"export", TOK_EXPORT}, {"as", TOK_AS},
        {"try", TOK_TRY}, {"catch", TOK_CATCH}, {"finally", TOK_FINALLY}, {"throw", TOK_THROW}
    };
    for (size_t i = 0; i < sizeof(m) / sizeof(m[0]); i++) {
        if (strlen(m[i].k) == n && memcmp(s, m[i].k, n) == 0) return m[i].t;
    }
    return TOK_IDENT;
}

static Token lex_ident(Lexer *lx) {
    size_t line = lx->line, col = lx->col;
    size_t start = lx->i;
    adv(lx);
    while (is_ident(cur(lx))) adv(lx);
    size_t n = lx->i - start;
    TokenKind k = keyword_kind(lx->src + start, n);
    return tok_make(k, lx->src + start, n, line, col);
}

static void handle_indentation(Lexer *lx) {
    if (!lx->at_line_start) return;
    lx->at_line_start = false;

    int spaces = 0;
    while (cur(lx) == ' ') { spaces++; adv(lx); }

    if (cur(lx) == '\n' || cur(lx) == '\0' || (cur(lx) == '#' )) {
        return;
    }

    int current = lx->indent_stack[lx->indent_top];
    if (spaces > current) {
        lx->indent_top++;
        lx->indent_stack[lx->indent_top] = spaces;
        lx->peek_tok = tok_make(TOK_INDENT, NULL, 0, lx->line, lx->col);
        lx->has_peek = true;
        return;
    }
    if (spaces < current) {
        while (lx->indent_top > 0 && spaces < lx->indent_stack[lx->indent_top]) {
            lx->indent_top--;
            lx->pending_dedents++;
        }
        if (spaces != lx->indent_stack[lx->indent_top]) {
            bp_fatal("indentation error at %zu:%zu", lx->line, lx->col);
        }
    }
}

Lexer *lexer_new(const char *src) {
    Lexer *lx = bp_xmalloc(sizeof(*lx));
    memset(lx, 0, sizeof(*lx));
    lx->src = src;
    lx->i = 0;
    lx->line = 1;
    lx->col = 1;
    lx->indent_stack[0] = 0;
    lx->indent_top = 0;
    lx->pending_dedents = 0;
    lx->at_line_start = true;
    return lx;
}

void lexer_free(Lexer *lx) {
    free(lx);
}

static Token lex_one(Lexer *lx) {
    handle_indentation(lx);
    if (lx->has_peek) { lx->has_peek = false; return lx->peek_tok; }
    
    if (lx->pending_dedents > 0) {
        lx->pending_dedents--;
        return tok_make(TOK_DEDENT, NULL, 0, lx->line, lx->col);
    }

    skip_ws_inline(lx);

    size_t line = lx->line, col = lx->col;
    char c = cur(lx);

    if (c == '\0') {
        if (lx->indent_top > 0) {
            lx->indent_top--;
            return tok_make(TOK_DEDENT, NULL, 0, line, col);
        }
        return tok_make(TOK_EOF, NULL, 0, line, col);
    }

    if (c == '#') {
        while (cur(lx) && cur(lx) != '\n') adv(lx);
        return lex_one(lx);
    }

    if (c == '\n') { adv(lx); return tok_make(TOK_NEWLINE, NULL, 0, line, col); }
    if (c == '"') return lex_string(lx);
    if (isdigit((unsigned char)c)) return lex_number(lx);
    if (is_ident_start(c)) return lex_ident(lx);

    if (c == ':' ) { adv(lx); return tok_make(TOK_COLON, NULL, 0, line, col); }
    if (c == ',' ) { adv(lx); return tok_make(TOK_COMMA, NULL, 0, line, col); }
    if (c == '(' ) { adv(lx); return tok_make(TOK_LPAREN, NULL, 0, line, col); }
    if (c == ')' ) { adv(lx); return tok_make(TOK_RPAREN, NULL, 0, line, col); }
    if (c == '[' ) { adv(lx); return tok_make(TOK_LBRACKET, NULL, 0, line, col); }
    if (c == ']' ) { adv(lx); return tok_make(TOK_RBRACKET, NULL, 0, line, col); }
    if (c == '{' ) { adv(lx); return tok_make(TOK_LBRACE, NULL, 0, line, col); }
    if (c == '}' ) { adv(lx); return tok_make(TOK_RBRACE, NULL, 0, line, col); }
    if (c == '.' ) { adv(lx); return tok_make(TOK_DOT, NULL, 0, line, col); }

    if (c == '-' && nxt(lx) == '>') { adv(lx); adv(lx); return tok_make(TOK_ARROW, NULL, 0, line, col); }

    if (c == '=' && nxt(lx) == '=') { adv(lx); adv(lx); return tok_make(TOK_EQ, NULL, 0, line, col); }
    if (c == '!' && nxt(lx) == '=') { adv(lx); adv(lx); return tok_make(TOK_NEQ, NULL, 0, line, col); }
    if (c == '<' && nxt(lx) == '=') { adv(lx); adv(lx); return tok_make(TOK_LTE, NULL, 0, line, col); }
    if (c == '>' && nxt(lx) == '=') { adv(lx); adv(lx); return tok_make(TOK_GTE, NULL, 0, line, col); }

    if (c == '=') { adv(lx); return tok_make(TOK_ASSIGN, NULL, 0, line, col); }
    if (c == '+') { adv(lx); return tok_make(TOK_PLUS, NULL, 0, line, col); }
    if (c == '-') { adv(lx); return tok_make(TOK_MINUS, NULL, 0, line, col); }
    if (c == '*') { adv(lx); return tok_make(TOK_STAR, NULL, 0, line, col); }
    if (c == '/') { adv(lx); return tok_make(TOK_SLASH, NULL, 0, line, col); }
    if (c == '%') { adv(lx); return tok_make(TOK_PCT, NULL, 0, line, col); }
    if (c == '<') { adv(lx); return tok_make(TOK_LT, NULL, 0, line, col); }
    if (c == '>') { adv(lx); return tok_make(TOK_GT, NULL, 0, line, col); }

    bp_fatal("unexpected character '%c' at %zu:%zu", c, line, col);
    return tok_make(TOK_EOF, NULL, 0, line, col);
}

Token lexer_next(Lexer *lx) {
    if (lx->has_peek) { lx->has_peek = false; return lx->peek_tok; }
    return lex_one(lx);
}

Token lexer_peek(Lexer *lx) {
    if (!lx->has_peek) { lx->peek_tok = lex_one(lx); lx->has_peek = true; }
    return lx->peek_tok;
}

const char *token_kind_name(TokenKind k) {
    switch (k) {
        case TOK_EOF: return "EOF";
        case TOK_NEWLINE: return "NEWLINE";
        case TOK_INDENT: return "INDENT";
        case TOK_DEDENT: return "DEDENT";
        case TOK_IDENT: return "IDENT";
        case TOK_INT: return "INT";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STR: return "STR";
        case TOK_LET: return "LET";
        case TOK_DEF: return "DEF";
        case TOK_IF: return "IF";
        case TOK_ELIF: return "ELIF";
        case TOK_ELSE: return "ELSE";
        case TOK_WHILE: return "WHILE";
        case TOK_FOR: return "FOR";
        case TOK_IN: return "IN";
        case TOK_BREAK: return "BREAK";
        case TOK_CONTINUE: return "CONTINUE";
        case TOK_RETURN: return "RETURN";
        case TOK_TRUE: return "TRUE";
        case TOK_FALSE: return "FALSE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";
        case TOK_NOT: return "NOT";
        case TOK_IMPORT: return "IMPORT";
        case TOK_EXPORT: return "EXPORT";
        case TOK_AS: return "AS";
        case TOK_DOT: return "DOT";
        case TOK_TRY: return "TRY";
        case TOK_CATCH: return "CATCH";
        case TOK_FINALLY: return "FINALLY";
        case TOK_THROW: return "THROW";
        case TOK_COLON: return "COLON";
        case TOK_COMMA: return "COMMA";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_ARROW: return "ARROW";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PCT: return "PCT";
        case TOK_EQ: return "EQ";
        case TOK_NEQ: return "NEQ";
        case TOK_LT: return "LT";
        case TOK_LTE: return "LTE";
        case TOK_GT: return "GT";
        case TOK_GTE: return "GTE";
        default: return "UNKNOWN";
    }
}
