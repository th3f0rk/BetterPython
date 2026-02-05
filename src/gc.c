#include "gc.h"
#include "util.h"
#include <string.h>

void gc_init(Gc *gc) {
    gc->head = NULL;
    gc->arr_head = NULL;
    gc->map_head = NULL;
    gc->struct_head = NULL;
    gc->class_head = NULL;
    gc->bytes = 0;
    gc->next_gc = 1024 * 1024;
}

static void str_free(BpStr *s) {
    free(s->data);
    free(s);
}

static void arr_free(BpArray *a) {
    free(a->data);
    free(a);
}

static void map_free(BpMap *m) {
    free(m->entries);
    free(m);
}

static void struct_free(BpStruct *st) {
    free(st->fields);
    free(st);
}

static void class_free(BpClass *cls) {
    free(cls->fields);
    free(cls);
}

void gc_free_all(Gc *gc) {
    BpStr *it = gc->head;
    while (it) {
        BpStr *n = it->next;
        str_free(it);
        it = n;
    }
    gc->head = NULL;

    BpArray *ait = gc->arr_head;
    while (ait) {
        BpArray *n = ait->next;
        arr_free(ait);
        ait = n;
    }
    gc->arr_head = NULL;

    BpMap *mit = gc->map_head;
    while (mit) {
        BpMap *n = mit->next;
        map_free(mit);
        mit = n;
    }
    gc->map_head = NULL;

    BpStruct *sit = gc->struct_head;
    while (sit) {
        BpStruct *n = sit->next;
        struct_free(sit);
        sit = n;
    }
    gc->struct_head = NULL;

    BpClass *cit = gc->class_head;
    while (cit) {
        BpClass *n = cit->next;
        class_free(cit);
        cit = n;
    }
    gc->class_head = NULL;

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

BpArray *gc_new_array(Gc *gc, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 8;
    BpArray *a = bp_xmalloc(sizeof(*a));
    a->marked = 0;
    a->len = 0;
    a->cap = initial_cap;
    a->data = bp_xmalloc(initial_cap * sizeof(Value));
    for (size_t i = 0; i < initial_cap; i++) a->data[i] = v_null();

    a->next = gc->arr_head;
    gc->arr_head = a;

    gc->bytes += sizeof(*a) + initial_cap * sizeof(Value);
    return a;
}

void gc_array_push(Gc *gc, BpArray *arr, Value v) {
    if (arr->len + 1 > arr->cap) {
        size_t old_cap = arr->cap;
        arr->cap = arr->cap ? arr->cap * 2 : 8;
        arr->data = bp_xrealloc(arr->data, arr->cap * sizeof(Value));
        for (size_t i = old_cap; i < arr->cap; i++) arr->data[i] = v_null();
        gc->bytes += (arr->cap - old_cap) * sizeof(Value);
    }
    arr->data[arr->len++] = v;
}

Value gc_array_get(BpArray *arr, size_t idx) {
    if (idx >= arr->len) bp_fatal("array index out of bounds: %zu >= %zu", idx, arr->len);
    return arr->data[idx];
}

void gc_array_set(BpArray *arr, size_t idx, Value v) {
    if (idx >= arr->len) bp_fatal("array index out of bounds: %zu >= %zu", idx, arr->len);
    arr->data[idx] = v;
}

// Hash function for map keys
static uint64_t hash_value(Value v) {
    switch (v.type) {
        case VAL_INT:
            return (uint64_t)v.as.i * 2654435761ULL;
        case VAL_FLOAT: {
            uint64_t bits;
            memcpy(&bits, &v.as.f, sizeof(bits));
            return bits * 2654435761ULL;
        }
        case VAL_BOOL:
            return v.as.b ? 1231 : 1237;
        case VAL_STR:
            if (!v.as.s) return 0;
            {
                uint64_t h = 14695981039346656037ULL;  // FNV-1a offset basis
                for (size_t i = 0; i < v.as.s->len; i++) {
                    h ^= (uint8_t)v.as.s->data[i];
                    h *= 1099511628211ULL;  // FNV prime
                }
                return h;
            }
        case VAL_NULL:
            return 0;
        default:
            return 0;  // Arrays and maps are not hashable as keys
    }
}

// Value equality check for map keys
static bool value_eq(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_INT: return a.as.i == b.as.i;
        case VAL_FLOAT: return a.as.f == b.as.f;
        case VAL_BOOL: return a.as.b == b.as.b;
        case VAL_NULL: return true;
        case VAL_STR:
            if (a.as.s == b.as.s) return true;
            if (!a.as.s || !b.as.s) return false;
            if (a.as.s->len != b.as.s->len) return false;
            return memcmp(a.as.s->data, b.as.s->data, a.as.s->len) == 0;
        default:
            return a.as.arr == b.as.arr;  // Reference equality for arrays/maps
    }
}

BpMap *gc_new_map(Gc *gc, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 16;
    BpMap *m = bp_xmalloc(sizeof(*m));
    m->marked = 0;
    m->count = 0;
    m->cap = initial_cap;
    m->entries = bp_xmalloc(initial_cap * sizeof(MapEntry));
    for (size_t i = 0; i < initial_cap; i++) {
        m->entries[i].used = 0;
    }

    m->next = gc->map_head;
    gc->map_head = m;

    gc->bytes += sizeof(*m) + initial_cap * sizeof(MapEntry);
    return m;
}

// Find entry slot for key (for get/set operations)
static MapEntry *find_entry(MapEntry *entries, size_t cap, Value key) {
    uint64_t hash = hash_value(key);
    size_t idx = hash % cap;
    MapEntry *tombstone = NULL;

    for (;;) {
        MapEntry *entry = &entries[idx];
        if (entry->used == 0) {
            // Empty entry - return tombstone if we found one, otherwise this slot
            return tombstone ? tombstone : entry;
        } else if (entry->used == 2) {
            // Tombstone - remember it but keep looking
            if (!tombstone) tombstone = entry;
        } else if (value_eq(entry->key, key)) {
            // Found the key
            return entry;
        }
        idx = (idx + 1) % cap;
    }
}

// Grow and rehash the map
static void map_grow(Gc *gc, BpMap *map) {
    size_t old_cap = map->cap;
    MapEntry *old_entries = map->entries;

    size_t new_cap = old_cap * 2;
    MapEntry *new_entries = bp_xmalloc(new_cap * sizeof(MapEntry));
    for (size_t i = 0; i < new_cap; i++) {
        new_entries[i].used = 0;
    }

    // Rehash all existing entries
    map->count = 0;
    for (size_t i = 0; i < old_cap; i++) {
        if (old_entries[i].used == 1) {
            MapEntry *dest = find_entry(new_entries, new_cap, old_entries[i].key);
            dest->key = old_entries[i].key;
            dest->value = old_entries[i].value;
            dest->used = 1;
            map->count++;
        }
    }

    map->entries = new_entries;
    map->cap = new_cap;
    gc->bytes += (new_cap - old_cap) * sizeof(MapEntry);
    free(old_entries);
}

void gc_map_set(Gc *gc, BpMap *map, Value key, Value value) {
    // Grow if load factor > 0.75
    if (map->count + 1 > map->cap * 3 / 4) {
        map_grow(gc, map);
    }

    MapEntry *entry = find_entry(map->entries, map->cap, key);
    bool is_new = (entry->used != 1);
    if (is_new) map->count++;

    entry->key = key;
    entry->value = value;
    entry->used = 1;
}

Value gc_map_get(BpMap *map, Value key, bool *found) {
    if (map->count == 0) {
        *found = false;
        return v_null();
    }

    MapEntry *entry = find_entry(map->entries, map->cap, key);
    if (entry->used != 1) {
        *found = false;
        return v_null();
    }

    *found = true;
    return entry->value;
}

bool gc_map_has_key(BpMap *map, Value key) {
    if (map->count == 0) return false;
    MapEntry *entry = find_entry(map->entries, map->cap, key);
    return entry->used == 1;
}

bool gc_map_delete(BpMap *map, Value key) {
    if (map->count == 0) return false;
    MapEntry *entry = find_entry(map->entries, map->cap, key);
    if (entry->used != 1) return false;

    // Mark as tombstone
    entry->used = 2;
    map->count--;
    return true;
}

BpArray *gc_map_keys(Gc *gc, BpMap *map) {
    BpArray *arr = gc_new_array(gc, map->count > 0 ? map->count : 1);
    for (size_t i = 0; i < map->cap; i++) {
        if (map->entries[i].used == 1) {
            gc_array_push(gc, arr, map->entries[i].key);
        }
    }
    return arr;
}

BpArray *gc_map_values(Gc *gc, BpMap *map) {
    BpArray *arr = gc_new_array(gc, map->count > 0 ? map->count : 1);
    for (size_t i = 0; i < map->cap; i++) {
        if (map->entries[i].used == 1) {
            gc_array_push(gc, arr, map->entries[i].value);
        }
    }
    return arr;
}

BpStruct *gc_new_struct(Gc *gc, uint16_t type_id, size_t field_count) {
    BpStruct *st = bp_xmalloc(sizeof(*st));
    st->marked = 0;
    st->type_id = type_id;
    st->field_count = field_count;
    st->fields = bp_xmalloc(field_count * sizeof(Value));
    for (size_t i = 0; i < field_count; i++) st->fields[i] = v_null();

    st->next = gc->struct_head;
    gc->struct_head = st;

    gc->bytes += sizeof(*st) + field_count * sizeof(Value);
    return st;
}

Value gc_struct_get(BpStruct *st, size_t field_idx) {
    if (field_idx >= st->field_count) bp_fatal("struct field index out of bounds: %zu >= %zu", field_idx, st->field_count);
    return st->fields[field_idx];
}

void gc_struct_set(BpStruct *st, size_t field_idx, Value v) {
    if (field_idx >= st->field_count) bp_fatal("struct field index out of bounds: %zu >= %zu", field_idx, st->field_count);
    st->fields[field_idx] = v;
}

BpClass *gc_new_class(Gc *gc, uint16_t class_id, size_t field_count) {
    BpClass *cls = bp_xmalloc(sizeof(*cls));
    cls->marked = 0;
    cls->class_id = class_id;
    cls->field_count = field_count;
    cls->parent = NULL;
    cls->fields = bp_xmalloc(field_count * sizeof(Value));
    for (size_t i = 0; i < field_count; i++) cls->fields[i] = v_null();

    cls->next = gc->class_head;
    gc->class_head = cls;

    gc->bytes += sizeof(*cls) + field_count * sizeof(Value);
    return cls;
}

Value gc_class_get(BpClass *cls, size_t field_idx) {
    if (field_idx >= cls->field_count) bp_fatal("class field index out of bounds: %zu >= %zu", field_idx, cls->field_count);
    return cls->fields[field_idx];
}

void gc_class_set(BpClass *cls, size_t field_idx, Value v) {
    if (field_idx >= cls->field_count) bp_fatal("class field index out of bounds: %zu >= %zu", field_idx, cls->field_count);
    cls->fields[field_idx] = v;
}

static void mark_value(Value v);

static void mark_array(BpArray *arr) {
    if (!arr || arr->marked) return;
    arr->marked = 1;
    for (size_t i = 0; i < arr->len; i++) {
        mark_value(arr->data[i]);
    }
}

static void mark_map(BpMap *map) {
    if (!map || map->marked) return;
    map->marked = 1;
    for (size_t i = 0; i < map->cap; i++) {
        if (map->entries[i].used == 1) {
            mark_value(map->entries[i].key);
            mark_value(map->entries[i].value);
        }
    }
}

static void mark_struct(BpStruct *st) {
    if (!st || st->marked) return;
    st->marked = 1;
    for (size_t i = 0; i < st->field_count; i++) {
        mark_value(st->fields[i]);
    }
}

static void mark_class(BpClass *cls) {
    if (!cls || cls->marked) return;
    cls->marked = 1;
    for (size_t i = 0; i < cls->field_count; i++) {
        mark_value(cls->fields[i]);
    }
    if (cls->parent) mark_class(cls->parent);
}

static void mark_value(Value v) {
    if (v.type == VAL_STR && v.as.s) {
        v.as.s->marked = 1;
    } else if (v.type == VAL_ARRAY && v.as.arr) {
        mark_array(v.as.arr);
    } else if (v.type == VAL_MAP && v.as.map) {
        mark_map(v.as.map);
    } else if (v.type == VAL_STRUCT && v.as.st) {
        mark_struct(v.as.st);
    } else if (v.type == VAL_CLASS && v.as.cls) {
        mark_class(v.as.cls);
    }
}

static void mark_roots(Value *stack, size_t sp, Value *locals, size_t localc) {
    for (size_t i = 0; i < sp; i++) mark_value(stack[i]);
    for (size_t i = 0; i < localc; i++) mark_value(locals[i]);
}

static void sweep(Gc *gc) {
    // Sweep strings
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

    // Sweep arrays
    BpArray **ait = &gc->arr_head;
    while (*ait) {
        BpArray *obj = *ait;
        if (obj->marked) {
            obj->marked = 0;
            ait = &obj->next;
            continue;
        }
        *ait = obj->next;
        gc->bytes -= sizeof(*obj) + obj->cap * sizeof(Value);
        arr_free(obj);
    }

    // Sweep maps
    BpMap **mit = &gc->map_head;
    while (*mit) {
        BpMap *obj = *mit;
        if (obj->marked) {
            obj->marked = 0;
            mit = &obj->next;
            continue;
        }
        *mit = obj->next;
        gc->bytes -= sizeof(*obj) + obj->cap * sizeof(MapEntry);
        map_free(obj);
    }

    // Sweep structs
    BpStruct **sit = &gc->struct_head;
    while (*sit) {
        BpStruct *obj = *sit;
        if (obj->marked) {
            obj->marked = 0;
            sit = &obj->next;
            continue;
        }
        *sit = obj->next;
        gc->bytes -= sizeof(*obj) + obj->field_count * sizeof(Value);
        struct_free(obj);
    }

    // Sweep classes
    BpClass **cit = &gc->class_head;
    while (*cit) {
        BpClass *obj = *cit;
        if (obj->marked) {
            obj->marked = 0;
            cit = &obj->next;
            continue;
        }
        *cit = obj->next;
        gc->bytes -= sizeof(*obj) + obj->field_count * sizeof(Value);
        class_free(obj);
    }

    if (gc->bytes < 1024 * 1024) gc->next_gc = 1024 * 1024;
    else gc->next_gc = gc->bytes * 2;
}

void gc_collect(Gc *gc, Value *stack, size_t sp, Value *locals, size_t localc) {
    mark_roots(stack, sp, locals, localc);
    sweep(gc);
}
