#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TY_INT,
    TY_FLOAT,
    TY_BOOL,
    TY_STR,
    TY_VOID
} TypeKind;

typedef struct {
    TypeKind kind;
} Type;

static inline Type type_int(void)   { Type t = {TY_INT}; return t; }
static inline Type type_float(void) { Type t = {TY_FLOAT}; return t; }
static inline Type type_bool(void)  { Type t = {TY_BOOL}; return t; }
static inline Type type_str(void)   { Type t = {TY_STR}; return t; }
static inline Type type_void(void)  { Type t = {TY_VOID}; return t; }

typedef enum {
    EX_INT,
    EX_FLOAT,
    EX_STR,
    EX_BOOL,
    EX_VAR,
    EX_CALL,
    EX_UNARY,
    EX_BINARY
} ExprKind;

typedef enum {
    UOP_NEG,
    UOP_NOT
} UnaryOp;

typedef enum {
    BOP_ADD, BOP_SUB, BOP_MUL, BOP_DIV, BOP_MOD,
    BOP_EQ, BOP_NEQ, BOP_LT, BOP_LTE, BOP_GT, BOP_GTE,
    BOP_AND, BOP_OR
} BinaryOp;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    char *name;
    Type type;
} Param;

struct Expr {
    ExprKind kind;
    size_t line;

    Type inferred;

    union {
        int64_t int_val;
        double float_val;
        char *str_val;
        bool bool_val;
        char *var_name;

        struct {
            char *name;
            Expr **args;
            size_t argc;
        } call;

        struct {
            UnaryOp op;
            Expr *rhs;
        } unary;

        struct {
            BinaryOp op;
            Expr *lhs;
            Expr *rhs;
        } binary;
    } as;
};

typedef enum {
    ST_LET,
    ST_ASSIGN,
    ST_EXPR,
    ST_IF,
    ST_WHILE,
    ST_RETURN
} StmtKind;

struct Stmt {
    StmtKind kind;
    size_t line;

    union {
        struct {
            char *name;
            Type type;
            Expr *init;
        } let;

        struct {
            char *name;
            Expr *value;
        } assign;

        struct {
            Expr *expr;
        } expr;

        struct {
            Expr *cond;
            Stmt **then_stmts;
            size_t then_len;
            Stmt **else_stmts;
            size_t else_len;
        } ifs;

        struct {
            Expr *cond;
            Stmt **body;
            size_t body_len;
        } wh;

        struct {
            Expr *value; // may be NULL
        } ret;
    } as;
};

typedef struct {
    char *name;
    Param *params;
    size_t paramc;
    Type ret_type;
    Stmt **body;
    size_t body_len;
    size_t line;
} Function;

typedef struct {
    Function *fns;
    size_t fnc;
} Module;

Expr *expr_new_int(int64_t v, size_t line);
Expr *expr_new_float(double v, size_t line);
Expr *expr_new_str(char *s, size_t line);
Expr *expr_new_bool(bool v, size_t line);
Expr *expr_new_var(char *name, size_t line);
Expr *expr_new_call(char *name, Expr **args, size_t argc, size_t line);
Expr *expr_new_unary(UnaryOp op, Expr *rhs, size_t line);
Expr *expr_new_binary(BinaryOp op, Expr *lhs, Expr *rhs, size_t line);

Stmt *stmt_new_let(char *name, Type t, Expr *init, size_t line);
Stmt *stmt_new_assign(char *name, Expr *value, size_t line);
Stmt *stmt_new_expr(Expr *e, size_t line);
Stmt *stmt_new_if(Expr *cond, Stmt **then_s, size_t then_len, Stmt **else_s, size_t else_len, size_t line);
Stmt *stmt_new_while(Expr *cond, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_return(Expr *value, size_t line);

void module_free(Module *m);
