#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OP_CONST_I64 = 1,
    OP_CONST_BOOL,
    OP_CONST_STR,
    OP_POP,

    OP_LOAD_LOCAL,
    OP_STORE_LOCAL,

    OP_ADD_I64,
    OP_SUB_I64,
    OP_MUL_I64,
    OP_DIV_I64,
    OP_MOD_I64,

    OP_ADD_STR,

    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_LTE,
    OP_GT,
    OP_GTE,

    OP_NOT,
    OP_NEG,
    OP_AND,
    OP_OR,

    OP_JMP,
    OP_JMP_IF_FALSE,

    OP_CALL_BUILTIN,
    OP_CALL,
    OP_RET
} OpCode;

typedef enum {
    BI_PRINT = 1,
    BI_LEN,
    BI_SUBSTR,
    BI_READ_LINE,
    BI_TO_STR,
    BI_CLOCK_MS,
    BI_EXIT,
    BI_FILE_READ,
    BI_FILE_WRITE,
    BI_CHR,
    BI_ORD,
    BI_BASE64_ENCODE,
    BI_BASE64_DECODE,
    BI_BYTES_TO_STR,
    BI_STR_TO_BYTES,
    
    // Math functions
    BI_ABS,
    BI_MIN,
    BI_MAX,
    BI_POW,
    BI_SQRT,
    BI_FLOOR,
    BI_CEIL,
    BI_ROUND,
    
    // String operations
    BI_STR_UPPER,
    BI_STR_LOWER,
    BI_STR_TRIM,
    BI_STR_STARTS_WITH,
    BI_STR_ENDS_WITH,
    BI_STR_FIND,
    BI_STR_REPLACE,
    BI_STR_SPLIT,
    BI_STR_JOIN,
    
    // Random numbers
    BI_RAND,
    BI_RAND_RANGE,
    BI_RAND_SEED,
    
    // File operations
    BI_FILE_EXISTS,
    BI_FILE_DELETE,
    BI_FILE_APPEND,
    
    // System operations
    BI_SLEEP,
    BI_GETENV,
    
    // Security functions
    BI_HASH_SHA256,
    BI_HASH_MD5,
    BI_SECURE_COMPARE,
    BI_RANDOM_BYTES,
    
    // String utilities
    BI_STR_REVERSE,
    BI_STR_REPEAT,
    BI_STR_PAD_LEFT,
    BI_STR_PAD_RIGHT,
    BI_STR_CONTAINS,
    BI_STR_COUNT,
    BI_INT_TO_HEX,
    BI_HEX_TO_INT,
    
    // Array/List simulation (string-based)
    BI_STR_CHAR_AT,
    BI_STR_INDEX_OF,
    
    // Validation functions
    BI_IS_DIGIT,
    BI_IS_ALPHA,
    BI_IS_ALNUM,
    BI_IS_SPACE,
    
    // Math extensions
    BI_CLAMP,
    BI_SIGN,
    
    // File utilities
    BI_FILE_SIZE,
    BI_FILE_COPY
} BuiltinId;

typedef struct {
    char *name;
    uint16_t arity;
    uint16_t locals;
    uint8_t *code;
    size_t code_len;

    uint32_t *str_const_ids;
    size_t str_const_len;
} BpFunc;

typedef struct {
    char **strings;
    size_t str_len;

    BpFunc *funcs;
    size_t fn_len;

    uint32_t entry;
} BpModule;

void bc_module_free(BpModule *m);

int bc_write_file(const char *path, const BpModule *m);
BpModule bc_read_file(const char *path);
