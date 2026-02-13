#include "compiler.h"
#include "types.h"
#include "util.h"
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_put(Buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        while (b->len + n > b->cap) b->cap *= 2;
        b->data = bp_xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static void buf_u8(Buf *b, uint8_t x) { buf_put(b, &x, 1); }
static void buf_u16(Buf *b, uint16_t x) { buf_put(b, &x, 2); }
static void buf_u32(Buf *b, uint32_t x) { buf_put(b, &x, 4); }
static void buf_i64(Buf *b, int64_t x) { buf_put(b, &x, 8); }
static void buf_f64(Buf *b, double x) { buf_put(b, &x, 8); }

typedef struct {
    char *name;
    uint16_t slot;
    uint32_t scope_id;  // Unique scope ID when variable was created
} Local;

typedef struct {
    Local *items;
    size_t len;
    size_t cap;
    // Scope stack: each entry is a unique scope ID
    uint32_t *scope_stack;
    size_t scope_depth;
    size_t scope_cap;
    uint32_t next_scope_id;  // Counter for generating unique scope IDs
} Locals;

// Enter a new block scope with a unique ID
static void locals_push_scope(Locals *ls) {
    if (ls->scope_depth + 1 > ls->scope_cap) {
        ls->scope_cap = ls->scope_cap ? ls->scope_cap * 2 : 16;
        ls->scope_stack = bp_xrealloc(ls->scope_stack, ls->scope_cap * sizeof(uint32_t));
    }
    ls->scope_stack[ls->scope_depth++] = ls->next_scope_id++;
}

// Exit current block scope
static void locals_pop_scope(Locals *ls) {
    if (ls->scope_depth > 0) {
        ls->scope_depth--;
    }
}

// Special scope ID for function-level scope
#define SCOPE_FUNCTION_LEVEL UINT32_MAX

// Get the current scope ID (or SCOPE_FUNCTION_LEVEL if at function level)
static uint32_t locals_current_scope(Locals *ls) {
    if (ls->scope_depth == 0) return SCOPE_FUNCTION_LEVEL;
    return ls->scope_stack[ls->scope_depth - 1];
}

// Check if a scope ID is visible (is it in our current scope chain?)
static bool locals_scope_visible(Locals *ls, uint32_t scope_id) {
    if (scope_id == SCOPE_FUNCTION_LEVEL) return true;  // Function-level scope always visible
    for (size_t i = 0; i < ls->scope_depth; i++) {
        if (ls->scope_stack[i] == scope_id) return true;
    }
    return false;
}

static uint16_t locals_add(Locals *ls, const char *name) {
    uint32_t current = locals_current_scope(ls);
    // Only check for duplicates in the SAME scope (same scope_id)
    for (size_t i = 0; i < ls->len; i++) {
        if (ls->items[i].scope_id == current &&
            strcmp(ls->items[i].name, name) == 0) {
            bp_fatal("duplicate local '%s' in same scope", name);
        }
    }
    if (ls->len + 1 > ls->cap) {
        ls->cap = ls->cap ? ls->cap * 2 : 16;
        ls->items = bp_xrealloc(ls->items, ls->cap * sizeof(*ls->items));
    }
    uint16_t slot = (uint16_t)ls->len;
    ls->items[ls->len].name = bp_xstrdup(name);
    ls->items[ls->len].slot = slot;
    ls->items[ls->len].scope_id = current;
    ls->len++;
    return slot;
}

// Try to get a local variable slot, returns -1 if not found (also used where locals_get was)
static int locals_try_get(Locals *ls, const char *name) {
    for (size_t i = ls->len; i > 0; i--) {
        if (locals_scope_visible(ls, ls->items[i-1].scope_id) &&
            strcmp(ls->items[i-1].name, name) == 0) {
            return (int)ls->items[i-1].slot;
        }
    }
    return -1;
}

// Global variable tracking for cross-function access
static char **g_global_names = NULL;
static size_t g_global_count = 0;

static int global_get(const char *name) {
    for (size_t i = 0; i < g_global_count; i++) {
        if (strcmp(g_global_names[i], name) == 0) return (int)i;
    }
    return -1;
}

static void locals_free(Locals *ls) {
    for (size_t i = 0; i < ls->len; i++) free(ls->items[i].name);
    free(ls->items);
    free(ls->scope_stack);
    memset(ls, 0, sizeof(*ls));
}

typedef struct {
    char **strings;
    size_t len;
    size_t cap;
} StrPool;

static uint32_t strpool_add(StrPool *sp, const char *s) {
    for (size_t i = 0; i < sp->len; i++) {
        if (strcmp(sp->strings[i], s) == 0) return (uint32_t)i;
    }
    if (sp->len + 1 > sp->cap) {
        sp->cap = sp->cap ? sp->cap * 2 : 32;
        sp->strings = bp_xrealloc(sp->strings, sp->cap * sizeof(*sp->strings));
    }
    sp->strings[sp->len] = bp_xstrdup(s);
    return (uint32_t)sp->len++;
}

/* Convert AST Type to FFI type code for parameter marshalling */
static uint8_t type_to_ffi_tc(TypeKind k) {
    switch (k) {
        case TY_FLOAT: return FFI_TC_FLOAT;
        case TY_STR:   return FFI_TC_STR;
        case TY_PTR:   return FFI_TC_PTR;
        case TY_VOID:  return FFI_TC_VOID;
        default:       return FFI_TC_INT;   /* int, bool, enum, all fixed-width ints */
    }
}

static BuiltinId builtin_id(const char *name) {
    if (strcmp(name, "print") == 0) return BI_PRINT;
    if (strcmp(name, "len") == 0) return BI_LEN;
    if (strcmp(name, "substr") == 0) return BI_SUBSTR;
    if (strcmp(name, "read_line") == 0) return BI_READ_LINE;
    if (strcmp(name, "to_str") == 0) return BI_TO_STR;
    if (strcmp(name, "clock_ms") == 0) return BI_CLOCK_MS;
    if (strcmp(name, "exit") == 0) return BI_EXIT;
    if (strcmp(name, "file_read") == 0) return BI_FILE_READ;
    if (strcmp(name, "file_write") == 0) return BI_FILE_WRITE;
    if (strcmp(name, "base64_encode") == 0) return BI_BASE64_ENCODE;
    if (strcmp(name, "base64_decode") == 0) return BI_BASE64_DECODE;
    if (strcmp(name, "chr") == 0) return BI_CHR;
    if (strcmp(name, "ord") == 0) return BI_ORD;
    
    // Math
    if (strcmp(name, "abs") == 0) return BI_ABS;
    if (strcmp(name, "min") == 0) return BI_MIN;
    if (strcmp(name, "max") == 0) return BI_MAX;
    if (strcmp(name, "pow") == 0) return BI_POW;
    if (strcmp(name, "sqrt") == 0) return BI_SQRT;
    if (strcmp(name, "floor") == 0) return BI_FLOOR;
    if (strcmp(name, "ceil") == 0) return BI_CEIL;
    if (strcmp(name, "round") == 0) return BI_ROUND;
    
    // Strings
    if (strcmp(name, "str_upper") == 0) return BI_STR_UPPER;
    if (strcmp(name, "str_lower") == 0) return BI_STR_LOWER;
    if (strcmp(name, "str_trim") == 0) return BI_STR_TRIM;
    if (strcmp(name, "starts_with") == 0) return BI_STR_STARTS_WITH;
    if (strcmp(name, "ends_with") == 0) return BI_STR_ENDS_WITH;
    if (strcmp(name, "str_find") == 0) return BI_STR_FIND;
    if (strcmp(name, "str_replace") == 0) return BI_STR_REPLACE;
    
    // Random
    if (strcmp(name, "rand") == 0) return BI_RAND;
    if (strcmp(name, "rand_range") == 0) return BI_RAND_RANGE;
    if (strcmp(name, "rand_seed") == 0) return BI_RAND_SEED;
    
    // File
    if (strcmp(name, "file_exists") == 0) return BI_FILE_EXISTS;
    if (strcmp(name, "file_delete") == 0) return BI_FILE_DELETE;
    if (strcmp(name, "file_append") == 0) return BI_FILE_APPEND;
    
    // System
    if (strcmp(name, "sleep") == 0) return BI_SLEEP;
    if (strcmp(name, "getenv") == 0) return BI_GETENV;
    
    // Security
    if (strcmp(name, "hash_sha256") == 0) return BI_HASH_SHA256;
    if (strcmp(name, "hash_md5") == 0) return BI_HASH_MD5;
    if (strcmp(name, "secure_compare") == 0) return BI_SECURE_COMPARE;
    if (strcmp(name, "random_bytes") == 0) return BI_RANDOM_BYTES;
    
    // String utilities
    if (strcmp(name, "str_reverse") == 0) return BI_STR_REVERSE;
    if (strcmp(name, "str_repeat") == 0) return BI_STR_REPEAT;
    if (strcmp(name, "str_pad_left") == 0) return BI_STR_PAD_LEFT;
    if (strcmp(name, "str_pad_right") == 0) return BI_STR_PAD_RIGHT;
    if (strcmp(name, "str_contains") == 0) return BI_STR_CONTAINS;
    if (strcmp(name, "str_count") == 0) return BI_STR_COUNT;
    if (strcmp(name, "int_to_hex") == 0) return BI_INT_TO_HEX;
    if (strcmp(name, "hex_to_int") == 0) return BI_HEX_TO_INT;
    if (strcmp(name, "str_char_at") == 0) return BI_STR_CHAR_AT;
    if (strcmp(name, "str_index_of") == 0) return BI_STR_INDEX_OF;
    
    // Validation
    if (strcmp(name, "is_digit") == 0) return BI_IS_DIGIT;
    if (strcmp(name, "is_alpha") == 0) return BI_IS_ALPHA;
    if (strcmp(name, "is_alnum") == 0) return BI_IS_ALNUM;
    if (strcmp(name, "is_space") == 0) return BI_IS_SPACE;
    
    // Math extensions
    if (strcmp(name, "clamp") == 0) return BI_CLAMP;
    if (strcmp(name, "sign") == 0) return BI_SIGN;
    
    // File utilities
    if (strcmp(name, "file_size") == 0) return BI_FILE_SIZE;
    if (strcmp(name, "file_copy") == 0) return BI_FILE_COPY;

    // Float math functions
    if (strcmp(name, "sin") == 0) return BI_SIN;
    if (strcmp(name, "cos") == 0) return BI_COS;
    if (strcmp(name, "tan") == 0) return BI_TAN;
    if (strcmp(name, "asin") == 0) return BI_ASIN;
    if (strcmp(name, "acos") == 0) return BI_ACOS;
    if (strcmp(name, "atan") == 0) return BI_ATAN;
    if (strcmp(name, "atan2") == 0) return BI_ATAN2;
    if (strcmp(name, "log") == 0) return BI_LOG;
    if (strcmp(name, "log10") == 0) return BI_LOG10;
    if (strcmp(name, "log2") == 0) return BI_LOG2;
    if (strcmp(name, "exp") == 0) return BI_EXP;
    if (strcmp(name, "fabs") == 0) return BI_FABS;
    if (strcmp(name, "ffloor") == 0) return BI_FFLOOR;
    if (strcmp(name, "fceil") == 0) return BI_FCEIL;
    if (strcmp(name, "fround") == 0) return BI_FROUND;
    if (strcmp(name, "fsqrt") == 0) return BI_FSQRT;
    if (strcmp(name, "fpow") == 0) return BI_FPOW;

    // Float conversion functions
    if (strcmp(name, "int_to_float") == 0) return BI_INT_TO_FLOAT;
    if (strcmp(name, "float_to_int") == 0) return BI_FLOAT_TO_INT;
    if (strcmp(name, "float_to_str") == 0) return BI_FLOAT_TO_STR;
    if (strcmp(name, "str_to_float") == 0) return BI_STR_TO_FLOAT;

    // Float utilities
    if (strcmp(name, "is_nan") == 0) return BI_IS_NAN;
    if (strcmp(name, "is_inf") == 0) return BI_IS_INF;

    // Array operations
    if (strcmp(name, "array_len") == 0) return BI_ARRAY_LEN;
    if (strcmp(name, "array_push") == 0) return BI_ARRAY_PUSH;
    if (strcmp(name, "array_pop") == 0) return BI_ARRAY_POP;

    // Map operations
    if (strcmp(name, "map_len") == 0) return BI_MAP_LEN;
    if (strcmp(name, "map_keys") == 0) return BI_MAP_KEYS;
    if (strcmp(name, "map_values") == 0) return BI_MAP_VALUES;
    if (strcmp(name, "map_has_key") == 0) return BI_MAP_HAS_KEY;
    if (strcmp(name, "map_delete") == 0) return BI_MAP_DELETE;

    // System/argv
    if (strcmp(name, "argv") == 0) return BI_ARGV;
    if (strcmp(name, "argc") == 0) return BI_ARGC;

    // Threading
    if (strcmp(name, "thread_spawn") == 0) return BI_THREAD_SPAWN;
    if (strcmp(name, "thread_join") == 0) return BI_THREAD_JOIN;
    if (strcmp(name, "thread_detach") == 0) return BI_THREAD_DETACH;
    if (strcmp(name, "thread_current") == 0) return BI_THREAD_CURRENT;
    if (strcmp(name, "thread_yield") == 0) return BI_THREAD_YIELD;
    if (strcmp(name, "thread_sleep") == 0) return BI_THREAD_SLEEP;
    if (strcmp(name, "mutex_new") == 0) return BI_MUTEX_NEW;
    if (strcmp(name, "mutex_lock") == 0) return BI_MUTEX_LOCK;
    if (strcmp(name, "mutex_trylock") == 0) return BI_MUTEX_TRYLOCK;
    if (strcmp(name, "mutex_unlock") == 0) return BI_MUTEX_UNLOCK;
    if (strcmp(name, "cond_new") == 0) return BI_COND_NEW;
    if (strcmp(name, "cond_wait") == 0) return BI_COND_WAIT;
    if (strcmp(name, "cond_signal") == 0) return BI_COND_SIGNAL;
    if (strcmp(name, "cond_broadcast") == 0) return BI_COND_BROADCAST;

    // Regex operations
    if (strcmp(name, "regex_match") == 0) return BI_REGEX_MATCH;
    if (strcmp(name, "regex_search") == 0) return BI_REGEX_SEARCH;
    if (strcmp(name, "regex_replace") == 0) return BI_REGEX_REPLACE;
    if (strcmp(name, "regex_split") == 0) return BI_REGEX_SPLIT;
    if (strcmp(name, "regex_find_all") == 0) return BI_REGEX_FIND_ALL;

    // String split/join - support both short and long names
    if (strcmp(name, "str_split") == 0 || strcmp(name, "str_split_str") == 0) return BI_STR_SPLIT_STR;
    if (strcmp(name, "str_join") == 0 || strcmp(name, "str_join_arr") == 0) return BI_STR_JOIN_ARR;
    if (strcmp(name, "str_concat_all") == 0) return BI_STR_CONCAT_ALL;

    // Bitwise operations
    if (strcmp(name, "bit_and") == 0) return BI_BIT_AND;
    if (strcmp(name, "bit_or") == 0) return BI_BIT_OR;
    if (strcmp(name, "bit_xor") == 0) return BI_BIT_XOR;
    if (strcmp(name, "bit_not") == 0) return BI_BIT_NOT;
    if (strcmp(name, "bit_shl") == 0) return BI_BIT_SHL;
    if (strcmp(name, "bit_shr") == 0) return BI_BIT_SHR;

    // Byte conversion
    if (strcmp(name, "bytes_to_str") == 0) return BI_BYTES_TO_STR;
    if (strcmp(name, "str_to_bytes") == 0) return BI_STR_TO_BYTES;

    // Type conversion
    if (strcmp(name, "parse_int") == 0) return BI_PARSE_INT;

    // JSON
    if (strcmp(name, "json_stringify") == 0) return BI_JSON_STRINGIFY;

    // Byte arrays
    if (strcmp(name, "bytes_new") == 0) return BI_BYTES_NEW;
    if (strcmp(name, "bytes_get") == 0) return BI_BYTES_GET;
    if (strcmp(name, "bytes_set") == 0) return BI_BYTES_SET;

    // Array utilities
    if (strcmp(name, "array_sort") == 0) return BI_ARRAY_SORT;
    if (strcmp(name, "array_slice") == 0) return BI_ARRAY_SLICE;

    // Byte packing
    if (strcmp(name, "int_to_bytes") == 0) return BI_INT_TO_BYTES;
    if (strcmp(name, "int_from_bytes") == 0) return BI_INT_FROM_BYTES;

    // Byte buffer operations
    if (strcmp(name, "bytes_append") == 0) return BI_BYTES_APPEND;
    if (strcmp(name, "bytes_len") == 0) return BI_BYTES_LEN;
    if (strcmp(name, "bytes_write_u16") == 0) return BI_BYTES_WRITE_U16;
    if (strcmp(name, "bytes_write_u32") == 0) return BI_BYTES_WRITE_U32;
    if (strcmp(name, "bytes_write_i64") == 0) return BI_BYTES_WRITE_I64;
    if (strcmp(name, "bytes_read_u16") == 0) return BI_BYTES_READ_U16;
    if (strcmp(name, "bytes_read_u32") == 0) return BI_BYTES_READ_U32;
    if (strcmp(name, "bytes_read_i64") == 0) return BI_BYTES_READ_I64;

    // Binary file I/O
    if (strcmp(name, "file_read_bytes") == 0) return BI_FILE_READ_BYTES;
    if (strcmp(name, "file_write_bytes") == 0) return BI_FILE_WRITE_BYTES;

    // Array utilities
    if (strcmp(name, "array_insert") == 0) return BI_ARRAY_INSERT;
    if (strcmp(name, "array_remove") == 0) return BI_ARRAY_REMOVE;

    // Type introspection
    if (strcmp(name, "typeof") == 0) return BI_TYPEOF;

    // Array utility builtins
    if (strcmp(name, "array_concat") == 0) return BI_ARRAY_CONCAT;
    if (strcmp(name, "array_copy") == 0) return BI_ARRAY_COPY;
    if (strcmp(name, "array_clear") == 0) return BI_ARRAY_CLEAR;
    if (strcmp(name, "array_index_of") == 0) return BI_ARRAY_INDEX_OF;
    if (strcmp(name, "array_contains") == 0) return BI_ARRAY_CONTAINS;
    if (strcmp(name, "array_reverse") == 0) return BI_ARRAY_REVERSE;
    if (strcmp(name, "array_fill") == 0) return BI_ARRAY_FILL;
    if (strcmp(name, "str_from_chars") == 0) return BI_STR_FROM_CHARS;
    if (strcmp(name, "str_bytes") == 0) return BI_STR_BYTES;

    bp_fatal("unknown builtin '%s'", name);
    return BI_PRINT;
}

static void emit_jump_patch(Buf *b, size_t at, uint32_t target) {
    memcpy(b->data + at, &target, 4);
}

// Loop context for break/continue tracking
typedef struct {
    size_t *break_patches;    // Locations of break jumps to patch
    size_t break_count;
    size_t break_cap;
    size_t *continue_patches; // Locations of continue jumps to patch
    size_t continue_count;
    size_t continue_cap;
    uint32_t continue_target; // Address to jump to for continue (0 = needs patching)
} LoopCtx;

typedef struct {
    Buf code;
    uint32_t *str_ids;
    size_t str_len;
    size_t str_cap;
    Locals locals;
    StrPool *pool;
    BpModule *module;   // For lambda compilation (adding anonymous functions)

    // Loop context stack for break/continue
    LoopCtx *loops;
    size_t loop_count;
    size_t loop_cap;
} FnEmit;

static void push_loop(FnEmit *fe, uint32_t continue_target) {
    if (fe->loop_count + 1 > fe->loop_cap) {
        fe->loop_cap = fe->loop_cap ? fe->loop_cap * 2 : 8;
        fe->loops = bp_xrealloc(fe->loops, fe->loop_cap * sizeof(*fe->loops));
    }
    fe->loops[fe->loop_count].break_patches = NULL;
    fe->loops[fe->loop_count].break_count = 0;
    fe->loops[fe->loop_count].break_cap = 0;
    fe->loops[fe->loop_count].continue_patches = NULL;
    fe->loops[fe->loop_count].continue_count = 0;
    fe->loops[fe->loop_count].continue_cap = 0;
    fe->loops[fe->loop_count].continue_target = continue_target;
    fe->loop_count++;
}

static void add_break_patch(FnEmit *fe, size_t at) {
    if (fe->loop_count == 0) bp_fatal("break outside of loop");
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    if (ctx->break_count + 1 > ctx->break_cap) {
        ctx->break_cap = ctx->break_cap ? ctx->break_cap * 2 : 8;
        ctx->break_patches = bp_xrealloc(ctx->break_patches, ctx->break_cap * sizeof(size_t));
    }
    ctx->break_patches[ctx->break_count++] = at;
}

static void add_continue_patch(FnEmit *fe, size_t at) {
    if (fe->loop_count == 0) bp_fatal("continue outside of loop");
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    if (ctx->continue_count + 1 > ctx->continue_cap) {
        ctx->continue_cap = ctx->continue_cap ? ctx->continue_cap * 2 : 8;
        ctx->continue_patches = bp_xrealloc(ctx->continue_patches, ctx->continue_cap * sizeof(size_t));
    }
    ctx->continue_patches[ctx->continue_count++] = at;
}

static void patch_breaks(FnEmit *fe, uint32_t target) {
    if (fe->loop_count == 0) return;
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    for (size_t i = 0; i < ctx->break_count; i++) {
        emit_jump_patch(&fe->code, ctx->break_patches[i], target);
    }
    free(ctx->break_patches);
}

static void patch_continues(FnEmit *fe, uint32_t target) {
    if (fe->loop_count == 0) return;
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    for (size_t i = 0; i < ctx->continue_count; i++) {
        emit_jump_patch(&fe->code, ctx->continue_patches[i], target);
    }
    free(ctx->continue_patches);
}

static void pop_loop(FnEmit *fe) {
    if (fe->loop_count > 0) fe->loop_count--;
}

static void fn_add_strref(FnEmit *fe, uint32_t sid) {
    if (fe->str_len + 1 > fe->str_cap) {
        fe->str_cap = fe->str_cap ? fe->str_cap * 2 : 16;
        fe->str_ids = bp_xrealloc(fe->str_ids, fe->str_cap * sizeof(uint32_t));
    }
    fe->str_ids[fe->str_len++] = sid;
}

static uint32_t fn_str_index(FnEmit *fe, uint32_t pool_id) {
    for (size_t i = 0; i < fe->str_len; i++) {
        if (fe->str_ids[i] == pool_id) return (uint32_t)i;
    }
    fn_add_strref(fe, pool_id);
    return (uint32_t)(fe->str_len - 1);
}

static void emit_expr(FnEmit *fe, const Expr *e);

static void emit_stmt(FnEmit *fe, const Stmt *s) {
    switch (s->kind) {
        case ST_LET: {
            emit_expr(fe, s->as.let.init);
            uint16_t slot = locals_add(&fe->locals, s->as.let.name);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, slot);
            return;
        }
        case ST_ASSIGN: {
            emit_expr(fe, s->as.assign.value);
            int slot = locals_try_get(&fe->locals, s->as.assign.name);
            if (slot >= 0) {
                buf_u8(&fe->code, OP_STORE_LOCAL);
                buf_u16(&fe->code, (uint16_t)slot);
            } else {
                int gi = global_get(s->as.assign.name);
                if (gi < 0) bp_fatal("unknown variable '%s'", s->as.assign.name);
                buf_u8(&fe->code, OP_STORE_GLOBAL);
                buf_u16(&fe->code, (uint16_t)gi);
            }
            return;
        }
        case ST_INDEX_ASSIGN: {
            // container[key] = value
            // Stack order: container, key, value
            emit_expr(fe, s->as.idx_assign.array);
            emit_expr(fe, s->as.idx_assign.index);
            emit_expr(fe, s->as.idx_assign.value);
            // Check if container is a map or array based on inferred type
            if (s->as.idx_assign.array->inferred.kind == TY_MAP) {
                buf_u8(&fe->code, OP_MAP_SET);
            } else {
                buf_u8(&fe->code, OP_ARRAY_SET);
            }
            return;
        }
        case ST_EXPR: {
            emit_expr(fe, s->as.expr.expr);
            buf_u8(&fe->code, OP_POP);
            return;
        }
        case ST_RETURN: {
            if (s->as.ret.value) emit_expr(fe, s->as.ret.value);
            else { buf_u8(&fe->code, OP_CONST_BOOL); buf_u8(&fe->code, 0); } // placeholder "null" via bool false? (VM will ignore for void)
            buf_u8(&fe->code, OP_RET);
            return;
        }
        case ST_IF: {
            emit_expr(fe, s->as.ifs.cond);
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_false_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Enter then-block scope
            locals_push_scope(&fe->locals);
            for (size_t i = 0; i < s->as.ifs.then_len; i++) emit_stmt(fe, s->as.ifs.then_stmts[i]);
            locals_pop_scope(&fe->locals);

            buf_u8(&fe->code, OP_JMP);
            size_t jmp_end_at = fe->code.len;
            buf_u32(&fe->code, 0);

            emit_jump_patch(&fe->code, jmp_false_at, (uint32_t)fe->code.len);

            // Enter else-block scope
            locals_push_scope(&fe->locals);
            for (size_t i = 0; i < s->as.ifs.else_len; i++) emit_stmt(fe, s->as.ifs.else_stmts[i]);
            locals_pop_scope(&fe->locals);

            emit_jump_patch(&fe->code, jmp_end_at, (uint32_t)fe->code.len);
            return;
        }
        case ST_WHILE: {
            uint32_t loop_start = (uint32_t)fe->code.len;
            push_loop(fe, loop_start);

            emit_expr(fe, s->as.wh.cond);
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Enter while body scope
            locals_push_scope(&fe->locals);
            for (size_t i = 0; i < s->as.wh.body_len; i++) emit_stmt(fe, s->as.wh.body[i]);
            locals_pop_scope(&fe->locals);

            buf_u8(&fe->code, OP_JMP);
            buf_u32(&fe->code, loop_start);

            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);
            return;
        }
        case ST_FOR: {
            // for i in range(start, end):
            //   body
            // becomes:
            //   let i = start
            //   while i < end:
            //     body
            //     i = i + 1

            // Enter for loop scope (includes iterator variable)
            locals_push_scope(&fe->locals);

            // Emit: let i = start
            emit_expr(fe, s->as.forr.start);
            uint16_t iter_slot = locals_add(&fe->locals, s->as.forr.var);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, iter_slot);

            // loop_start: condition check (i < end)
            uint32_t loop_start = (uint32_t)fe->code.len;

            // For continue, we want to jump to increment, not condition
            // So continue_target will be set to increment position later
            // For now, we'll use a simpler approach: continue jumps to increment
            // We'll compute continue_target after we know where increment is

            // Load i
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, iter_slot);
            // Emit end expression
            emit_expr(fe, s->as.forr.end);
            // i < end
            buf_u8(&fe->code, OP_LT);
            // Jump if false (i >= end)
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Push loop context with continue pointing to increment
            // We'll patch this later
            push_loop(fe, 0);  // placeholder, will be patched

            // Emit body (already in for loop scope)
            for (size_t i = 0; i < s->as.forr.body_len; i++) emit_stmt(fe, s->as.forr.body[i]);

            // continue_target: increment section
            uint32_t continue_pos = (uint32_t)fe->code.len;
            // Patch any continue statements that were deferred
            patch_continues(fe, continue_pos);

            // i = i + 1
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, iter_slot);
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, 1);
            buf_u8(&fe->code, OP_ADD_I64);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, iter_slot);

            // Jump back to condition
            buf_u8(&fe->code, OP_JMP);
            buf_u32(&fe->code, loop_start);

            // loop_end
            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);

            // Exit for loop scope
            locals_pop_scope(&fe->locals);
            return;
        }
        case ST_FOR_IN: {
            // for x in collection:  →  desugars to index-based loop
            locals_push_scope(&fe->locals);
            Type coll_type = s->as.for_in.collection->inferred;
            bool is_map = (coll_type.kind == TY_MAP);

            // Evaluate collection
            emit_expr(fe, s->as.for_in.collection);
            uint16_t coll_slot = locals_add(&fe->locals, "__for_coll");
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, coll_slot);

            // For maps: get keys array  →  coll = map_keys(coll)
            if (is_map) {
                buf_u8(&fe->code, OP_LOAD_LOCAL);
                buf_u16(&fe->code, coll_slot);
                buf_u8(&fe->code, OP_CALL_BUILTIN);
                buf_u16(&fe->code, (uint16_t)BI_MAP_KEYS);
                buf_u8(&fe->code, 1);
                buf_u8(&fe->code, OP_STORE_LOCAL);
                buf_u16(&fe->code, coll_slot);
            }

            // len = array_len(coll)
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, coll_slot);
            buf_u8(&fe->code, OP_CALL_BUILTIN);
            buf_u16(&fe->code, (uint16_t)BI_ARRAY_LEN);
            buf_u8(&fe->code, 1);
            uint16_t len_slot = locals_add(&fe->locals, "__for_len");
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, len_slot);

            // idx = 0
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, 0);
            uint16_t idx_slot = locals_add(&fe->locals, "__for_idx");
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, idx_slot);

            // loop_start:
            uint32_t loop_start = (uint32_t)fe->code.len;
            push_loop(fe, 0);

            // condition: idx < len
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, idx_slot);
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, len_slot);
            buf_u8(&fe->code, OP_LT);
            buf_u8(&fe->code, OP_JMP_IF_FALSE);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // x = coll[idx]
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, coll_slot);
            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, idx_slot);
            buf_u8(&fe->code, OP_ARRAY_GET);
            uint16_t var_slot = locals_add(&fe->locals, s->as.for_in.var);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, var_slot);

            // body
            for (size_t i = 0; i < s->as.for_in.body_len; i++) emit_stmt(fe, s->as.for_in.body[i]);

            // continue_target: idx++
            uint32_t continue_pos = (uint32_t)fe->code.len;
            patch_continues(fe, continue_pos);

            buf_u8(&fe->code, OP_LOAD_LOCAL);
            buf_u16(&fe->code, idx_slot);
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, 1);
            buf_u8(&fe->code, OP_ADD_I64);
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, idx_slot);

            buf_u8(&fe->code, OP_JMP);
            buf_u32(&fe->code, loop_start);

            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);
            locals_pop_scope(&fe->locals);
            return;
        }
        case ST_MATCH: {
            // Match compiles as a chain of if/elif with equality checks
            emit_expr(fe, s->as.match.expr);
            // Store matched value in temp local
            uint16_t match_slot = locals_add(&fe->locals, "__match_val");
            buf_u8(&fe->code, OP_STORE_LOCAL);
            buf_u16(&fe->code, match_slot);

            size_t *end_patches = NULL;
            size_t end_count = 0, end_cap = 0;

            for (size_t i = 0; i < s->as.match.case_count; i++) {
                if (s->as.match.case_values[i] == NULL) continue; // skip default for now

                // Load match value and compare
                buf_u8(&fe->code, OP_LOAD_LOCAL);
                buf_u16(&fe->code, match_slot);
                emit_expr(fe, s->as.match.case_values[i]);
                buf_u8(&fe->code, OP_EQ);
                buf_u8(&fe->code, OP_JMP_IF_FALSE);
                size_t skip_at = fe->code.len;
                buf_u32(&fe->code, 0);

                // Case body
                locals_push_scope(&fe->locals);
                for (size_t j = 0; j < s->as.match.case_body_lens[i]; j++)
                    emit_stmt(fe, s->as.match.case_bodies[i][j]);
                locals_pop_scope(&fe->locals);

                // Jump to end after body
                buf_u8(&fe->code, OP_JMP);
                if (end_count + 1 > end_cap) {
                    end_cap = end_cap ? end_cap * 2 : 8;
                    end_patches = bp_xrealloc(end_patches, end_cap * sizeof(size_t));
                }
                end_patches[end_count++] = fe->code.len;
                buf_u32(&fe->code, 0);

                // Patch skip
                emit_jump_patch(&fe->code, skip_at, (uint32_t)fe->code.len);
            }

            // Default case (if any)
            if (s->as.match.has_default) {
                size_t di = s->as.match.default_idx;
                locals_push_scope(&fe->locals);
                for (size_t j = 0; j < s->as.match.case_body_lens[di]; j++)
                    emit_stmt(fe, s->as.match.case_bodies[di][j]);
                locals_pop_scope(&fe->locals);
            }

            // Patch all end jumps
            uint32_t end_addr = (uint32_t)fe->code.len;
            for (size_t i = 0; i < end_count; i++)
                emit_jump_patch(&fe->code, end_patches[i], end_addr);
            free(end_patches);
            return;
        }
        case ST_BREAK: {
            if (fe->loop_count == 0) bp_fatal("break outside of loop");
            buf_u8(&fe->code, OP_JMP);
            add_break_patch(fe, fe->code.len);
            buf_u32(&fe->code, 0);  // Will be patched when loop ends
            return;
        }
        case ST_CONTINUE: {
            if (fe->loop_count == 0) bp_fatal("continue outside of loop");
            buf_u8(&fe->code, OP_JMP);
            // If continue_target is 0, we need to patch later (for loops)
            // Otherwise use the known target (while loops)
            if (fe->loops[fe->loop_count - 1].continue_target == 0) {
                add_continue_patch(fe, fe->code.len);
                buf_u32(&fe->code, 0);
            } else {
                buf_u32(&fe->code, fe->loops[fe->loop_count - 1].continue_target);
            }
            return;
        }
        case ST_TRY: {
            // Enter try scope
            locals_push_scope(&fe->locals);

            // Allocate slot for catch variable if needed (must be done before try body)
            // The catch variable is in catch scope, but we allocate its slot first
            uint16_t catch_var_slot = 0;
            bool has_catch_var = s->as.try_catch.catch_var != NULL;

            // Emit: OP_TRY_BEGIN <catch_addr> <finally_addr> <catch_var_slot>
            buf_u8(&fe->code, OP_TRY_BEGIN);
            size_t catch_addr_at = fe->code.len;
            buf_u32(&fe->code, 0);  // Placeholder for catch address
            size_t finally_addr_at = fe->code.len;
            buf_u32(&fe->code, 0);  // Placeholder for finally address (0 = none)
            size_t catch_var_slot_at = fe->code.len;
            buf_u16(&fe->code, 0);  // Placeholder for catch var slot

            // Emit try body
            for (size_t i = 0; i < s->as.try_catch.try_len; i++) {
                emit_stmt(fe, s->as.try_catch.try_stmts[i]);
            }

            // Exit try scope
            locals_pop_scope(&fe->locals);

            // Emit: OP_TRY_END (normal exit - no exception)
            buf_u8(&fe->code, OP_TRY_END);

            // Jump to finally (or end if no finally)
            buf_u8(&fe->code, OP_JMP);
            size_t skip_catch_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Patch catch address
            uint32_t catch_addr = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, catch_addr_at, catch_addr);

            // Enter catch scope
            locals_push_scope(&fe->locals);
            if (has_catch_var) {
                catch_var_slot = locals_add(&fe->locals, s->as.try_catch.catch_var);
                // Patch the catch var slot in the OP_TRY_BEGIN instruction
                memcpy(fe->code.data + catch_var_slot_at, &catch_var_slot, 2);
            }

            // Emit catch body
            for (size_t i = 0; i < s->as.try_catch.catch_len; i++) {
                emit_stmt(fe, s->as.try_catch.catch_stmts[i]);
            }
            // Exit catch scope
            locals_pop_scope(&fe->locals);

            // Patch skip_catch to jump to finally/end
            uint32_t finally_addr = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, skip_catch_at, finally_addr);

            // Patch finally address if there's a finally block
            if (s->as.try_catch.finally_len > 0) {
                emit_jump_patch(&fe->code, finally_addr_at, finally_addr);

                // Enter finally scope
                locals_push_scope(&fe->locals);
                // Emit finally body
                for (size_t i = 0; i < s->as.try_catch.finally_len; i++) {
                    emit_stmt(fe, s->as.try_catch.finally_stmts[i]);
                }
                // Exit finally scope
                locals_pop_scope(&fe->locals);
            }
            return;
        }
        case ST_THROW: {
            emit_expr(fe, s->as.throw.value);
            buf_u8(&fe->code, OP_THROW);
            return;
        }
        case ST_FIELD_ASSIGN: {
            // obj.field = value -> stack: [obj, value] then STRUCT_SET/CLASS_SET field_idx
            const Expr *fa = s->as.field_assign.object;
            emit_expr(fe, fa->as.field_access.object);  // push object
            emit_expr(fe, s->as.field_assign.value);     // push value
            if (fa->as.field_access.object->inferred.kind == TY_CLASS) {
                buf_u8(&fe->code, OP_CLASS_SET);
            } else {
                buf_u8(&fe->code, OP_STRUCT_SET);
            }
            buf_u16(&fe->code, (uint16_t)fa->as.field_access.field_index);
            return;
        }
        default: break;
    }
    bp_fatal("unknown stmt");
}

static void emit_expr(FnEmit *fe, const Expr *e) {
    switch (e->kind) {
        case EX_INT:
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, e->as.int_val);
            return;
        case EX_FLOAT:
            buf_u8(&fe->code, OP_CONST_F64);
            buf_f64(&fe->code, e->as.float_val);
            return;
        case EX_BOOL:
            buf_u8(&fe->code, OP_CONST_BOOL);
            buf_u8(&fe->code, e->as.bool_val ? 1 : 0);
            return;
        case EX_STR: {
            uint32_t sid = strpool_add(fe->pool, e->as.str_val);
            uint32_t local_id = fn_str_index(fe, sid);
            buf_u8(&fe->code, OP_CONST_STR);
            buf_u32(&fe->code, local_id);
            return;
        }
        case EX_VAR: {
            int slot = locals_try_get(&fe->locals, e->as.var_name);
            if (slot >= 0) {
                buf_u8(&fe->code, OP_LOAD_LOCAL);
                buf_u16(&fe->code, (uint16_t)slot);
            } else {
                int gi = global_get(e->as.var_name);
                if (gi < 0) bp_fatal("unknown variable '%s'", e->as.var_name);
                buf_u8(&fe->code, OP_LOAD_GLOBAL);
                buf_u16(&fe->code, (uint16_t)gi);
            }
            return;
        }
        case EX_CALL: {
            for (size_t i = 0; i < e->as.call.argc; i++) emit_expr(fe, e->as.call.args[i]);

            if (e->as.call.fn_index <= -100) {
                // FFI extern function call (fn_index = -(100 + extern_index))
                uint16_t extern_id = (uint16_t)(-(e->as.call.fn_index + 100));
                buf_u8(&fe->code, OP_FFI_CALL);
                buf_u16(&fe->code, extern_id);
                buf_u8(&fe->code, (uint8_t)e->as.call.argc);
            } else if (e->as.call.fn_index < 0) {
                // Builtin function call
                BuiltinId id = builtin_id(e->as.call.name);
                buf_u8(&fe->code, OP_CALL_BUILTIN);
                buf_u16(&fe->code, (uint16_t)id);
                buf_u16(&fe->code, (uint16_t)e->as.call.argc);
            } else {
                // User-defined function call
                buf_u8(&fe->code, OP_CALL);
                buf_u32(&fe->code, (uint32_t)e->as.call.fn_index);
                buf_u16(&fe->code, (uint16_t)e->as.call.argc);
            }
            return;
        }
        case EX_UNARY: {
            emit_expr(fe, e->as.unary.rhs);
            if (e->as.unary.op == UOP_NEG) {
                if (e->inferred.kind == TY_FLOAT) buf_u8(&fe->code, OP_NEG_F64);
                else buf_u8(&fe->code, OP_NEG);
            } else {
                buf_u8(&fe->code, OP_NOT);
            }
            return;
        }
        case EX_BINARY: {
            // Boolean AND/OR operators
            if (e->as.binary.op == BOP_AND || e->as.binary.op == BOP_OR) {
                emit_expr(fe, e->as.binary.lhs);
                emit_expr(fe, e->as.binary.rhs);
                buf_u8(&fe->code, (e->as.binary.op == BOP_AND) ? OP_AND : OP_OR);
                return;
            }
            emit_expr(fe, e->as.binary.lhs);
            emit_expr(fe, e->as.binary.rhs);
            bool is_float = (e->inferred.kind == TY_FLOAT) ||
                             (e->as.binary.lhs->inferred.kind == TY_FLOAT) ||
                             (e->as.binary.rhs->inferred.kind == TY_FLOAT);
            switch (e->as.binary.op) {
                case BOP_ADD:
                    if (e->inferred.kind == TY_STR) buf_u8(&fe->code, OP_ADD_STR);
                    else if (is_float) buf_u8(&fe->code, OP_ADD_F64);
                    else buf_u8(&fe->code, OP_ADD_I64);
                    return;
                case BOP_SUB:
                    buf_u8(&fe->code, is_float ? OP_SUB_F64 : OP_SUB_I64);
                    return;
                case BOP_MUL:
                    buf_u8(&fe->code, is_float ? OP_MUL_F64 : OP_MUL_I64);
                    return;
                case BOP_DIV:
                    buf_u8(&fe->code, is_float ? OP_DIV_F64 : OP_DIV_I64);
                    return;
                case BOP_MOD:
                    buf_u8(&fe->code, is_float ? OP_MOD_F64 : OP_MOD_I64);
                    return;
                case BOP_EQ: buf_u8(&fe->code, OP_EQ); return;
                case BOP_NEQ: buf_u8(&fe->code, OP_NEQ); return;
                case BOP_LT:
                    buf_u8(&fe->code, is_float ? OP_LT_F64 : OP_LT);
                    return;
                case BOP_LTE:
                    buf_u8(&fe->code, is_float ? OP_LTE_F64 : OP_LTE);
                    return;
                case BOP_GT:
                    buf_u8(&fe->code, is_float ? OP_GT_F64 : OP_GT);
                    return;
                case BOP_GTE:
                    buf_u8(&fe->code, is_float ? OP_GTE_F64 : OP_GTE);
                    return;
                default: break;
            }
            bp_fatal("unsupported binary op");
            return;
        }
        case EX_ARRAY: {
            // Push all elements onto stack, then OP_ARRAY_NEW with count
            for (size_t i = 0; i < e->as.array.len; i++) {
                emit_expr(fe, e->as.array.elements[i]);
            }
            buf_u8(&fe->code, OP_ARRAY_NEW);
            buf_u32(&fe->code, (uint32_t)e->as.array.len);
            return;
        }
        case EX_INDEX: {
            // Push container, push key, then appropriate GET opcode
            emit_expr(fe, e->as.index.array);
            emit_expr(fe, e->as.index.index);
            // Check if container is a map or array based on inferred type
            if (e->as.index.array->inferred.kind == TY_MAP) {
                buf_u8(&fe->code, OP_MAP_GET);
            } else {
                buf_u8(&fe->code, OP_ARRAY_GET);
            }
            return;
        }
        case EX_MAP: {
            // Push all key-value pairs onto stack, then OP_MAP_NEW with count
            for (size_t i = 0; i < e->as.map.len; i++) {
                emit_expr(fe, e->as.map.keys[i]);
                emit_expr(fe, e->as.map.values[i]);
            }
            buf_u8(&fe->code, OP_MAP_NEW);
            buf_u32(&fe->code, (uint32_t)e->as.map.len);
            return;
        }
        case EX_STRUCT_LITERAL: {
            // Push all field values onto stack
            for (size_t i = 0; i < e->as.struct_literal.field_count; i++) {
                emit_expr(fe, e->as.struct_literal.field_values[i]);
            }
            buf_u8(&fe->code, OP_STRUCT_NEW);
            buf_u16(&fe->code, 0);  // struct type ID (simple for now)
            buf_u16(&fe->code, (uint16_t)e->as.struct_literal.field_count);
            return;
        }
        case EX_FIELD_ACCESS: {
            emit_expr(fe, e->as.field_access.object);
            // Use CLASS_GET for classes, STRUCT_GET for structs
            if (e->as.field_access.object->inferred.kind == TY_CLASS) {
                buf_u8(&fe->code, OP_CLASS_GET);
            } else {
                buf_u8(&fe->code, OP_STRUCT_GET);
            }
            buf_u16(&fe->code, (uint16_t)e->as.field_access.field_index);
            return;
        }
        case EX_NEW: {
            // Push constructor arguments onto stack
            for (size_t i = 0; i < e->as.new_expr.argc; i++) {
                emit_expr(fe, e->as.new_expr.args[i]);
            }
            buf_u8(&fe->code, OP_CLASS_NEW);
            buf_u16(&fe->code, (uint16_t)e->as.new_expr.class_index);
            buf_u8(&fe->code, (uint8_t)e->as.new_expr.argc);
            return;
        }
        case EX_SUPER_CALL: {
            // For super(), push arguments and call parent constructor
            // For super.method(), push object (self) and arguments
            for (size_t i = 0; i < e->as.super_call.argc; i++) {
                emit_expr(fe, e->as.super_call.args[i]);
            }
            buf_u8(&fe->code, OP_SUPER_CALL);
            buf_u16(&fe->code, 0);  // Method ID (0 = constructor)
            buf_u8(&fe->code, (uint8_t)e->as.super_call.argc);
            return;
        }
        case EX_ENUM_MEMBER: {
            // Enum members are integer constants
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, (int64_t)e->as.enum_member.member_value);
            return;
        }
        case EX_METHOD_CALL: {
            // Method calls: push object (self) as first arg, then other args, then CALL
            // The type checker transforms module method calls to EX_CALL, so if we get
            // here it's a real method call on a struct/class instance
            emit_expr(fe, e->as.method_call.object);  // push self
            for (size_t i = 0; i < e->as.method_call.argc; i++) {
                emit_expr(fe, e->as.method_call.args[i]);
            }
            buf_u8(&fe->code, OP_METHOD_CALL);
            buf_u16(&fe->code, (uint16_t)e->as.method_call.method_index);
            buf_u8(&fe->code, (uint8_t)(e->as.method_call.argc + 1));  // +1 for self
            return;
        }
        case EX_TUPLE: {
            // Compile tuples as arrays
            for (size_t i = 0; i < e->as.tuple.len; i++) {
                emit_expr(fe, e->as.tuple.elements[i]);
            }
            buf_u8(&fe->code, OP_ARRAY_NEW);
            buf_u32(&fe->code, (uint32_t)e->as.tuple.len);
            return;
        }
        case EX_FSTRING: {
            // F-string interpolation: emit parts and concatenate
            if (e->as.fstring.partc == 0) {
                uint32_t sid = strpool_add(fe->pool, "");
                uint32_t local_id = fn_str_index(fe, sid);
                buf_u8(&fe->code, OP_CONST_STR);
                buf_u32(&fe->code, local_id);
                return;
            }
            // Emit first part
            emit_expr(fe, e->as.fstring.parts[0]);
            if (e->as.fstring.parts[0]->inferred.kind != TY_STR) {
                buf_u8(&fe->code, OP_CALL_BUILTIN);
                buf_u16(&fe->code, (uint16_t)BI_TO_STR);
                buf_u8(&fe->code, 1);
            }
            // Emit remaining parts and concatenate
            for (size_t i = 1; i < e->as.fstring.partc; i++) {
                emit_expr(fe, e->as.fstring.parts[i]);
                if (e->as.fstring.parts[i]->inferred.kind != TY_STR) {
                    buf_u8(&fe->code, OP_CALL_BUILTIN);
                    buf_u16(&fe->code, (uint16_t)BI_TO_STR);
                    buf_u8(&fe->code, 1);
                }
                buf_u8(&fe->code, OP_ADD_STR);
            }
            return;
        }
        case EX_LAMBDA: {
            // Compile lambda body as anonymous function
            // The type checker assigned this lambda an fn_index via struct_name
            const char *lname = e->inferred.struct_name;
            if (!lname) bp_fatal("lambda missing function name from type checker");

            FnEmit lfe;
            memset(&lfe, 0, sizeof(lfe));
            lfe.pool = fe->pool;
            lfe.module = fe->module;

            // Add parameters as locals
            for (size_t p = 0; p < e->as.lambda.paramc; p++)
                locals_add(&lfe.locals, e->as.lambda.params[p].name);

            // Compile body expression and return its result
            emit_expr(&lfe, e->as.lambda.body);
            buf_u8(&lfe.code, OP_RET);

            // Find the function slot - parse lambda index from name
            size_t lambda_num = 0;
            if (sscanf(lname, "__lambda_%zu", &lambda_num) != 1) {
                bp_fatal("bad lambda name: %s", lname);
            }
            size_t fn_idx = fe->module->fn_len - typecheck_lambda_count() + lambda_num;

            BpFunc *lfn = &fe->module->funcs[fn_idx];
            if (lfn->name) free(lfn->name);
            lfn->name = bp_xstrdup(lname);
            lfn->arity = (uint16_t)e->as.lambda.paramc;
            lfn->locals = (uint16_t)lfe.locals.len;
            lfn->code = lfe.code.data;
            lfn->code_len = lfe.code.len;
            lfn->str_const_ids = lfe.str_ids;
            lfn->str_const_len = lfe.str_len;
            locals_free(&lfe.locals);

            // Push the function index as integer (stored in variable slot)
            buf_u8(&fe->code, OP_CONST_I64);
            buf_i64(&fe->code, (int64_t)fn_idx);
            return;
        }
        default: break;
    }
    bp_fatal("unknown expr");
}

BpModule compile_module(const Module *m) {
    BpModule out;
    memset(&out, 0, sizeof(out));

    StrPool pool = {0};

    // Count total functions: module functions + all class methods + lambdas
    size_t total_methods = 0;
    for (size_t i = 0; i < m->classc; i++) {
        total_methods += m->classes[i].method_count;
    }
    size_t lambda_count = typecheck_lambda_count();

    out.fn_len = m->fnc + total_methods + lambda_count;
    out.funcs = bp_xmalloc(out.fn_len * sizeof(*out.funcs));
    memset(out.funcs, 0, out.fn_len * sizeof(*out.funcs));

    out.entry = 0;
    for (size_t i = 0; i < m->fnc; i++) {
        if (strcmp(m->fns[i].name, "main") == 0) out.entry = (uint32_t)i;
    }

    // Store global variable count for VM
    out.global_count = m->global_varc;
    if (m->global_varc > 0) {
        out.globals = bp_xmalloc(m->global_varc * sizeof(Value));
        memset(out.globals, 0, m->global_varc * sizeof(Value));
    }

    // Populate global variable name table for cross-function access
    g_global_count = m->global_varc;
    if (m->global_varc > 0) {
        g_global_names = bp_xmalloc(m->global_varc * sizeof(char *));
        for (size_t g = 0; g < m->global_varc; g++) {
            g_global_names[g] = m->global_vars[g]->as.let.name;
        }
    }

    for (size_t i = 0; i < m->fnc; i++) {
        const Function *f = &m->fns[i];

        FnEmit fe;
        memset(&fe, 0, sizeof(fe));
        fe.pool = &pool;
        fe.module = &out;

        // params occupy first locals
        for (size_t p = 0; p < f->paramc; p++) locals_add(&fe.locals, f->params[p].name);

        // If this is the entry function, emit global variable initializers
        if (i == out.entry) {
            for (size_t g = 0; g < m->global_varc; g++) {
                if (m->global_vars[g]->kind == ST_LET && m->global_vars[g]->as.let.init) {
                    emit_expr(&fe, m->global_vars[g]->as.let.init);
                    buf_u8(&fe.code, OP_STORE_GLOBAL);
                    buf_u16(&fe.code, (uint16_t)g);
                }
            }
        }

        for (size_t s = 0; s < f->body_len; s++) emit_stmt(&fe, f->body[s]);

        // implicit return if missing
        buf_u8(&fe.code, OP_CONST_I64);
        buf_i64(&fe.code, 0);
        buf_u8(&fe.code, OP_RET);

        out.funcs[i].name = bp_xstrdup(f->name);
        out.funcs[i].arity = (uint16_t)f->paramc;
        out.funcs[i].locals = (uint16_t)fe.locals.len;
        out.funcs[i].code = fe.code.data;
        out.funcs[i].code_len = fe.code.len;
        out.funcs[i].str_const_ids = fe.str_ids;
        out.funcs[i].str_const_len = fe.str_len;

        locals_free(&fe.locals);
    }

    // Compile class definitions and their methods
    size_t method_fn_idx = m->fnc; // methods start after module functions
    out.class_type_len = m->classc;
    if (m->classc > 0) {
        out.class_types = bp_xmalloc(m->classc * sizeof(*out.class_types));
        memset(out.class_types, 0, m->classc * sizeof(*out.class_types));
        for (size_t i = 0; i < m->classc; i++) {
            const ClassDef *cd = &m->classes[i];
            out.class_types[i].name = bp_xstrdup(cd->name);
            out.class_types[i].parent_name = cd->parent_name ? bp_xstrdup(cd->parent_name) : NULL;
            out.class_types[i].field_count = cd->field_count;
            if (cd->field_count > 0) {
                out.class_types[i].field_names = bp_xmalloc(cd->field_count * sizeof(char *));
                for (size_t j = 0; j < cd->field_count; j++) {
                    out.class_types[i].field_names[j] = bp_xstrdup(cd->field_names[j]);
                }
            }
            out.class_types[i].method_count = cd->method_count;
            if (cd->method_count > 0) {
                out.class_types[i].method_ids = bp_xmalloc(cd->method_count * sizeof(uint16_t));
                out.class_types[i].method_names = bp_xmalloc(cd->method_count * sizeof(char *));
                for (size_t j = 0; j < cd->method_count; j++) {
                    const MethodDef *md = &cd->methods[j];
                    out.class_types[i].method_names[j] = bp_xstrdup(md->name);
                    out.class_types[i].method_ids[j] = (uint16_t)method_fn_idx;

                    // Compile the method as a function
                    FnEmit fe;
                    memset(&fe, 0, sizeof(fe));
                    fe.pool = &pool;
                    fe.module = &out;
                    for (size_t p = 0; p < md->paramc; p++)
                        locals_add(&fe.locals, md->params[p].name);
                    for (size_t s = 0; s < md->body_len; s++)
                        emit_stmt(&fe, md->body[s]);
                    buf_u8(&fe.code, OP_CONST_I64);
                    buf_i64(&fe.code, 0);
                    buf_u8(&fe.code, OP_RET);

                    char mname[256];
                    snprintf(mname, sizeof(mname), "%s$%s", cd->name, md->name);
                    out.funcs[method_fn_idx].name = bp_xstrdup(mname);
                    out.funcs[method_fn_idx].arity = (uint16_t)md->paramc;
                    out.funcs[method_fn_idx].locals = (uint16_t)fe.locals.len;
                    out.funcs[method_fn_idx].code = fe.code.data;
                    out.funcs[method_fn_idx].code_len = fe.code.len;
                    out.funcs[method_fn_idx].str_const_ids = fe.str_ids;
                    out.funcs[method_fn_idx].str_const_len = fe.str_len;
                    locals_free(&fe.locals);
                    method_fn_idx++;
                }
            }
        }
    }

    // Compile extern function declarations (FFI metadata)
    out.extern_func_len = m->externc;
    if (m->externc > 0) {
        out.extern_funcs = bp_xmalloc(m->externc * sizeof(*out.extern_funcs));
        memset(out.extern_funcs, 0, m->externc * sizeof(*out.extern_funcs));
        for (size_t i = 0; i < m->externc; i++) {
            const ExternDef *ed = &m->externs[i];
            out.extern_funcs[i].name = bp_xstrdup(ed->name);
            out.extern_funcs[i].c_name = ed->c_name ? bp_xstrdup(ed->c_name) : bp_xstrdup(ed->name);
            out.extern_funcs[i].library = bp_xstrdup(ed->library);
            out.extern_funcs[i].param_count = (uint16_t)ed->paramc;
            out.extern_funcs[i].is_variadic = ed->is_variadic;
            out.extern_funcs[i].ret_type = type_to_ffi_tc(ed->ret_type.kind);
            out.extern_funcs[i].handle = NULL;
            out.extern_funcs[i].fn_ptr = NULL;
            if (ed->paramc > 0) {
                out.extern_funcs[i].param_types = bp_xmalloc(ed->paramc);
                for (size_t j = 0; j < ed->paramc; j++) {
                    out.extern_funcs[i].param_types[j] = type_to_ffi_tc(ed->params[j].type.kind);
                }
            }
        }
    }

    out.str_len = pool.len;
    out.strings = pool.strings;

    // Clean up global variable tracking
    free(g_global_names);
    g_global_names = NULL;
    g_global_count = 0;

    return out;
}
