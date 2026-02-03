#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define BP_UNUSED(x) (void)(x)

typedef struct BpStr BpStr;

typedef enum {
    VAL_INT,
    VAL_BOOL,
    VAL_NULL,
    VAL_STR
} ValueType;

typedef struct {
    ValueType type;
    union {
        int64_t i;
        bool b;
        BpStr *s;
    } as;
} Value;

static inline Value v_int(int64_t x) { Value v; v.type = VAL_INT; v.as.i = x; return v; }
static inline Value v_bool(bool x)   { Value v; v.type = VAL_BOOL; v.as.b = x; return v; }
static inline Value v_null(void)     { Value v; v.type = VAL_NULL; return v; }
static inline Value v_str(BpStr *s)  { Value v; v.type = VAL_STR; v.as.s = s; return v; }

static inline bool v_is_truthy(Value v) {
    if (v.type == VAL_BOOL) return v.as.b;
    if (v.type == VAL_NULL) return false;
    if (v.type == VAL_INT) return v.as.i != 0;
    return true;
}
