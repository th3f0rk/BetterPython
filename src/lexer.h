#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,

    TOK_IDENT,
    TOK_INT,
    TOK_STR,

    TOK_LET,
    TOK_DEF,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_RETURN,
    TOK_TRUE,
    TOK_FALSE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,

    TOK_COLON,
    TOK_COMMA,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_ARROW,

    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PCT,

    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *lexeme;
    size_t len;
    int64_t int_val;
    const char *str_val;
    size_t line;
    size_t col;
} Token;

typedef struct Lexer Lexer;

Lexer *lexer_new(const char *src);
void lexer_free(Lexer *lx);

Token lexer_next(Lexer *lx);
Token lexer_peek(Lexer *lx);

const char *token_kind_name(TokenKind k);
