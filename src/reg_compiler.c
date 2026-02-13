/*
 * BetterPython Register-Based Compiler
 * Generates register-based bytecode from AST
 *
 * Key differences from stack compiler:
 * - emit_expr returns register number instead of pushing to stack
 * - 3-address instructions: dst, src1, src2
 * - Register allocator manages virtual registers
 */

#include "reg_compiler.h"
#include "regalloc.h"
#include "types.h"
#include "util.h"
#include <string.h>

// ============================================================================
// Bytecode Buffer
// ============================================================================

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

// ============================================================================
// String Pool
// ============================================================================

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

// ============================================================================
// Builtin ID lookup (same as stack compiler)
// ============================================================================

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

    // StringBuilder-like operations (with aliases)
    if (strcmp(name, "str_split") == 0) return BI_STR_SPLIT_STR;
    if (strcmp(name, "str_split_str") == 0) return BI_STR_SPLIT_STR;
    if (strcmp(name, "str_join") == 0) return BI_STR_JOIN_ARR;
    if (strcmp(name, "str_join_arr") == 0) return BI_STR_JOIN_ARR;
    if (strcmp(name, "str_concat_all") == 0) return BI_STR_CONCAT_ALL;

    // Bitwise operations
    if (strcmp(name, "bit_and") == 0) return BI_BIT_AND;
    if (strcmp(name, "bit_or") == 0) return BI_BIT_OR;
    if (strcmp(name, "bit_xor") == 0) return BI_BIT_XOR;
    if (strcmp(name, "bit_not") == 0) return BI_BIT_NOT;
    if (strcmp(name, "bit_shl") == 0) return BI_BIT_SHL;
    if (strcmp(name, "bit_shr") == 0) return BI_BIT_SHR;

    // Byte conversions
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
    if (strcmp(name, "file_read_bytes") == 0) return BI_FILE_READ_BYTES;
    if (strcmp(name, "file_write_bytes") == 0) return BI_FILE_WRITE_BYTES;
    if (strcmp(name, "array_insert") == 0) return BI_ARRAY_INSERT;
    if (strcmp(name, "array_remove") == 0) return BI_ARRAY_REMOVE;
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

// ============================================================================
// Jump Patching
// ============================================================================

static void emit_jump_patch(Buf *b, size_t at, uint32_t target) {
    memcpy(b->data + at, &target, 4);
}

// ============================================================================
// Global Variable Tracking
// ============================================================================

static char **g_global_names = NULL;
static size_t g_global_count = 0;

static int global_get(const char *name) {
    for (size_t i = 0; i < g_global_count; i++) {
        if (strcmp(g_global_names[i], name) == 0) return (int)i;
    }
    return -1;
}

// ============================================================================
// Loop Context for Break/Continue
// ============================================================================

typedef struct {
    size_t *break_patches;
    size_t break_count;
    size_t break_cap;
    size_t *continue_patches;
    size_t continue_count;
    size_t continue_cap;
    uint32_t continue_target;
} LoopCtx;

// ============================================================================
// Function Emitter Context
// ============================================================================

typedef struct {
    Buf code;
    uint32_t *str_ids;
    size_t str_len;
    size_t str_cap;
    RegAlloc ra;
    StrPool *pool;
    BpModule *module;   // For lambda compilation

    // Loop context stack
    LoopCtx *loops;
    size_t loop_count;
    size_t loop_cap;
} RegFnEmit;

static void push_loop(RegFnEmit *fe, uint32_t continue_target) {
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

static void add_break_patch(RegFnEmit *fe, size_t at) {
    if (fe->loop_count == 0) bp_fatal("break outside of loop");
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    if (ctx->break_count + 1 > ctx->break_cap) {
        ctx->break_cap = ctx->break_cap ? ctx->break_cap * 2 : 8;
        ctx->break_patches = bp_xrealloc(ctx->break_patches, ctx->break_cap * sizeof(size_t));
    }
    ctx->break_patches[ctx->break_count++] = at;
}

static void add_continue_patch(RegFnEmit *fe, size_t at) {
    if (fe->loop_count == 0) bp_fatal("continue outside of loop");
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    if (ctx->continue_count + 1 > ctx->continue_cap) {
        ctx->continue_cap = ctx->continue_cap ? ctx->continue_cap * 2 : 8;
        ctx->continue_patches = bp_xrealloc(ctx->continue_patches, ctx->continue_cap * sizeof(size_t));
    }
    ctx->continue_patches[ctx->continue_count++] = at;
}

static void patch_breaks(RegFnEmit *fe, uint32_t target) {
    if (fe->loop_count == 0) return;
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    for (size_t i = 0; i < ctx->break_count; i++) {
        emit_jump_patch(&fe->code, ctx->break_patches[i], target);
    }
    free(ctx->break_patches);
}

static void patch_continues(RegFnEmit *fe, uint32_t target) {
    if (fe->loop_count == 0) return;
    LoopCtx *ctx = &fe->loops[fe->loop_count - 1];
    for (size_t i = 0; i < ctx->continue_count; i++) {
        emit_jump_patch(&fe->code, ctx->continue_patches[i], target);
    }
    free(ctx->continue_patches);
}

static void pop_loop(RegFnEmit *fe) {
    if (fe->loop_count > 0) fe->loop_count--;
}

// ============================================================================
// String Reference Management
// ============================================================================

static void fn_add_strref(RegFnEmit *fe, uint32_t sid) {
    if (fe->str_len + 1 > fe->str_cap) {
        fe->str_cap = fe->str_cap ? fe->str_cap * 2 : 16;
        fe->str_ids = bp_xrealloc(fe->str_ids, fe->str_cap * sizeof(uint32_t));
    }
    fe->str_ids[fe->str_len++] = sid;
}

static uint32_t fn_str_index(RegFnEmit *fe, uint32_t pool_id) {
    for (size_t i = 0; i < fe->str_len; i++) {
        if (fe->str_ids[i] == pool_id) return (uint32_t)i;
    }
    fn_add_strref(fe, pool_id);
    return (uint32_t)(fe->str_len - 1);
}

// ============================================================================
// Expression Emission
// Returns the register containing the result
// ============================================================================

static uint8_t reg_emit_expr(RegFnEmit *fe, const Expr *e);

static void reg_emit_stmt(RegFnEmit *fe, const Stmt *s) {
    switch (s->kind) {
        case ST_LET: {
            // Evaluate init expression
            uint8_t val_reg = reg_emit_expr(fe, s->as.let.init);
            // Allocate register for variable
            uint8_t var_reg = reg_alloc_var(&fe->ra, s->as.let.name);
            // Move if different registers
            if (val_reg != var_reg) {
                buf_u8(&fe->code, R_MOV);
                buf_u8(&fe->code, var_reg);
                buf_u8(&fe->code, val_reg);
            }
            // Free temp if it was a temp
            if (val_reg != var_reg) {
                reg_free_temp(&fe->ra, val_reg);
            }
            return;
        }
        case ST_ASSIGN: {
            // Evaluate value expression
            uint8_t val_reg = reg_emit_expr(fe, s->as.assign.value);
            // Check if it's a local variable
            if (reg_has_var(&fe->ra, s->as.assign.name)) {
                uint8_t var_reg = reg_get_var(&fe->ra, s->as.assign.name);
                if (val_reg != var_reg) {
                    buf_u8(&fe->code, R_MOV);
                    buf_u8(&fe->code, var_reg);
                    buf_u8(&fe->code, val_reg);
                    reg_free_temp(&fe->ra, val_reg);
                }
            } else {
                // Check if it's a global variable
                int gi = global_get(s->as.assign.name);
                if (gi < 0) bp_fatal("unknown variable '%s'", s->as.assign.name);
                buf_u8(&fe->code, R_STORE_GLOBAL);
                buf_u8(&fe->code, val_reg);
                buf_u16(&fe->code, (uint16_t)gi);
                reg_free_temp(&fe->ra, val_reg);
            }
            return;
        }
        case ST_INDEX_ASSIGN: {
            // container[key] = value
            uint8_t arr_reg = reg_emit_expr(fe, s->as.idx_assign.array);
            uint8_t idx_reg = reg_emit_expr(fe, s->as.idx_assign.index);
            uint8_t val_reg = reg_emit_expr(fe, s->as.idx_assign.value);

            if (s->as.idx_assign.array->inferred.kind == TY_MAP) {
                buf_u8(&fe->code, R_MAP_SET);
            } else {
                buf_u8(&fe->code, R_ARRAY_SET);
            }
            buf_u8(&fe->code, arr_reg);
            buf_u8(&fe->code, idx_reg);
            buf_u8(&fe->code, val_reg);

            reg_free_temp(&fe->ra, val_reg);
            reg_free_temp(&fe->ra, idx_reg);
            reg_free_temp(&fe->ra, arr_reg);
            return;
        }
        case ST_EXPR: {
            // Expression statement - evaluate and discard result
            uint8_t reg = reg_emit_expr(fe, s->as.expr.expr);
            reg_free_temp(&fe->ra, reg);
            return;
        }
        case ST_RETURN: {
            if (s->as.ret.value) {
                uint8_t val_reg = reg_emit_expr(fe, s->as.ret.value);
                buf_u8(&fe->code, R_RET);
                buf_u8(&fe->code, val_reg);
            } else {
                // Return 0 for void functions
                uint8_t tmp = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_CONST_I64);
                buf_u8(&fe->code, tmp);
                buf_i64(&fe->code, 0);
                buf_u8(&fe->code, R_RET);
                buf_u8(&fe->code, tmp);
                reg_free_temp(&fe->ra, tmp);
            }
            return;
        }
        case ST_IF: {
            // Evaluate condition
            uint8_t cond_reg = reg_emit_expr(fe, s->as.ifs.cond);

            // Jump if false to else branch
            buf_u8(&fe->code, R_JMP_IF_FALSE);
            buf_u8(&fe->code, cond_reg);
            size_t jmp_false_at = fe->code.len;
            buf_u32(&fe->code, 0);

            reg_free_temp(&fe->ra, cond_reg);

            // Then branch
            for (size_t i = 0; i < s->as.ifs.then_len; i++) {
                reg_emit_stmt(fe, s->as.ifs.then_stmts[i]);
            }

            // Jump to end
            buf_u8(&fe->code, R_JMP);
            size_t jmp_end_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Patch jump to else
            emit_jump_patch(&fe->code, jmp_false_at, (uint32_t)fe->code.len);

            // Else branch
            for (size_t i = 0; i < s->as.ifs.else_len; i++) {
                reg_emit_stmt(fe, s->as.ifs.else_stmts[i]);
            }

            // Patch jump to end
            emit_jump_patch(&fe->code, jmp_end_at, (uint32_t)fe->code.len);
            return;
        }
        case ST_WHILE: {
            uint32_t loop_start = (uint32_t)fe->code.len;
            push_loop(fe, loop_start);

            // Condition
            uint8_t cond_reg = reg_emit_expr(fe, s->as.wh.cond);
            buf_u8(&fe->code, R_JMP_IF_FALSE);
            buf_u8(&fe->code, cond_reg);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);
            reg_free_temp(&fe->ra, cond_reg);

            // Body
            for (size_t i = 0; i < s->as.wh.body_len; i++) {
                reg_emit_stmt(fe, s->as.wh.body[i]);
            }

            // Jump back to condition
            buf_u8(&fe->code, R_JMP);
            buf_u32(&fe->code, loop_start);

            // Patch exit jump
            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);
            return;
        }
        case ST_FOR: {
            // Initialize loop variable
            uint8_t start_reg = reg_emit_expr(fe, s->as.forr.start);
            uint8_t iter_reg = reg_alloc_var(&fe->ra, s->as.forr.var);
            if (start_reg != iter_reg) {
                buf_u8(&fe->code, R_MOV);
                buf_u8(&fe->code, iter_reg);
                buf_u8(&fe->code, start_reg);
                reg_free_temp(&fe->ra, start_reg);
            }

            // Loop start
            uint32_t loop_start = (uint32_t)fe->code.len;
            push_loop(fe, 0);  // continue_target will be patched

            // Condition: iter < end
            uint8_t end_reg = reg_emit_expr(fe, s->as.forr.end);
            uint8_t cond_reg = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_LT);
            buf_u8(&fe->code, cond_reg);
            buf_u8(&fe->code, iter_reg);
            buf_u8(&fe->code, end_reg);
            reg_free_temp(&fe->ra, end_reg);

            // Jump if false
            buf_u8(&fe->code, R_JMP_IF_FALSE);
            buf_u8(&fe->code, cond_reg);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);
            reg_free_temp(&fe->ra, cond_reg);

            // Body
            for (size_t i = 0; i < s->as.forr.body_len; i++) {
                reg_emit_stmt(fe, s->as.forr.body[i]);
            }

            // Continue target: increment
            uint32_t continue_pos = (uint32_t)fe->code.len;
            patch_continues(fe, continue_pos);

            // iter = iter + 1
            uint8_t one_reg = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, one_reg);
            buf_i64(&fe->code, 1);

            buf_u8(&fe->code, R_ADD_I64);
            buf_u8(&fe->code, iter_reg);
            buf_u8(&fe->code, iter_reg);
            buf_u8(&fe->code, one_reg);
            reg_free_temp(&fe->ra, one_reg);

            // Jump back
            buf_u8(&fe->code, R_JMP);
            buf_u32(&fe->code, loop_start);

            // Patch exit
            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);
            return;
        }
        case ST_FOR_IN: {
            // for x in collection:
            // Desugars to: coll = expr (or map_keys(expr)), len = array_len(coll),
            //              idx = 0, while idx < len: x = coll[idx]; body; idx++
            Type coll_type = s->as.for_in.collection->inferred;
            bool is_map = (coll_type.kind == TY_MAP);

            // Evaluate collection
            uint8_t coll_src = reg_emit_expr(fe, s->as.for_in.collection);
            uint8_t coll_reg = reg_alloc_var(&fe->ra, "__for_coll");
            buf_u8(&fe->code, R_MOV);
            buf_u8(&fe->code, coll_reg);
            buf_u8(&fe->code, coll_src);
            if (coll_src != coll_reg) reg_free_temp(&fe->ra, coll_src);

            // For maps: coll = map_keys(coll)
            if (is_map) {
                uint8_t arg_reg = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_MOV);
                buf_u8(&fe->code, arg_reg);
                buf_u8(&fe->code, coll_reg);
                buf_u8(&fe->code, R_CALL_BUILTIN);
                buf_u8(&fe->code, coll_reg);
                buf_u16(&fe->code, (uint16_t)BI_MAP_KEYS);
                buf_u8(&fe->code, arg_reg);
                buf_u8(&fe->code, 1);
                reg_free_temp(&fe->ra, arg_reg);
            }

            // len = array_len(coll)
            uint8_t len_reg = reg_alloc_var(&fe->ra, "__for_len");
            {
                uint8_t arg_reg = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_MOV);
                buf_u8(&fe->code, arg_reg);
                buf_u8(&fe->code, coll_reg);
                buf_u8(&fe->code, R_CALL_BUILTIN);
                buf_u8(&fe->code, len_reg);
                buf_u16(&fe->code, (uint16_t)BI_ARRAY_LEN);
                buf_u8(&fe->code, arg_reg);
                buf_u8(&fe->code, 1);
                reg_free_temp(&fe->ra, arg_reg);
            }

            // idx = 0
            uint8_t idx_reg = reg_alloc_var(&fe->ra, "__for_idx");
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, idx_reg);
            buf_i64(&fe->code, 0);

            // Loop start
            uint32_t loop_start = (uint32_t)fe->code.len;
            push_loop(fe, 0);

            // Condition: idx < len
            uint8_t cond_reg = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_LT);
            buf_u8(&fe->code, cond_reg);
            buf_u8(&fe->code, idx_reg);
            buf_u8(&fe->code, len_reg);

            buf_u8(&fe->code, R_JMP_IF_FALSE);
            buf_u8(&fe->code, cond_reg);
            size_t jmp_out_at = fe->code.len;
            buf_u32(&fe->code, 0);
            reg_free_temp(&fe->ra, cond_reg);

            // x = coll[idx]
            uint8_t var_reg = reg_alloc_var(&fe->ra, s->as.for_in.var);
            buf_u8(&fe->code, R_ARRAY_GET);
            buf_u8(&fe->code, var_reg);
            buf_u8(&fe->code, coll_reg);
            buf_u8(&fe->code, idx_reg);

            // Body
            for (size_t i = 0; i < s->as.for_in.body_len; i++) {
                reg_emit_stmt(fe, s->as.for_in.body[i]);
            }

            // continue_target: idx++
            uint32_t continue_pos = (uint32_t)fe->code.len;
            patch_continues(fe, continue_pos);

            uint8_t one_reg = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, one_reg);
            buf_i64(&fe->code, 1);
            buf_u8(&fe->code, R_ADD_I64);
            buf_u8(&fe->code, idx_reg);
            buf_u8(&fe->code, idx_reg);
            buf_u8(&fe->code, one_reg);
            reg_free_temp(&fe->ra, one_reg);

            // Jump back
            buf_u8(&fe->code, R_JMP);
            buf_u32(&fe->code, loop_start);

            // Patch exit
            uint32_t loop_end = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, jmp_out_at, loop_end);
            patch_breaks(fe, loop_end);
            pop_loop(fe);
            return;
        }
        case ST_MATCH: {
            // Match compiles as a chain of equality checks (if/elif style)
            // Evaluate match expression into a register
            uint8_t match_reg = reg_emit_expr(fe, s->as.match.expr);

            size_t *end_patches = NULL;
            size_t end_count = 0, end_cap = 0;

            for (size_t i = 0; i < s->as.match.case_count; i++) {
                if (s->as.match.case_values[i] == NULL) continue; // skip default

                // Evaluate case value
                uint8_t case_reg = reg_emit_expr(fe, s->as.match.case_values[i]);

                // Compare: tmp = (match_reg == case_reg)
                uint8_t cmp_reg = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_EQ);
                buf_u8(&fe->code, cmp_reg);
                buf_u8(&fe->code, match_reg);
                buf_u8(&fe->code, case_reg);

                reg_free_temp(&fe->ra, case_reg);

                // JMP_IF_FALSE to next case
                buf_u8(&fe->code, R_JMP_IF_FALSE);
                buf_u8(&fe->code, cmp_reg);
                size_t skip_at = fe->code.len;
                buf_u32(&fe->code, 0);

                reg_free_temp(&fe->ra, cmp_reg);

                // Case body
                for (size_t j = 0; j < s->as.match.case_body_lens[i]; j++)
                    reg_emit_stmt(fe, s->as.match.case_bodies[i][j]);

                // Jump to end
                buf_u8(&fe->code, R_JMP);
                if (end_count + 1 > end_cap) {
                    end_cap = end_cap ? end_cap * 2 : 8;
                    end_patches = bp_xrealloc(end_patches, end_cap * sizeof(size_t));
                }
                end_patches[end_count++] = fe->code.len;
                buf_u32(&fe->code, 0);

                // Patch skip
                emit_jump_patch(&fe->code, skip_at, (uint32_t)fe->code.len);
            }

            // Default case
            if (s->as.match.has_default) {
                size_t di = s->as.match.default_idx;
                for (size_t j = 0; j < s->as.match.case_body_lens[di]; j++)
                    reg_emit_stmt(fe, s->as.match.case_bodies[di][j]);
            }

            // Patch all end jumps
            uint32_t end_addr = (uint32_t)fe->code.len;
            for (size_t i = 0; i < end_count; i++)
                emit_jump_patch(&fe->code, end_patches[i], end_addr);
            free(end_patches);

            reg_free_temp(&fe->ra, match_reg);
            return;
        }
        case ST_BREAK: {
            if (fe->loop_count == 0) bp_fatal("break outside of loop");
            buf_u8(&fe->code, R_JMP);
            add_break_patch(fe, fe->code.len);
            buf_u32(&fe->code, 0);
            return;
        }
        case ST_CONTINUE: {
            if (fe->loop_count == 0) bp_fatal("continue outside of loop");
            buf_u8(&fe->code, R_JMP);
            if (fe->loops[fe->loop_count - 1].continue_target == 0) {
                add_continue_patch(fe, fe->code.len);
                buf_u32(&fe->code, 0);
            } else {
                buf_u32(&fe->code, fe->loops[fe->loop_count - 1].continue_target);
            }
            return;
        }
        case ST_TRY: {
            // Allocate register for exception variable if needed
            uint8_t exc_reg = 0;
            if (s->as.try_catch.catch_var) {
                exc_reg = reg_alloc_var(&fe->ra, s->as.try_catch.catch_var);
            }

            // OP_TRY_BEGIN <catch_offset> <finally_offset> <exc_reg>
            buf_u8(&fe->code, R_TRY_BEGIN);
            size_t catch_addr_at = fe->code.len;
            buf_u32(&fe->code, 0);  // catch offset placeholder
            size_t finally_addr_at = fe->code.len;
            buf_u32(&fe->code, 0);  // finally offset placeholder
            buf_u8(&fe->code, exc_reg);

            // Try body
            for (size_t i = 0; i < s->as.try_catch.try_len; i++) {
                reg_emit_stmt(fe, s->as.try_catch.try_stmts[i]);
            }

            // TRY_END
            buf_u8(&fe->code, R_TRY_END);

            // Jump to finally/end
            buf_u8(&fe->code, R_JMP);
            size_t skip_catch_at = fe->code.len;
            buf_u32(&fe->code, 0);

            // Patch catch address
            emit_jump_patch(&fe->code, catch_addr_at, (uint32_t)fe->code.len);

            // Catch body
            for (size_t i = 0; i < s->as.try_catch.catch_len; i++) {
                reg_emit_stmt(fe, s->as.try_catch.catch_stmts[i]);
            }

            // Patch skip_catch
            uint32_t finally_addr = (uint32_t)fe->code.len;
            emit_jump_patch(&fe->code, skip_catch_at, finally_addr);

            // Finally body if present
            if (s->as.try_catch.finally_len > 0) {
                emit_jump_patch(&fe->code, finally_addr_at, finally_addr);
                for (size_t i = 0; i < s->as.try_catch.finally_len; i++) {
                    reg_emit_stmt(fe, s->as.try_catch.finally_stmts[i]);
                }
            }
            return;
        }
        case ST_THROW: {
            uint8_t val_reg = reg_emit_expr(fe, s->as.throw.value);
            buf_u8(&fe->code, R_THROW);
            buf_u8(&fe->code, val_reg);
            return;
        }
        case ST_FIELD_ASSIGN: {
            // obj.field = value
            const Expr *fa = s->as.field_assign.object;
            uint8_t obj_reg = reg_emit_expr(fe, fa->as.field_access.object);
            uint8_t val_reg = reg_emit_expr(fe, s->as.field_assign.value);
            if (fa->as.field_access.object->inferred.kind == TY_CLASS) {
                buf_u8(&fe->code, R_CLASS_SET);
            } else {
                buf_u8(&fe->code, R_STRUCT_SET);
            }
            buf_u8(&fe->code, obj_reg);
            buf_u16(&fe->code, (uint16_t)fa->as.field_access.field_index);
            buf_u8(&fe->code, val_reg);
            reg_free_temp(&fe->ra, obj_reg);
            reg_free_temp(&fe->ra, val_reg);
            return;
        }
        default:
            break;
    }
    bp_fatal("unknown stmt in register compiler");
}

static uint8_t reg_emit_expr(RegFnEmit *fe, const Expr *e) {
    switch (e->kind) {
        case EX_INT: {
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, dst);
            buf_i64(&fe->code, e->as.int_val);
            return dst;
        }
        case EX_FLOAT: {
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_F64);
            buf_u8(&fe->code, dst);
            buf_f64(&fe->code, e->as.float_val);
            return dst;
        }
        case EX_BOOL: {
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_BOOL);
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, e->as.bool_val ? 1 : 0);
            return dst;
        }
        case EX_STR: {
            uint32_t sid = strpool_add(fe->pool, e->as.str_val);
            uint32_t local_id = fn_str_index(fe, sid);
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_STR);
            buf_u8(&fe->code, dst);
            buf_u32(&fe->code, local_id);
            return dst;
        }
        case EX_VAR: {
            // Check if it's a local/parameter variable first
            if (reg_has_var(&fe->ra, e->as.var_name)) {
                return reg_get_var(&fe->ra, e->as.var_name);
            }
            // Check if it's a global variable
            int gi = global_get(e->as.var_name);
            if (gi >= 0) {
                uint8_t dst = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_LOAD_GLOBAL);
                buf_u8(&fe->code, dst);
                buf_u16(&fe->code, (uint16_t)gi);
                return dst;
            }
            bp_fatal("unknown variable '%s'", e->as.var_name);
            return 0;
        }
        case EX_CALL: {
            // Allocate consecutive registers for arguments
            size_t argc = e->as.call.argc;
            uint8_t arg_base = 0;

            if (argc > 0) {
                // Allocate a contiguous block of registers for args
                arg_base = reg_alloc_block(&fe->ra, argc);

                // Evaluate arguments into consecutive registers
                for (size_t i = 0; i < argc; i++) {
                    uint8_t val_reg = reg_emit_expr(fe, e->as.call.args[i]);
                    if (val_reg != arg_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, arg_base + (uint8_t)i);
                        buf_u8(&fe->code, val_reg);
                        // Only free if it's a different temp
                        if (val_reg >= arg_base + argc || val_reg < arg_base) {
                            reg_free_temp(&fe->ra, val_reg);
                        }
                    }
                }
            }

            // Result register
            uint8_t dst = reg_alloc_temp(&fe->ra);

            if (e->as.call.fn_index <= -100) {
                // FFI extern function call
                uint16_t extern_id = (uint16_t)(-(e->as.call.fn_index + 100));
                buf_u8(&fe->code, R_FFI_CALL);
                buf_u8(&fe->code, dst);
                buf_u16(&fe->code, extern_id);
                buf_u8(&fe->code, arg_base);
                buf_u8(&fe->code, (uint8_t)argc);
            } else if (e->as.call.fn_index < 0) {
                // Builtin call
                BuiltinId id = builtin_id(e->as.call.name);
                buf_u8(&fe->code, R_CALL_BUILTIN);
                buf_u8(&fe->code, dst);
                buf_u16(&fe->code, (uint16_t)id);
                buf_u8(&fe->code, arg_base);
                buf_u8(&fe->code, (uint8_t)argc);
            } else {
                // User function call
                buf_u8(&fe->code, R_CALL);
                buf_u8(&fe->code, dst);
                buf_u32(&fe->code, (uint32_t)e->as.call.fn_index);
                buf_u8(&fe->code, arg_base);
                buf_u8(&fe->code, (uint8_t)argc);
            }

            // Free argument registers
            for (size_t i = 0; i < argc; i++) {
                reg_free_temp(&fe->ra, arg_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_UNARY: {
            uint8_t src = reg_emit_expr(fe, e->as.unary.rhs);
            uint8_t dst = reg_alloc_temp(&fe->ra);

            if (e->as.unary.op == UOP_NEG) {
                if (e->inferred.kind == TY_FLOAT) {
                    buf_u8(&fe->code, R_NEG_F64);
                } else {
                    buf_u8(&fe->code, R_NEG_I64);
                }
            } else if (e->as.unary.op == UOP_BIT_NOT) {
                buf_u8(&fe->code, R_BIT_NOT);
            } else {
                buf_u8(&fe->code, R_NOT);
            }
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, src);

            reg_free_temp(&fe->ra, src);
            return dst;
        }
        case EX_BINARY: {
            // Short-circuit evaluation for AND/OR would be complex
            // For simplicity, evaluate both sides (can optimize later)
            uint8_t lhs = reg_emit_expr(fe, e->as.binary.lhs);
            uint8_t rhs = reg_emit_expr(fe, e->as.binary.rhs);
            uint8_t dst = reg_alloc_temp(&fe->ra);

            bool is_float = (e->inferred.kind == TY_FLOAT) ||
                             (e->as.binary.lhs->inferred.kind == TY_FLOAT) ||
                             (e->as.binary.rhs->inferred.kind == TY_FLOAT);

            switch (e->as.binary.op) {
                case BOP_ADD:
                    if (e->inferred.kind == TY_STR) buf_u8(&fe->code, R_ADD_STR);
                    else if (is_float) buf_u8(&fe->code, R_ADD_F64);
                    else buf_u8(&fe->code, R_ADD_I64);
                    break;
                case BOP_SUB:
                    buf_u8(&fe->code, is_float ? R_SUB_F64 : R_SUB_I64);
                    break;
                case BOP_MUL:
                    buf_u8(&fe->code, is_float ? R_MUL_F64 : R_MUL_I64);
                    break;
                case BOP_DIV:
                    buf_u8(&fe->code, is_float ? R_DIV_F64 : R_DIV_I64);
                    break;
                case BOP_MOD:
                    buf_u8(&fe->code, is_float ? R_MOD_F64 : R_MOD_I64);
                    break;
                case BOP_EQ:
                    buf_u8(&fe->code, R_EQ);
                    break;
                case BOP_NEQ:
                    buf_u8(&fe->code, R_NEQ);
                    break;
                case BOP_LT:
                    buf_u8(&fe->code, is_float ? R_LT_F64 : R_LT);
                    break;
                case BOP_LTE:
                    buf_u8(&fe->code, is_float ? R_LTE_F64 : R_LTE);
                    break;
                case BOP_GT:
                    buf_u8(&fe->code, is_float ? R_GT_F64 : R_GT);
                    break;
                case BOP_GTE:
                    buf_u8(&fe->code, is_float ? R_GTE_F64 : R_GTE);
                    break;
                case BOP_AND:
                    buf_u8(&fe->code, R_AND);
                    break;
                case BOP_OR:
                    buf_u8(&fe->code, R_OR);
                    break;
                case BOP_BIT_AND: buf_u8(&fe->code, R_BIT_AND); break;
                case BOP_BIT_OR:  buf_u8(&fe->code, R_BIT_OR); break;
                case BOP_BIT_XOR: buf_u8(&fe->code, R_BIT_XOR); break;
                case BOP_BIT_SHL: buf_u8(&fe->code, R_BIT_SHL); break;
                case BOP_BIT_SHR: buf_u8(&fe->code, R_BIT_SHR); break;
                default:
                    bp_fatal("unsupported binary op");
            }
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, lhs);
            buf_u8(&fe->code, rhs);

            reg_free_temp(&fe->ra, rhs);
            reg_free_temp(&fe->ra, lhs);
            return dst;
        }
        case EX_ARRAY: {
            // Allocate consecutive registers for elements
            size_t count = e->as.array.len;
            uint8_t src_base = 0;

            if (count > 0) {
                // Allocate a contiguous block of registers
                src_base = reg_alloc_block(&fe->ra, count);

                // Evaluate elements
                for (size_t i = 0; i < count; i++) {
                    uint8_t val = reg_emit_expr(fe, e->as.array.elements[i]);
                    if (val != src_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, src_base + (uint8_t)i);
                        buf_u8(&fe->code, val);
                        if (val >= src_base + count || val < src_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }

            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_ARRAY_NEW);
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, src_base);
            buf_u16(&fe->code, (uint16_t)count);

            // Free element registers
            for (size_t i = 0; i < count; i++) {
                reg_free_temp(&fe->ra, src_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_INDEX: {
            uint8_t arr = reg_emit_expr(fe, e->as.index.array);
            uint8_t idx = reg_emit_expr(fe, e->as.index.index);
            uint8_t dst = reg_alloc_temp(&fe->ra);

            if (e->as.index.array->inferred.kind == TY_MAP) {
                buf_u8(&fe->code, R_MAP_GET);
            } else {
                buf_u8(&fe->code, R_ARRAY_GET);
            }
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, arr);
            buf_u8(&fe->code, idx);

            reg_free_temp(&fe->ra, idx);
            reg_free_temp(&fe->ra, arr);
            return dst;
        }
        case EX_MAP: {
            // Allocate consecutive registers for key-value pairs
            size_t count = e->as.map.len;
            size_t total = count * 2;
            uint8_t src_base = 0;

            if (total > 0) {
                // Allocate a contiguous block of registers for key-value pairs
                src_base = reg_alloc_block(&fe->ra, total);

                // Evaluate key-value pairs
                for (size_t i = 0; i < count; i++) {
                    uint8_t key = reg_emit_expr(fe, e->as.map.keys[i]);
                    if (key != src_base + i * 2) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, src_base + (uint8_t)(i * 2));
                        buf_u8(&fe->code, key);
                        if (key >= src_base + total || key < src_base) {
                            reg_free_temp(&fe->ra, key);
                        }
                    }

                    uint8_t val = reg_emit_expr(fe, e->as.map.values[i]);
                    if (val != src_base + i * 2 + 1) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, src_base + (uint8_t)(i * 2 + 1));
                        buf_u8(&fe->code, val);
                        if (val >= src_base + total || val < src_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }

            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_MAP_NEW);
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, src_base);
            buf_u16(&fe->code, (uint16_t)count);

            // Free pair registers
            for (size_t i = 0; i < total; i++) {
                reg_free_temp(&fe->ra, src_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_STRUCT_LITERAL: {
            size_t count = e->as.struct_literal.field_count;
            uint8_t src_base = 0;

            if (count > 0) {
                // Allocate a contiguous block of registers for struct fields
                src_base = reg_alloc_block(&fe->ra, count);

                for (size_t i = 0; i < count; i++) {
                    uint8_t val = reg_emit_expr(fe, e->as.struct_literal.field_values[i]);
                    if (val != src_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, src_base + (uint8_t)i);
                        buf_u8(&fe->code, val);
                        if (val >= src_base + count || val < src_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }

            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_STRUCT_NEW);
            buf_u8(&fe->code, dst);
            buf_u16(&fe->code, 0);  // type ID
            buf_u8(&fe->code, src_base);
            buf_u8(&fe->code, (uint8_t)count);

            for (size_t i = 0; i < count; i++) {
                reg_free_temp(&fe->ra, src_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_FIELD_ACCESS: {
            uint8_t obj = reg_emit_expr(fe, e->as.field_access.object);
            uint8_t dst = reg_alloc_temp(&fe->ra);

            if (e->as.field_access.object->inferred.kind == TY_CLASS) {
                buf_u8(&fe->code, R_CLASS_GET);
            } else {
                buf_u8(&fe->code, R_STRUCT_GET);
            }
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, obj);
            buf_u16(&fe->code, (uint16_t)e->as.field_access.field_index);

            reg_free_temp(&fe->ra, obj);
            return dst;
        }
        case EX_NEW: {
            size_t argc = e->as.new_expr.argc;
            uint8_t arg_base = 0;

            if (argc > 0) {
                // Allocate a contiguous block of registers for constructor args
                arg_base = reg_alloc_block(&fe->ra, argc);

                for (size_t i = 0; i < argc; i++) {
                    uint8_t val = reg_emit_expr(fe, e->as.new_expr.args[i]);
                    if (val != arg_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, arg_base + (uint8_t)i);
                        buf_u8(&fe->code, val);
                        if (val >= arg_base + argc || val < arg_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }

            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CLASS_NEW);
            buf_u8(&fe->code, dst);
            buf_u16(&fe->code, (uint16_t)e->as.new_expr.class_index);
            buf_u8(&fe->code, arg_base);
            buf_u8(&fe->code, (uint8_t)argc);

            for (size_t i = 0; i < argc; i++) {
                reg_free_temp(&fe->ra, arg_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_SUPER_CALL: {
            size_t argc = e->as.super_call.argc;
            uint8_t arg_base = 0;

            if (argc > 0) {
                // Allocate a contiguous block of registers for super call args
                arg_base = reg_alloc_block(&fe->ra, argc);

                for (size_t i = 0; i < argc; i++) {
                    uint8_t val = reg_emit_expr(fe, e->as.super_call.args[i]);
                    if (val != arg_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, arg_base + (uint8_t)i);
                        buf_u8(&fe->code, val);
                        if (val >= arg_base + argc || val < arg_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }

            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_SUPER_CALL);
            buf_u8(&fe->code, dst);
            buf_u16(&fe->code, 0);  // method ID
            buf_u8(&fe->code, arg_base);
            buf_u8(&fe->code, (uint8_t)argc);

            for (size_t i = 0; i < argc; i++) {
                reg_free_temp(&fe->ra, arg_base + (uint8_t)i);
            }

            return dst;
        }
        case EX_ENUM_MEMBER: {
            // Enum members are integer constants
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, dst);
            buf_i64(&fe->code, (int64_t)e->as.enum_member.member_value);
            return dst;
        }
        case EX_TUPLE: {
            // Tuples compile as arrays
            size_t count = e->as.tuple.len;
            uint8_t src_base = 0;
            if (count > 0) {
                src_base = reg_alloc_block(&fe->ra, count);
                for (size_t i = 0; i < count; i++) {
                    uint8_t val = reg_emit_expr(fe, e->as.tuple.elements[i]);
                    if (val != src_base + i) {
                        buf_u8(&fe->code, R_MOV);
                        buf_u8(&fe->code, src_base + (uint8_t)i);
                        buf_u8(&fe->code, val);
                        if (val >= src_base + count || val < src_base) {
                            reg_free_temp(&fe->ra, val);
                        }
                    }
                }
            }
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_ARRAY_NEW);
            buf_u8(&fe->code, dst);
            buf_u8(&fe->code, src_base);
            buf_u16(&fe->code, (uint16_t)count);
            for (size_t i = 0; i < count; i++) {
                reg_free_temp(&fe->ra, src_base + (uint8_t)i);
            }
            return dst;
        }
        case EX_LAMBDA: {
            // Lambda compiles to its function index as an integer
            // The type checker assigned a name like __lambda_N
            const char *lname = e->inferred.struct_name;
            size_t lambda_num = 0;
            if (!lname || sscanf(lname, "__lambda_%zu", &lambda_num) != 1) {
                bp_fatal("lambda missing function name from type checker");
            }
            size_t fn_idx = fe->module->fn_len - typecheck_lambda_count() + lambda_num;

            // Compile the lambda body as a function in the module
            RegFnEmit lfe;
            memset(&lfe, 0, sizeof(lfe));
            lfe.pool = fe->pool;
            lfe.module = fe->module;
            reg_alloc_init(&lfe.ra, e->as.lambda.paramc);
            for (size_t p = 0; p < e->as.lambda.paramc; p++) {
                reg_alloc_param(&lfe.ra, e->as.lambda.params[p].name, (uint8_t)p);
            }
            uint8_t body_result = reg_emit_expr(&lfe, e->as.lambda.body);
            buf_u8(&lfe.code, R_RET);
            buf_u8(&lfe.code, body_result);

            BpFunc *lfn = &fe->module->funcs[fn_idx];
            if (lfn->name) free(lfn->name);
            lfn->name = bp_xstrdup(lname);
            lfn->arity = (uint16_t)e->as.lambda.paramc;
            lfn->locals = 0;
            lfn->code = lfe.code.data;
            lfn->code_len = lfe.code.len;
            lfn->str_const_ids = lfe.str_ids;
            lfn->str_const_len = lfe.str_len;
            lfn->format = BC_FORMAT_REGISTER;
            lfn->reg_count = reg_max_used(&lfe.ra) + 1;
            reg_alloc_free(&lfe.ra);

            // Push function index as integer
            uint8_t dst = reg_alloc_temp(&fe->ra);
            buf_u8(&fe->code, R_CONST_I64);
            buf_u8(&fe->code, dst);
            buf_i64(&fe->code, (int64_t)fn_idx);
            return dst;
        }
        case EX_FSTRING: {
            // F-string interpolation: emit parts, convert to str, concatenate
            if (e->as.fstring.partc == 0) {
                uint32_t sid = strpool_add(fe->pool, "");
                uint32_t local_id = fn_str_index(fe, sid);
                uint8_t dst = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_CONST_STR);
                buf_u8(&fe->code, dst);
                buf_u32(&fe->code, local_id);
                return dst;
            }
            // Emit first part (convert to string if needed)
            uint8_t result = reg_emit_expr(fe, e->as.fstring.parts[0]);
            if (e->as.fstring.parts[0]->inferred.kind != TY_STR) {
                uint8_t str_reg = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_CALL_BUILTIN);
                buf_u8(&fe->code, str_reg);
                buf_u16(&fe->code, (uint16_t)BI_TO_STR);
                buf_u8(&fe->code, result);
                buf_u8(&fe->code, 1);
                reg_free_temp(&fe->ra, result);
                result = str_reg;
            }
            // Emit remaining parts and chain R_ADD_STR
            for (size_t i = 1; i < e->as.fstring.partc; i++) {
                uint8_t part = reg_emit_expr(fe, e->as.fstring.parts[i]);
                if (e->as.fstring.parts[i]->inferred.kind != TY_STR) {
                    uint8_t str_reg = reg_alloc_temp(&fe->ra);
                    buf_u8(&fe->code, R_CALL_BUILTIN);
                    buf_u8(&fe->code, str_reg);
                    buf_u16(&fe->code, (uint16_t)BI_TO_STR);
                    buf_u8(&fe->code, part);
                    buf_u8(&fe->code, 1);
                    reg_free_temp(&fe->ra, part);
                    part = str_reg;
                }
                uint8_t cat = reg_alloc_temp(&fe->ra);
                buf_u8(&fe->code, R_ADD_STR);
                buf_u8(&fe->code, cat);
                buf_u8(&fe->code, result);
                buf_u8(&fe->code, part);
                reg_free_temp(&fe->ra, result);
                reg_free_temp(&fe->ra, part);
                result = cat;
            }
            return result;
        }
        default:
            break;
    }
    bp_fatal("unknown expr in register compiler");
    return 0;
}

// ============================================================================
// Module Compilation
// ============================================================================

BpModule reg_compile_module(const Module *m) {
    BpModule out;
    memset(&out, 0, sizeof(out));

    StrPool pool = {0};

    // Count total functions: module functions + class methods + lambdas
    size_t total_methods = 0;
    for (size_t i = 0; i < m->classc; i++)
        total_methods += m->classes[i].method_count;
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

        RegFnEmit fe;
        memset(&fe, 0, sizeof(fe));
        fe.pool = &pool;
        fe.module = &out;

        // Initialize register allocator with parameter count
        reg_alloc_init(&fe.ra, f->paramc);

        // Parameters are pre-allocated in registers r0..r(paramc-1)
        for (size_t p = 0; p < f->paramc; p++) {
            reg_alloc_param(&fe.ra, f->params[p].name, (uint8_t)p);
        }

        // If this is the entry function, emit global variable initializers
        if (i == out.entry) {
            for (size_t g = 0; g < m->global_varc; g++) {
                if (m->global_vars[g]->kind == ST_LET && m->global_vars[g]->as.let.init) {
                    uint8_t val = reg_emit_expr(&fe, m->global_vars[g]->as.let.init);
                    buf_u8(&fe.code, R_STORE_GLOBAL);
                    buf_u8(&fe.code, val);
                    buf_u16(&fe.code, (uint16_t)g);
                    reg_free_temp(&fe.ra, val);
                }
            }
        }

        // Compile function body
        for (size_t s = 0; s < f->body_len; s++) {
            reg_emit_stmt(&fe, f->body[s]);
        }

        // Implicit return if missing
        uint8_t tmp = reg_alloc_temp(&fe.ra);
        buf_u8(&fe.code, R_CONST_I64);
        buf_u8(&fe.code, tmp);
        buf_i64(&fe.code, 0);
        buf_u8(&fe.code, R_RET);
        buf_u8(&fe.code, tmp);

        out.funcs[i].name = bp_xstrdup(f->name);
        out.funcs[i].arity = (uint16_t)f->paramc;
        out.funcs[i].locals = 0;  // Not used for register VM
        out.funcs[i].code = fe.code.data;
        out.funcs[i].code_len = fe.code.len;
        out.funcs[i].str_const_ids = fe.str_ids;
        out.funcs[i].str_const_len = fe.str_len;
        out.funcs[i].format = BC_FORMAT_REGISTER;
        out.funcs[i].reg_count = reg_max_used(&fe.ra) + 1;

        reg_alloc_free(&fe.ra);
        free(fe.loops);
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
                    RegFnEmit fe;
                    memset(&fe, 0, sizeof(fe));
                    fe.pool = &pool;
                    fe.module = &out;
                    reg_alloc_init(&fe.ra, md->paramc);
                    for (size_t p = 0; p < md->paramc; p++)
                        reg_alloc_param(&fe.ra, md->params[p].name, (uint8_t)p);
                    for (size_t s = 0; s < md->body_len; s++)
                        reg_emit_stmt(&fe, md->body[s]);
                    uint8_t tmp = reg_alloc_temp(&fe.ra);
                    buf_u8(&fe.code, R_CONST_I64);
                    buf_u8(&fe.code, tmp);
                    buf_i64(&fe.code, 0);
                    buf_u8(&fe.code, R_RET);
                    buf_u8(&fe.code, tmp);

                    char mname[256];
                    snprintf(mname, sizeof(mname), "%s$%s", cd->name, md->name);
                    out.funcs[method_fn_idx].name = bp_xstrdup(mname);
                    out.funcs[method_fn_idx].arity = (uint16_t)md->paramc;
                    out.funcs[method_fn_idx].locals = 0;
                    out.funcs[method_fn_idx].code = fe.code.data;
                    out.funcs[method_fn_idx].code_len = fe.code.len;
                    out.funcs[method_fn_idx].str_const_ids = fe.str_ids;
                    out.funcs[method_fn_idx].str_const_len = fe.str_len;
                    out.funcs[method_fn_idx].format = BC_FORMAT_REGISTER;
                    out.funcs[method_fn_idx].reg_count = reg_max_used(&fe.ra) + 1;
                    reg_alloc_free(&fe.ra);
                    method_fn_idx++;
                }
            }
        }
    }

    // Compile extern function declarations (same as stack compiler)
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
            out.extern_funcs[i].handle = NULL;
            out.extern_funcs[i].fn_ptr = NULL;
            // Map parameter types to FFI type codes
            if (ed->paramc > 0) {
                out.extern_funcs[i].param_types = bp_xmalloc(ed->paramc * sizeof(uint8_t));
                for (size_t p = 0; p < ed->paramc; p++) {
                    switch (ed->params[p].type.kind) {
                        case TY_FLOAT: out.extern_funcs[i].param_types[p] = FFI_TC_FLOAT; break;
                        case TY_STR:   out.extern_funcs[i].param_types[p] = FFI_TC_STR; break;
                        case TY_PTR:   out.extern_funcs[i].param_types[p] = FFI_TC_PTR; break;
                        default:       out.extern_funcs[i].param_types[p] = FFI_TC_INT; break;
                    }
                }
            }
            // Map return type
            switch (ed->ret_type.kind) {
                case TY_FLOAT: out.extern_funcs[i].ret_type = FFI_TC_FLOAT; break;
                case TY_STR:   out.extern_funcs[i].ret_type = FFI_TC_STR; break;
                case TY_PTR:   out.extern_funcs[i].ret_type = FFI_TC_PTR; break;
                case TY_VOID:  out.extern_funcs[i].ret_type = FFI_TC_VOID; break;
                default:       out.extern_funcs[i].ret_type = FFI_TC_INT; break;
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
