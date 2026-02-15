#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define BP_UNUSED(x) (void)(x)

typedef struct BpStr BpStr;
typedef struct BpArray BpArray;
typedef struct BpMap BpMap;
typedef struct BpStruct BpStruct;
typedef struct BpClass BpClass;

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_NULL,
    VAL_STR,
    VAL_ARRAY,
    VAL_MAP,
    VAL_STRUCT,
    VAL_CLASS,
    VAL_PTR,     // Opaque pointer (for threads, mutexes, etc.)
    VAL_FUNC     // Function reference (index into function table)
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t i;
        double f;
        bool b;
        BpStr *s;
        BpArray *arr;
        BpMap *map;
        BpStruct *st;
        BpClass *cls;
        void *ptr;       // Opaque pointer for threading primitives
    } as;
} Value;

static inline Value v_int(int64_t x)  { Value v; v.type = VAL_INT; v.as.i = x; return v; }
static inline Value v_float(double x) { Value v; v.type = VAL_FLOAT; v.as.f = x; return v; }
static inline Value v_bool(bool x)    { Value v; v.type = VAL_BOOL; v.as.b = x; return v; }
static inline Value v_null(void)      { Value v; v.type = VAL_NULL; return v; }
static inline Value v_str(BpStr *s)   { Value v; v.type = VAL_STR; v.as.s = s; return v; }
static inline Value v_array(BpArray *a) { Value v; v.type = VAL_ARRAY; v.as.arr = a; return v; }
static inline Value v_map(BpMap *m)   { Value v; v.type = VAL_MAP; v.as.map = m; return v; }
static inline Value v_struct(BpStruct *st) { Value v; v.type = VAL_STRUCT; v.as.st = st; return v; }
static inline Value v_class(BpClass *c) { Value v; v.type = VAL_CLASS; v.as.cls = c; return v; }
static inline Value v_ptr(void *p)    { Value v; v.type = VAL_PTR; v.as.ptr = p; return v; }
static inline Value v_func(int64_t idx) { Value v; v.type = VAL_FUNC; v.as.i = idx; return v; }

static inline bool v_is_truthy(Value v) {
    if (v.type == VAL_BOOL) return v.as.b;
    if (v.type == VAL_NULL) return false;
    if (v.type == VAL_INT) return v.as.i != 0;
    if (v.type == VAL_FLOAT) return v.as.f != 0.0;
    return true;
}
