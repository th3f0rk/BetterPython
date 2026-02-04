#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TY_INT,
    TY_FLOAT,
    TY_BOOL,
    TY_STR,
    TY_VOID,
    TY_ARRAY,
    TY_MAP
} TypeKind;

typedef struct Type Type;

struct Type {
    TypeKind kind;
    Type *elem_type;  // For TY_ARRAY: the element type, for TY_MAP: the value type
    Type *key_type;   // For TY_MAP: the key type
};

static inline Type type_int(void)   { Type t = {TY_INT, NULL, NULL}; return t; }
static inline Type type_float(void) { Type t = {TY_FLOAT, NULL, NULL}; return t; }
static inline Type type_bool(void)  { Type t = {TY_BOOL, NULL, NULL}; return t; }
static inline Type type_str(void)   { Type t = {TY_STR, NULL, NULL}; return t; }
static inline Type type_void(void)  { Type t = {TY_VOID, NULL, NULL}; return t; }

Type *type_new(TypeKind kind);
Type *type_array(Type *elem);
Type *type_map(Type *key, Type *value);

typedef enum {
    EX_INT,
    EX_FLOAT,
    EX_STR,
    EX_BOOL,
    EX_VAR,
    EX_CALL,
    EX_UNARY,
    EX_BINARY,
    EX_ARRAY,
    EX_INDEX,
    EX_MAP
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
            int fn_index;  // -1 = builtin, >= 0 = user function index
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

        struct {
            Expr **elements;
            size_t len;
        } array;

        struct {
            Expr *array;
            Expr *index;
        } index;

        struct {
            Expr **keys;
            Expr **values;
            size_t len;
        } map;
    } as;
};

typedef enum {
    ST_LET,
    ST_ASSIGN,
    ST_INDEX_ASSIGN,
    ST_EXPR,
    ST_IF,
    ST_WHILE,
    ST_FOR,
    ST_BREAK,
    ST_CONTINUE,
    ST_RETURN,
    ST_TRY,
    ST_THROW
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
            Expr *array;
            Expr *index;
            Expr *value;
        } idx_assign;

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
            char *var;      // iterator variable name
            Expr *start;    // range start
            Expr *end;      // range end
            Stmt **body;
            size_t body_len;
        } forr;

        struct {
            Expr *value; // may be NULL
        } ret;

        struct {
            Stmt **try_stmts;
            size_t try_len;
            char *catch_var;         // Variable name to bind exception (may be NULL)
            Stmt **catch_stmts;
            size_t catch_len;
            Stmt **finally_stmts;    // May be NULL
            size_t finally_len;
        } try_catch;

        struct {
            Expr *value;   // Exception value to throw
        } throw;
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
Expr *expr_new_array(Expr **elements, size_t len, size_t line);
Expr *expr_new_index(Expr *array, Expr *index, size_t line);
Expr *expr_new_map(Expr **keys, Expr **values, size_t len, size_t line);

Stmt *stmt_new_let(char *name, Type t, Expr *init, size_t line);
Stmt *stmt_new_assign(char *name, Expr *value, size_t line);
Stmt *stmt_new_index_assign(Expr *array, Expr *index, Expr *value, size_t line);
Stmt *stmt_new_expr(Expr *e, size_t line);
Stmt *stmt_new_if(Expr *cond, Stmt **then_s, size_t then_len, Stmt **else_s, size_t else_len, size_t line);
Stmt *stmt_new_while(Expr *cond, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_for(char *var, Expr *start, Expr *end, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_break(size_t line);
Stmt *stmt_new_continue(size_t line);
Stmt *stmt_new_return(Expr *value, size_t line);
Stmt *stmt_new_try(Stmt **try_s, size_t try_len, char *catch_var, Stmt **catch_s, size_t catch_len, Stmt **finally_s, size_t finally_len, size_t line);
Stmt *stmt_new_throw(Expr *value, size_t line);

void module_free(Module *m);
