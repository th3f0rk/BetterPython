/*
 * BetterPython Runtime Library
 * Provides runtime support for C-transpiled BetterPython programs.
 * Compile: cc -O2 -std=gnu11 -iquote <src_dir> program.c bp_runtime.c -lm -lpthread -o program
 * NOTE: Use -iquote (not -I) to avoid shadowing system <stdlib.h> with src/stdlib.h
 */
#pragma once

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <setjmp.h>
#include <pthread.h>
#include <regex.h>
#include <stdarg.h>

/* ───────────────────────────────────────────────────── */
/*  Forward declarations                                 */
/* ───────────────────────────────────────────────────── */
typedef struct BpRtStr    BpRtStr;
typedef struct BpRtArray  BpRtArray;
typedef struct BpRtMap    BpRtMap;

/* ───────────────────────────────────────────────────── */
/*  Value type – tagged union identical to VM's Value    */
/* ───────────────────────────────────────────────────── */
typedef enum {
    RT_INT, RT_FLOAT, RT_BOOL, RT_NULL,
    RT_STR, RT_ARRAY, RT_MAP, RT_PTR
} RtValType;

typedef struct {
    RtValType type;
    union {
        int64_t  i;
        double   f;
        bool     b;
        BpRtStr   *s;
        BpRtArray *arr;
        BpRtMap   *map;
        void      *ptr;
    } as;
} RtVal;

/* ───────────────────────────────────────────────────── */
/*  String type (ref-counted)                            */
/* ───────────────────────────────────────────────────── */
struct BpRtStr {
    size_t len;
    size_t refcnt;
    char  *data;
};

/* ───────────────────────────────────────────────────── */
/*  Dynamic array                                        */
/* ───────────────────────────────────────────────────── */
struct BpRtArray {
    RtVal  *data;
    size_t  len;
    size_t  cap;
    size_t  refcnt;
};

/* ───────────────────────────────────────────────────── */
/*  Hash map (open addressing)                           */
/* ───────────────────────────────────────────────────── */
typedef struct {
    RtVal   key;
    RtVal   value;
    uint8_t used;       /* 0=empty, 1=occupied, 2=tombstone */
} RtMapEntry;

struct BpRtMap {
    RtMapEntry *entries;
    size_t      cap;
    size_t      count;
    size_t      refcnt;
};

/* ───────────────────────────────────────────────────── */
/*  Exception handling (setjmp / longjmp)                */
/* ───────────────────────────────────────────────────── */
#define BP_EXC_STACK_MAX 64

typedef struct {
    jmp_buf  buf;
    bool     active;
} BpExcFrame;

typedef struct {
    BpExcFrame frames[BP_EXC_STACK_MAX];
    int        depth;
    RtVal      current_exception;
    bool       has_exception;
} BpExcState;

extern BpExcState bp_exc;

/* ───────────────────────────────────────────────────── */
/*  Runtime init / cleanup                               */
/* ───────────────────────────────────────────────────── */
void bp_runtime_init(int argc, char **argv);
void bp_runtime_cleanup(void);

/* ───────────────────────────────────────────────────── */
/*  Value constructors                                   */
/* ───────────────────────────────────────────────────── */
static inline RtVal rv_int(int64_t x)    { RtVal v; v.type = RT_INT;   v.as.i = x; return v; }
static inline RtVal rv_float(double x)   { RtVal v; v.type = RT_FLOAT; v.as.f = x; return v; }
static inline RtVal rv_bool(bool x)      { RtVal v; v.type = RT_BOOL;  v.as.b = x; return v; }
static inline RtVal rv_null(void)        { RtVal v; v.type = RT_NULL;  return v; }
static inline RtVal rv_str(BpRtStr *s)   { RtVal v; v.type = RT_STR;   v.as.s = s; return v; }
static inline RtVal rv_arr(BpRtArray *a) { RtVal v; v.type = RT_ARRAY; v.as.arr = a; return v; }
static inline RtVal rv_map(BpRtMap *m)   { RtVal v; v.type = RT_MAP;   v.as.map = m; return v; }
static inline RtVal rv_ptr(void *p)      { RtVal v; v.type = RT_PTR;   v.as.ptr = p; return v; }

/* ───────────────────────────────────────────────────── */
/*  Strings                                              */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_str_new(const char *data, size_t len);
BpRtStr *bp_str_from_cstr(const char *s);
BpRtStr *bp_str_concat(BpRtStr *a, BpRtStr *b);
void     bp_str_free(BpRtStr *s);

/* ───────────────────────────────────────────────────── */
/*  Arrays                                               */
/* ───────────────────────────────────────────────────── */
BpRtArray *bp_array_new(size_t initial_cap);
void       bp_array_push(BpRtArray *arr, RtVal v);
RtVal      bp_array_pop(BpRtArray *arr);
RtVal      bp_array_get(BpRtArray *arr, int64_t idx);
void       bp_array_set(BpRtArray *arr, int64_t idx, RtVal v);
void       bp_array_free(BpRtArray *arr);

/* ───────────────────────────────────────────────────── */
/*  Maps                                                 */
/* ───────────────────────────────────────────────────── */
BpRtMap *bp_map_new(size_t initial_cap);
void     bp_map_set(BpRtMap *map, RtVal key, RtVal value);
RtVal    bp_map_get(BpRtMap *map, RtVal key);
bool     bp_map_has_key(BpRtMap *map, RtVal key);
bool     bp_map_delete(BpRtMap *map, RtVal key);
BpRtArray *bp_map_keys(BpRtMap *map);
BpRtArray *bp_map_values(BpRtMap *map);
void     bp_map_free(BpRtMap *map);

/* ───────────────────────────────────────────────────── */
/*  Exception macros                                     */
/* ───────────────────────────────────────────────────── */
#define BP_TRY \
    do { \
        if (bp_exc.depth >= BP_EXC_STACK_MAX) { fprintf(stderr, "exception stack overflow\n"); exit(1); } \
        bp_exc.frames[bp_exc.depth].active = true; \
        bp_exc.depth++; \
        if (setjmp(bp_exc.frames[bp_exc.depth - 1].buf) == 0) {

#define BP_CATCH(varname) \
        bp_exc.depth--; \
        bp_exc.frames[bp_exc.depth].active = false; \
        } else { \
            bp_exc.depth--; \
            bp_exc.frames[bp_exc.depth].active = false; \
            RtVal varname = bp_exc.current_exception; \
            bp_exc.has_exception = false;

#define BP_END_TRY \
        } \
    } while (0)

void bp_throw(RtVal value);

/* ───────────────────────────────────────────────────── */
/*  Built-in functions                                   */
/* ───────────────────────────────────────────────────── */

/* I/O */
void      bp_print_val(RtVal v);
void      bp_print_vals(int count, ...);
BpRtStr  *bp_read_line(void);

/* Conversion */
BpRtStr  *bp_to_str(RtVal v);
int64_t   bp_to_int(RtVal v);

/* String operations */
int64_t   bp_len(BpRtStr *s);
BpRtStr  *bp_substr(BpRtStr *s, int64_t start, int64_t length);
BpRtStr  *bp_str_upper(BpRtStr *s);
BpRtStr  *bp_str_lower(BpRtStr *s);
BpRtStr  *bp_str_trim(BpRtStr *s);
bool      bp_starts_with(BpRtStr *s, BpRtStr *prefix);
bool      bp_ends_with(BpRtStr *s, BpRtStr *suffix);
int64_t   bp_str_find(BpRtStr *s, BpRtStr *sub);
BpRtStr  *bp_str_replace(BpRtStr *s, BpRtStr *old_s, BpRtStr *new_s);
BpRtStr  *bp_str_reverse(BpRtStr *s);
BpRtStr  *bp_str_repeat(BpRtStr *s, int64_t count);
BpRtStr  *bp_str_pad_left(BpRtStr *s, int64_t width, BpRtStr *pad);
BpRtStr  *bp_str_pad_right(BpRtStr *s, int64_t width, BpRtStr *pad);
bool      bp_str_contains(BpRtStr *s, BpRtStr *sub);
int64_t   bp_str_count(BpRtStr *s, BpRtStr *sub);
BpRtStr  *bp_str_char_at(BpRtStr *s, int64_t idx);
int64_t   bp_str_index_of(BpRtStr *s, BpRtStr *sub);
BpRtStr  *bp_chr(int64_t code);
int64_t   bp_ord(BpRtStr *s);
BpRtStr  *bp_int_to_hex(int64_t x);
int64_t   bp_hex_to_int(BpRtStr *s);
BpRtArray *bp_str_split(BpRtStr *s, BpRtStr *delim);
BpRtStr  *bp_str_join(BpRtStr *delim, BpRtArray *arr);
BpRtArray *bp_str_split_str(BpRtStr *s, BpRtStr *delim);
BpRtStr  *bp_str_join_arr(BpRtStr *delim, BpRtArray *arr);
BpRtStr  *bp_str_concat_all(BpRtArray *arr);

/* Validation */
bool      bp_is_digit(BpRtStr *s);
bool      bp_is_alpha(BpRtStr *s);
bool      bp_is_alnum(BpRtStr *s);
bool      bp_is_space(BpRtStr *s);

/* Math (int) */
int64_t   bp_abs(int64_t x);
int64_t   bp_min(int64_t a, int64_t b);
int64_t   bp_max(int64_t a, int64_t b);
int64_t   bp_pow_int(int64_t base, int64_t exp);
int64_t   bp_sqrt_int(int64_t x);
int64_t   bp_clamp(int64_t x, int64_t lo, int64_t hi);
int64_t   bp_sign(int64_t x);

/* Math (float) */
double    bp_fabs_f(double x);
double    bp_ffloor(double x);
double    bp_fceil(double x);
double    bp_fround(double x);
double    bp_fsqrt(double x);
double    bp_fpow(double base, double exp);

/* Trig */
double    bp_sin(double x);
double    bp_cos(double x);
double    bp_tan(double x);
double    bp_asin_f(double x);
double    bp_acos_f(double x);
double    bp_atan_f(double x);
double    bp_atan2_f(double y, double x);

/* Logarithms */
double    bp_log_f(double x);
double    bp_log10_f(double x);
double    bp_log2_f(double x);
double    bp_exp_f(double x);

/* Float conversion */
double    bp_int_to_float(int64_t x);
int64_t   bp_float_to_int(double x);
BpRtStr  *bp_float_to_str(double x);
double    bp_str_to_float(BpRtStr *s);
bool      bp_is_nan(double x);
bool      bp_is_inf(double x);

/* File I/O */
BpRtStr  *bp_file_read(BpRtStr *path);
void      bp_file_write(BpRtStr *path, BpRtStr *content);
void      bp_file_append(BpRtStr *path, BpRtStr *content);
bool      bp_file_exists(BpRtStr *path);
void      bp_file_delete(BpRtStr *path);
int64_t   bp_file_size(BpRtStr *path);
void      bp_file_copy(BpRtStr *src, BpRtStr *dst);

/* Encoding */
BpRtStr  *bp_base64_encode(BpRtStr *s);
BpRtStr  *bp_base64_decode(BpRtStr *s);

/* Random */
int64_t   bp_rand(void);
int64_t   bp_rand_range(int64_t lo, int64_t hi);
void      bp_rand_seed(int64_t seed);

/* System */
int64_t   bp_clock_ms(void);
void      bp_sleep_ms(int64_t ms);
BpRtStr  *bp_getenv_str(BpRtStr *name);
void      bp_exit(int64_t code);
BpRtStr  *bp_argv_get(int64_t idx);
int64_t   bp_argc_get(void);

/* Array operations */
int64_t   bp_array_len(BpRtArray *arr);

/* Map operations */
int64_t   bp_map_len(BpRtMap *map);

/* Regex */
bool      bp_regex_match(BpRtStr *pattern, BpRtStr *text);
BpRtStr  *bp_regex_search(BpRtStr *pattern, BpRtStr *text);
BpRtStr  *bp_regex_replace(BpRtStr *pattern, BpRtStr *replacement, BpRtStr *text);
BpRtArray *bp_regex_split(BpRtStr *pattern, BpRtStr *text);
BpRtArray *bp_regex_find_all(BpRtStr *pattern, BpRtStr *text);

/* Security */
BpRtStr  *bp_hash_sha256(BpRtStr *s);
BpRtStr  *bp_hash_md5(BpRtStr *s);
bool      bp_secure_compare(BpRtStr *a, BpRtStr *b);
BpRtStr  *bp_random_bytes(int64_t n);

/* Floor/Ceil/Round (int versions that just pass through) */
int64_t   bp_floor_int(int64_t x);
int64_t   bp_ceil_int(int64_t x);
int64_t   bp_round_int(int64_t x);
