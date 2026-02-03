#include "gc.h"
#include "util.h"
#include <string.h>

void gc_init(Gc *gc) {
    gc->head = NULL;
    gc->bytes = 0;
    gc->next_gc = 1024 * 1024;
}

static void str_free(BpStr *s) {
    free(s->data);
    free(s);
}

void gc_free_all(Gc *gc) {
    BpStr *it = gc->head;
    while (it) {
        BpStr *n = it->next;
        str_free(it);
        it = n;
    }
    gc->head = NULL;
    gc->bytes = 0;
}

BpStr *gc_new_str(Gc *gc, const char *s, size_t len) {
    BpStr *o = bp_xmalloc(sizeof(*o));
    o->marked = 0;
    o->len = len;
    o->data = bp_xmalloc(len + 1);
    memcpy(o->data, s, len);
    o->data[len] = '\0';

    o->next = gc->head;
    gc->head = o;

    gc->bytes += sizeof(*o) + len + 1;
    return o;
}

static void mark_value(Value v) {
    if (v.type != VAL_STR || !v.as.s) return;
    v.as.s->marked = 1;
}

static void mark_roots(Value *stack, size_t sp, Value *locals, size_t localc) {
    for (size_t i = 0; i < sp; i++) mark_value(stack[i]);
    for (size_t i = 0; i < localc; i++) mark_value(locals[i]);
}

static void sweep(Gc *gc) {
    BpStr **it = &gc->head;
    while (*it) {
        BpStr *obj = *it;
        if (obj->marked) {
            obj->marked = 0;
            it = &obj->next;
            continue;
        }
        *it = obj->next;
        gc->bytes -= sizeof(*obj) + obj->len + 1;
        str_free(obj);
    }
    if (gc->bytes < 1024 * 1024) gc->next_gc = 1024 * 1024;
    else gc->next_gc = gc->bytes * 2;
}

void gc_collect(Gc *gc, Value *stack, size_t sp, Value *locals, size_t localc) {
    mark_roots(stack, sp, locals, localc);
    sweep(gc);
}
