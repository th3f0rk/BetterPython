#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Register VM Configuration
#define REG_VM_MAX_REGS 256     // Maximum virtual registers per function
#define REG_VM_MAGIC "BPR1"    // Magic for register-based bytecode
#define STACK_VM_MAGIC "BPC1"  // Magic for stack-based bytecode (current)

// Bytecode format type
typedef enum {
    BC_FORMAT_STACK = 0,       // Original stack-based bytecode
    BC_FORMAT_REGISTER = 1     // New register-based bytecode
} BcFormat;

typedef enum {
    OP_CONST_I64 = 1,
    OP_CONST_F64,
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

    OP_ADD_F64,
    OP_SUB_F64,
    OP_MUL_F64,
    OP_DIV_F64,
    OP_MOD_F64,
    OP_NEG_F64,

    OP_ADD_STR,

    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_LTE,
    OP_GT,
    OP_GTE,

    OP_LT_F64,
    OP_LTE_F64,
    OP_GT_F64,
    OP_GTE_F64,

    OP_NOT,
    OP_NEG,
    OP_AND,
    OP_OR,

    OP_JMP,
    OP_JMP_IF_FALSE,
    OP_BREAK,
    OP_CONTINUE,

    OP_ARRAY_NEW,       // Create new array with N elements from stack
    OP_ARRAY_GET,       // Get element at index
    OP_ARRAY_SET,       // Set element at index

    OP_MAP_NEW,         // Create new map with N key-value pairs from stack
    OP_MAP_GET,         // Get value by key
    OP_MAP_SET,         // Set value by key

    OP_TRY_BEGIN,       // Register exception handler: u32 catch_addr, u32 finally_addr, u16 catch_var_slot
    OP_TRY_END,         // Unregister current exception handler
    OP_THROW,           // Throw exception (value on stack)

    OP_STRUCT_NEW,      // Create new struct: u16 type_id, then N field values from stack
    OP_STRUCT_GET,      // Get struct field: u16 field_index
    OP_STRUCT_SET,      // Set struct field: u16 field_index

    // Class operations
    OP_CLASS_NEW,       // Create new class instance: u16 class_id, u8 argc
    OP_METHOD_CALL,     // Call method on instance: u16 method_id, u8 argc
    OP_SUPER_CALL,      // Call parent method: u16 method_id, u8 argc
    OP_CLASS_GET,       // Get class field: u16 field_index
    OP_CLASS_SET,       // Set class field: u16 field_index

    // FFI operations
    OP_FFI_CALL,        // Call external C function: u16 extern_id, u8 argc

    OP_CALL_BUILTIN,
    OP_CALL,
    OP_RET,

    // =========================================================================
    // Register-Based Opcodes (v2.0) - Start at 128 to avoid conflicts
    // Format: R_OP dst, src1, src2 (3-address code)
    // =========================================================================

    // Constants and Movement
    R_CONST_I64 = 128,  // dst, imm64: r[dst] = imm64
    R_CONST_F64,        // dst, imm64: r[dst] = *(double*)&imm64
    R_CONST_BOOL,       // dst, imm8: r[dst] = bool(imm8)
    R_CONST_STR,        // dst, str_idx: r[dst] = strings[str_idx]
    R_CONST_NULL,       // dst: r[dst] = null
    R_MOV,              // dst, src: r[dst] = r[src]

    // Integer Arithmetic (3-address)
    R_ADD_I64,          // dst, src1, src2: r[dst] = r[src1] + r[src2]
    R_SUB_I64,          // dst, src1, src2: r[dst] = r[src1] - r[src2]
    R_MUL_I64,          // dst, src1, src2: r[dst] = r[src1] * r[src2]
    R_DIV_I64,          // dst, src1, src2: r[dst] = r[src1] / r[src2]
    R_MOD_I64,          // dst, src1, src2: r[dst] = r[src1] % r[src2]
    R_NEG_I64,          // dst, src: r[dst] = -r[src]

    // Float Arithmetic (3-address)
    R_ADD_F64,          // dst, src1, src2: r[dst] = r[src1] + r[src2]
    R_SUB_F64,          // dst, src1, src2: r[dst] = r[src1] - r[src2]
    R_MUL_F64,          // dst, src1, src2: r[dst] = r[src1] * r[src2]
    R_DIV_F64,          // dst, src1, src2: r[dst] = r[src1] / r[src2]
    R_MOD_F64,          // dst, src1, src2: r[dst] = fmod(r[src1], r[src2])
    R_NEG_F64,          // dst, src: r[dst] = -r[src]

    // String Operations
    R_ADD_STR,          // dst, src1, src2: r[dst] = r[src1] + r[src2]

    // Integer Comparisons (result is bool)
    R_EQ,               // dst, src1, src2: r[dst] = (r[src1] == r[src2])
    R_NEQ,              // dst, src1, src2: r[dst] = (r[src1] != r[src2])
    R_LT,               // dst, src1, src2: r[dst] = (r[src1] < r[src2])
    R_LTE,              // dst, src1, src2: r[dst] = (r[src1] <= r[src2])
    R_GT,               // dst, src1, src2: r[dst] = (r[src1] > r[src2])
    R_GTE,              // dst, src1, src2: r[dst] = (r[src1] >= r[src2])

    // Float Comparisons
    R_LT_F64,           // dst, src1, src2
    R_LTE_F64,
    R_GT_F64,
    R_GTE_F64,

    // Logical Operations
    R_NOT,              // dst, src: r[dst] = !r[src]
    R_AND,              // dst, src1, src2: r[dst] = r[src1] && r[src2]
    R_OR,               // dst, src1, src2: r[dst] = r[src1] || r[src2]

    // Control Flow
    R_JMP,              // offset: jump to offset
    R_JMP_IF_FALSE,     // cond, offset: if (!r[cond]) jump
    R_JMP_IF_TRUE,      // cond, offset: if (r[cond]) jump

    // Function Calls
    // Args are in r[arg_base..arg_base+argc-1]
    R_CALL,             // dst, fn_idx, arg_base, argc: r[dst] = fn(r[arg_base..])
    R_CALL_BUILTIN,     // dst, builtin_id, arg_base, argc
    R_RET,              // src: return r[src]

    // Arrays
    R_ARRAY_NEW,        // dst, src_base, count: r[dst] = [r[src_base]..r[src_base+count-1]]
    R_ARRAY_GET,        // dst, arr, idx: r[dst] = r[arr][r[idx]]
    R_ARRAY_SET,        // arr, idx, val: r[arr][r[idx]] = r[val]

    // Maps
    R_MAP_NEW,          // dst, src_base, count: r[dst] = {r[src]..} (key-value pairs)
    R_MAP_GET,          // dst, map, key: r[dst] = r[map][r[key]]
    R_MAP_SET,          // map, key, val: r[map][r[key]] = r[val]

    // Exception Handling
    R_TRY_BEGIN,        // catch_offset, finally_offset, exc_reg
    R_TRY_END,          // (no args)
    R_THROW,            // src: throw r[src]

    // Structs
    R_STRUCT_NEW,       // dst, type_id, src_base, field_count
    R_STRUCT_GET,       // dst, struct_reg, field_idx
    R_STRUCT_SET,       // struct_reg, field_idx, val_reg

    // Classes
    R_CLASS_NEW,        // dst, class_id, arg_base, argc
    R_CLASS_GET,        // dst, obj, field_idx
    R_CLASS_SET,        // obj, field_idx, val
    R_METHOD_CALL,      // dst, obj, method_id, arg_base, argc
    R_SUPER_CALL,       // dst, method_id, arg_base, argc

    // FFI
    R_FFI_CALL,         // dst, extern_id, arg_base, argc

    // Bitwise (3-address)
    R_BIT_AND,          // dst, src1, src2: r[dst] = r[src1] & r[src2]
    R_BIT_OR,           // dst, src1, src2: r[dst] = r[src1] | r[src2]
    R_BIT_XOR,          // dst, src1, src2: r[dst] = r[src1] ^ r[src2]
    R_BIT_NOT,          // dst, src: r[dst] = ~r[src]
    R_BIT_SHL,          // dst, src1, src2: r[dst] = r[src1] << r[src2]
    R_BIT_SHR           // dst, src1, src2: r[dst] = r[src1] >> r[src2]
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
    BI_FILE_COPY,

    // Float math functions
    BI_SIN,
    BI_COS,
    BI_TAN,
    BI_ASIN,
    BI_ACOS,
    BI_ATAN,
    BI_ATAN2,
    BI_LOG,
    BI_LOG10,
    BI_LOG2,
    BI_EXP,
    BI_FABS,
    BI_FFLOOR,
    BI_FCEIL,
    BI_FROUND,
    BI_FSQRT,
    BI_FPOW,

    // Float conversion functions
    BI_INT_TO_FLOAT,
    BI_FLOAT_TO_INT,
    BI_FLOAT_TO_STR,
    BI_STR_TO_FLOAT,

    // Float utilities
    BI_IS_NAN,
    BI_IS_INF,

    // Array operations
    BI_ARRAY_LEN,
    BI_ARRAY_PUSH,
    BI_ARRAY_POP,

    // Map operations
    BI_MAP_LEN,
    BI_MAP_KEYS,
    BI_MAP_VALUES,
    BI_MAP_HAS_KEY,
    BI_MAP_DELETE,

    // System/argv
    BI_ARGV,
    BI_ARGC,

    // Threading operations
    BI_THREAD_SPAWN,
    BI_THREAD_JOIN,
    BI_THREAD_DETACH,
    BI_THREAD_CURRENT,
    BI_THREAD_YIELD,
    BI_THREAD_SLEEP,
    BI_MUTEX_NEW,
    BI_MUTEX_LOCK,
    BI_MUTEX_TRYLOCK,
    BI_MUTEX_UNLOCK,
    BI_COND_NEW,
    BI_COND_WAIT,
    BI_COND_SIGNAL,
    BI_COND_BROADCAST,

    // Regex operations
    BI_REGEX_MATCH,
    BI_REGEX_SEARCH,
    BI_REGEX_REPLACE,
    BI_REGEX_SPLIT,
    BI_REGEX_FIND_ALL,

    // StringBuilder-like operations
    BI_STR_SPLIT_STR,
    BI_STR_JOIN_ARR,
    BI_STR_CONCAT_ALL,

    // Bitwise operations
    BI_BIT_AND,
    BI_BIT_OR,
    BI_BIT_XOR,
    BI_BIT_NOT,
    BI_BIT_SHL,
    BI_BIT_SHR,

    // Type conversion
    BI_PARSE_INT,

    // JSON
    BI_JSON_STRINGIFY,

    // Byte arrays
    BI_BYTES_NEW,
    BI_BYTES_GET,
    BI_BYTES_SET
} BuiltinId;

typedef struct {
    char *name;
    uint16_t arity;
    uint16_t locals;           // For stack VM: number of local slots
    uint8_t *code;
    size_t code_len;

    uint32_t *str_const_ids;
    size_t str_const_len;

    // Register VM fields (v2.0)
    BcFormat format;           // BC_FORMAT_STACK or BC_FORMAT_REGISTER
    uint8_t reg_count;         // Number of registers used (for register VM)
} BpFunc;

typedef struct {
    char *name;
    char **field_names;
    size_t field_count;
} BpStructType;

typedef struct {
    char *name;
    char *parent_name;          // NULL if no inheritance
    char **field_names;
    size_t field_count;
    uint16_t *method_ids;       // Function IDs for methods
    char **method_names;
    size_t method_count;
} BpClassType;

// FFI type codes for parameter/return marshalling
#define FFI_TC_INT    0   // int64_t (int, i8..i64, u8..u64, bool, enum)
#define FFI_TC_FLOAT  1   // double
#define FFI_TC_STR    2   // const char* (BpStr->data on call, new BpStr on return)
#define FFI_TC_PTR    3   // void*
#define FFI_TC_VOID   4   // void (return only)

typedef struct {
    char *name;                 // Function name in BetterPython
    char *c_name;               // Actual C function name
    char *library;              // Library path (e.g., "libc.so.6")
    uint16_t param_count;
    uint8_t *param_types;       // Array of FFI_TC_* codes, one per param
    uint8_t ret_type;           // FFI_TC_* code for return type
    bool is_variadic;
    void *handle;               // dlopen handle (set at runtime)
    void *fn_ptr;               // dlsym function pointer (set at runtime)
} BpExternFunc;

typedef struct {
    char **strings;
    size_t str_len;

    BpFunc *funcs;
    size_t fn_len;

    BpStructType *struct_types;
    size_t struct_type_len;

    BpClassType *class_types;
    size_t class_type_len;

    BpExternFunc *extern_funcs;
    size_t extern_func_len;

    uint32_t entry;
} BpModule;

void bc_module_free(BpModule *m);

int bc_write_file(const char *path, const BpModule *m);
BpModule bc_read_file(const char *path);
