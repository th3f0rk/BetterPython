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
    TY_MAP,
    TY_STRUCT,
    // Fixed-width integer types
    TY_I8,
    TY_I16,
    TY_I32,
    TY_I64,
    TY_U8,
    TY_U16,
    TY_U32,
    TY_U64,
    // Tuple type
    TY_TUPLE,
    // Enum type
    TY_ENUM,
    // Function/closure type
    TY_FUNC,
    // Class instance type
    TY_CLASS,
    // C pointer type (for FFI)
    TY_PTR
} TypeKind;

typedef struct Type Type;

struct Type {
    TypeKind kind;
    Type *elem_type;  // For TY_ARRAY: the element type, for TY_MAP: the value type, TY_PTR: pointed type
    Type *key_type;   // For TY_MAP: the key type
    char *struct_name; // For TY_STRUCT: the struct name, TY_ENUM: the enum name, TY_CLASS: class name
    // For TY_TUPLE: array of element types
    Type **tuple_types;
    size_t tuple_len;
    // For TY_FUNC: parameter types and return type
    Type **param_types;
    size_t param_count;
    Type *return_type;
};

static inline Type type_int(void)   { Type t = {TY_INT, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_float(void) { Type t = {TY_FLOAT, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_bool(void)  { Type t = {TY_BOOL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_str(void)   { Type t = {TY_STR, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_void(void)  { Type t = {TY_VOID, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }

// Fixed-width integer types
static inline Type type_i8(void)   { Type t = {TY_I8, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_i16(void)  { Type t = {TY_I16, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_i32(void)  { Type t = {TY_I32, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_i64(void)  { Type t = {TY_I64, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_u8(void)   { Type t = {TY_U8, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_u16(void)  { Type t = {TY_U16, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_u32(void)  { Type t = {TY_U32, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }
static inline Type type_u64(void)  { Type t = {TY_U64, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL}; return t; }

Type *type_new(TypeKind kind);
Type *type_array(Type *elem);
Type *type_map(Type *key, Type *value);
Type *type_struct(char *name);
Type *type_tuple(Type **types, size_t len);
Type *type_func(Type **params, size_t paramc, Type *ret);
Type *type_enum(char *name);
Type *type_class(char *name);
Type *type_ptr(Type *elem);

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
    EX_MAP,
    EX_STRUCT_LITERAL,
    EX_FIELD_ACCESS,
    // New expression kinds
    EX_TUPLE,           // Tuple literal: (a, b, c)
    EX_LAMBDA,          // Lambda: fn(x, y) -> x + y
    EX_ENUM_MEMBER,     // Enum member access: Color.RED
    EX_FSTRING,         // f-string: f"Hello {name}"
    EX_METHOD_CALL,     // Method call: obj.method(args)
    EX_NEW,             // Class instantiation: new ClassName(args)
    EX_SUPER_CALL       // Super class call: super.method(args) or super().__init__()
} ExprKind;

typedef enum {
    UOP_NEG,
    UOP_NOT,
    UOP_BIT_NOT
} UnaryOp;

typedef enum {
    BOP_ADD, BOP_SUB, BOP_MUL, BOP_DIV, BOP_MOD,
    BOP_EQ, BOP_NEQ, BOP_LT, BOP_LTE, BOP_GT, BOP_GTE,
    BOP_AND, BOP_OR,
    BOP_BIT_AND, BOP_BIT_OR, BOP_BIT_XOR, BOP_BIT_SHL, BOP_BIT_SHR
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

        struct {
            char *struct_name;
            char **field_names;
            Expr **field_values;
            size_t field_count;
        } struct_literal;

        struct {
            Expr *object;
            char *field_name;
            int field_index;  // Set by type checker
        } field_access;

        // Tuple literal
        struct {
            Expr **elements;
            size_t len;
        } tuple;

        // Lambda expression
        struct {
            Param *params;
            size_t paramc;
            Expr *body;         // For expression lambdas: fn(x) -> x + 1
            Stmt **body_stmts;  // For block lambdas (if needed)
            size_t body_len;
            Type return_type;
        } lambda;

        // Enum member access: Color.RED
        struct {
            char *enum_name;
            char *member_name;
            int member_value;  // Set by type checker
        } enum_member;

        // f-string (format string with interpolated expressions)
        struct {
            Expr **parts;   // Alternating string literals and expressions
            size_t partc;   // Number of parts
        } fstring;

        // Method call: obj.method(args)
        struct {
            Expr *object;
            char *method_name;
            Expr **args;
            size_t argc;
            int method_index;  // Set by type checker
        } method_call;

        // Class instantiation: new ClassName(args)
        struct {
            char *class_name;
            Expr **args;
            size_t argc;
            int class_index;  // Set by type checker
        } new_expr;

        // Super call: super.method(args) or super().__init__(args)
        struct {
            char *method_name;  // NULL for __init__
            Expr **args;
            size_t argc;
        } super_call;
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
    ST_THROW,
    ST_FIELD_ASSIGN,    // obj.field = value
    ST_FOR_IN           // for x in collection:
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
            char *var;          // iterator variable name
            Expr *collection;   // array or map expression
            Stmt **body;
            size_t body_len;
        } for_in;

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

        struct {
            Expr *object;  // The field access expression (EX_FIELD_ACCESS)
            Expr *value;   // Value to assign
        } field_assign;
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
    bool is_export;  // Is this function exported from the module
} Function;

// Method definition (for methods on structs)
typedef struct {
    char *name;
    Param *params;      // First param is implicit 'self'
    size_t paramc;
    Type ret_type;
    Stmt **body;
    size_t body_len;
    size_t line;
} MethodDef;

typedef struct {
    char *name;
    char **field_names;
    Type *field_types;
    size_t field_count;
    // Methods on this struct
    MethodDef *methods;
    size_t method_count;
    bool is_packed;     // @packed attribute
    bool is_export;     // Is this struct exported from the module
    size_t line;
} StructDef;

// Enum definition
typedef struct {
    char *name;
    char **variant_names;
    int64_t *variant_values;  // Optional explicit values
    size_t variant_count;
    bool is_export;           // Is this enum exported from the module
    size_t line;
} EnumDef;

// Import statement info
typedef struct {
    char *module_name;      // "math" or "utils/helper"
    char *alias;            // "as" name, or NULL
    char **import_names;    // Specific names, or NULL for all
    size_t import_count;
    size_t line;
} ImportDef;

// Class definition
typedef struct {
    char *name;
    char *parent_name;      // Parent class name for inheritance, NULL if none
    char **field_names;
    Type *field_types;
    size_t field_count;
    MethodDef *methods;     // Methods including __init__
    size_t method_count;
    bool is_export;
    size_t line;
} ClassDef;

// External C function declaration (FFI)
typedef struct {
    char *name;             // Function name in BetterPython
    char *c_name;           // Actual C function name (may differ)
    char *library;          // Library path (e.g., "libc.so.6", "libm.so")
    Param *params;
    size_t paramc;
    Type ret_type;
    bool is_variadic;       // For printf-like functions
    size_t line;
} ExternDef;

typedef struct {
    Function *fns;
    size_t fnc;
    StructDef *structs;
    size_t structc;
    EnumDef *enums;
    size_t enumc;
    ImportDef *imports;
    size_t importc;
    ClassDef *classes;
    size_t classc;
    ExternDef *externs;
    size_t externc;
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
Expr *expr_new_struct_literal(char *struct_name, char **field_names, Expr **field_values, size_t field_count, size_t line);
Expr *expr_new_field_access(Expr *object, char *field_name, size_t line);
Expr *expr_new_tuple(Expr **elements, size_t len, size_t line);
Expr *expr_new_lambda(Param *params, size_t paramc, Expr *body, Type ret_type, size_t line);
Expr *expr_new_enum_member(char *enum_name, char *member_name, size_t line);
Expr *expr_new_fstring(Expr **parts, size_t partc, size_t line);
Expr *expr_new_method_call(Expr *object, char *method_name, Expr **args, size_t argc, size_t line);
Expr *expr_new_new(char *class_name, Expr **args, size_t argc, size_t line);
Expr *expr_new_super_call(char *method_name, Expr **args, size_t argc, size_t line);

Stmt *stmt_new_let(char *name, Type t, Expr *init, size_t line);
Stmt *stmt_new_assign(char *name, Expr *value, size_t line);
Stmt *stmt_new_index_assign(Expr *array, Expr *index, Expr *value, size_t line);
Stmt *stmt_new_field_assign(Expr *object, Expr *value, size_t line);
Stmt *stmt_new_expr(Expr *e, size_t line);
Stmt *stmt_new_if(Expr *cond, Stmt **then_s, size_t then_len, Stmt **else_s, size_t else_len, size_t line);
Stmt *stmt_new_while(Expr *cond, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_for(char *var, Expr *start, Expr *end, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_for_in(char *var, Expr *collection, Stmt **body, size_t body_len, size_t line);
Stmt *stmt_new_break(size_t line);
Stmt *stmt_new_continue(size_t line);
Stmt *stmt_new_return(Expr *value, size_t line);
Stmt *stmt_new_try(Stmt **try_s, size_t try_len, char *catch_var, Stmt **catch_s, size_t catch_len, Stmt **finally_s, size_t finally_len, size_t line);
Stmt *stmt_new_throw(Expr *value, size_t line);

void module_free(Module *m);
