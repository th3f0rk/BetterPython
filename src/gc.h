#pragma once
#include "common.h"
#include <stddef.h>

struct BpStr {
    uint8_t marked;
    struct BpStr *next;

    size_t len;
    char *data;
};

typedef struct {
    BpStr *head;
    size_t bytes;
    size_t next_gc;
} Gc;

void gc_init(Gc *gc);
void gc_free_all(Gc *gc);

BpStr *gc_new_str(Gc *gc, const char *s, size_t len);

void gc_collect(Gc *gc, Value *stack, size_t sp, Value *locals, size_t localc);
