#pragma once
#include "common.h"
#include <stddef.h>

struct BpStr {
    uint8_t marked;
    struct BpStr *next;

    size_t len;
    char *data;
};

struct BpArray {
    uint8_t marked;
    struct BpArray *next;

    Value *data;
    size_t len;
    size_t cap;
};

typedef struct {
    Value key;
    Value value;
    uint8_t used;      // 0 = empty, 1 = occupied, 2 = tombstone
} MapEntry;

struct BpMap {
    uint8_t marked;
    struct BpMap *next;

    MapEntry *entries;
    size_t cap;
    size_t count;      // Number of used entries (not tombstones)
};

struct BpStruct {
    uint8_t marked;
    struct BpStruct *next;

    uint16_t type_id;       // Struct type identifier
    Value *fields;          // Array of field values
    size_t field_count;
};

struct BpClass {
    uint8_t marked;
    struct BpClass *next;

    uint16_t class_id;      // Class type identifier
    Value *fields;          // Array of field values
    size_t field_count;
    struct BpClass *parent; // Parent class instance (for inheritance)
};

typedef struct {
    BpStr *head;
    BpArray *arr_head;
    BpMap *map_head;
    BpStruct *struct_head;
    BpClass *class_head;
    size_t bytes;
    size_t next_gc;
} Gc;

void gc_init(Gc *gc);
void gc_free_all(Gc *gc);

BpStr *gc_new_str(Gc *gc, const char *s, size_t len);
BpArray *gc_new_array(Gc *gc, size_t initial_cap);
void gc_array_push(Gc *gc, BpArray *arr, Value v);
Value gc_array_get(BpArray *arr, size_t idx);
void gc_array_set(BpArray *arr, size_t idx, Value v);

BpMap *gc_new_map(Gc *gc, size_t initial_cap);
void gc_map_set(Gc *gc, BpMap *map, Value key, Value value);
Value gc_map_get(BpMap *map, Value key, bool *found);
bool gc_map_has_key(BpMap *map, Value key);
bool gc_map_delete(BpMap *map, Value key);
BpArray *gc_map_keys(Gc *gc, BpMap *map);
BpArray *gc_map_values(Gc *gc, BpMap *map);

BpStruct *gc_new_struct(Gc *gc, uint16_t type_id, size_t field_count);
Value gc_struct_get(BpStruct *st, size_t field_idx);
void gc_struct_set(BpStruct *st, size_t field_idx, Value v);

BpClass *gc_new_class(Gc *gc, uint16_t class_id, size_t field_count);
Value gc_class_get(BpClass *cls, size_t field_idx);
void gc_class_set(BpClass *cls, size_t field_idx, Value v);

void gc_collect(Gc *gc, Value *stack, size_t sp, Value *locals, size_t localc);
