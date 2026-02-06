/*
 * BetterPython Runtime Library
 * Provides all built-in functions and types for C-transpiled programs.
 */
#include "bp_runtime.h"

/* ───────────────────────────────────────────────────── */
/*  Globals                                              */
/* ───────────────────────────────────────────────────── */
BpExcState bp_exc;

static int   g_argc = 0;
static char **g_argv = NULL;

/* ───────────────────────────────────────────────────── */
/*  Runtime init / cleanup                               */
/* ───────────────────────────────────────────────────── */
void bp_runtime_init(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
    memset(&bp_exc, 0, sizeof(bp_exc));
    srand((unsigned)time(NULL));
}

void bp_runtime_cleanup(void) {
    /* Future: walk and free all allocations */
}

/* ───────────────────────────────────────────────────── */
/*  Strings                                              */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_str_new(const char *data, size_t len) {
    BpRtStr *s = (BpRtStr *)malloc(sizeof(BpRtStr));
    s->len = len;
    s->refcnt = 1;
    s->data = (char *)malloc(len + 1);
    if (data) memcpy(s->data, data, len);
    s->data[len] = '\0';
    return s;
}

BpRtStr *bp_str_from_cstr(const char *cs) {
    return bp_str_new(cs, strlen(cs));
}

BpRtStr *bp_str_concat(BpRtStr *a, BpRtStr *b) {
    size_t newlen = a->len + b->len;
    BpRtStr *s = bp_str_new(NULL, newlen);
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len);
    s->data[newlen] = '\0';
    return s;
}

void bp_str_free(BpRtStr *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

/* ───────────────────────────────────────────────────── */
/*  Arrays                                               */
/* ───────────────────────────────────────────────────── */
BpRtArray *bp_array_new(size_t initial_cap) {
    if (initial_cap < 4) initial_cap = 4;
    BpRtArray *a = (BpRtArray *)malloc(sizeof(BpRtArray));
    a->data = (RtVal *)malloc(initial_cap * sizeof(RtVal));
    a->len = 0;
    a->cap = initial_cap;
    a->refcnt = 1;
    return a;
}

void bp_array_push(BpRtArray *arr, RtVal v) {
    if (arr->len >= arr->cap) {
        arr->cap = arr->cap * 2;
        arr->data = (RtVal *)realloc(arr->data, arr->cap * sizeof(RtVal));
    }
    arr->data[arr->len++] = v;
}

RtVal bp_array_pop(BpRtArray *arr) {
    if (arr->len == 0) { fprintf(stderr, "array pop on empty array\n"); exit(1); }
    return arr->data[--arr->len];
}

RtVal bp_array_get(BpRtArray *arr, int64_t idx) {
    if (idx < 0 || (size_t)idx >= arr->len) {
        fprintf(stderr, "array index out of bounds: %lld (len %zu)\n", (long long)idx, arr->len);
        exit(1);
    }
    return arr->data[idx];
}

void bp_array_set(BpRtArray *arr, int64_t idx, RtVal v) {
    if (idx < 0 || (size_t)idx >= arr->len) {
        fprintf(stderr, "array index out of bounds: %lld (len %zu)\n", (long long)idx, arr->len);
        exit(1);
    }
    arr->data[idx] = v;
}

void bp_array_free(BpRtArray *arr) {
    if (!arr) return;
    free(arr->data);
    free(arr);
}

/* ───────────────────────────────────────────────────── */
/*  Maps (open addressing with linear probing)           */
/* ───────────────────────────────────────────────────── */
static uint64_t rtval_hash(RtVal v) {
    switch (v.type) {
        case RT_INT:   return (uint64_t)v.as.i * 2654435761ULL;
        case RT_FLOAT: { uint64_t u; memcpy(&u, &v.as.f, 8); return u * 2654435761ULL; }
        case RT_BOOL:  return v.as.b ? 1 : 0;
        case RT_STR: {
            uint64_t h = 5381;
            for (size_t i = 0; i < v.as.s->len; i++) h = h * 33 + (unsigned char)v.as.s->data[i];
            return h;
        }
        default: return 0;
    }
}

static bool rtval_eq(RtVal a, RtVal b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case RT_INT:   return a.as.i == b.as.i;
        case RT_FLOAT: return a.as.f == b.as.f;
        case RT_BOOL:  return a.as.b == b.as.b;
        case RT_NULL:  return true;
        case RT_STR:   return a.as.s->len == b.as.s->len && memcmp(a.as.s->data, b.as.s->data, a.as.s->len) == 0;
        default: return false;
    }
}

BpRtMap *bp_map_new(size_t initial_cap) {
    if (initial_cap < 8) initial_cap = 8;
    BpRtMap *m = (BpRtMap *)malloc(sizeof(BpRtMap));
    m->entries = (RtMapEntry *)calloc(initial_cap, sizeof(RtMapEntry));
    m->cap = initial_cap;
    m->count = 0;
    m->refcnt = 1;
    return m;
}

static void map_resize(BpRtMap *map, size_t new_cap) {
    RtMapEntry *old = map->entries;
    size_t old_cap = map->cap;
    map->entries = (RtMapEntry *)calloc(new_cap, sizeof(RtMapEntry));
    map->cap = new_cap;
    map->count = 0;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].used == 1) {
            bp_map_set(map, old[i].key, old[i].value);
        }
    }
    free(old);
}

void bp_map_set(BpRtMap *map, RtVal key, RtVal value) {
    if (map->count * 100 / map->cap > 70) {
        map_resize(map, map->cap * 2);
    }
    uint64_t h = rtval_hash(key) % map->cap;
    for (size_t i = 0; i < map->cap; i++) {
        size_t idx = (h + i) % map->cap;
        if (map->entries[idx].used == 0 || map->entries[idx].used == 2) {
            map->entries[idx].key = key;
            map->entries[idx].value = value;
            map->entries[idx].used = 1;
            map->count++;
            return;
        }
        if (map->entries[idx].used == 1 && rtval_eq(map->entries[idx].key, key)) {
            map->entries[idx].value = value;
            return;
        }
    }
}

RtVal bp_map_get(BpRtMap *map, RtVal key) {
    uint64_t h = rtval_hash(key) % map->cap;
    for (size_t i = 0; i < map->cap; i++) {
        size_t idx = (h + i) % map->cap;
        if (map->entries[idx].used == 0) break;
        if (map->entries[idx].used == 1 && rtval_eq(map->entries[idx].key, key)) {
            return map->entries[idx].value;
        }
    }
    return rv_null();
}

bool bp_map_has_key(BpRtMap *map, RtVal key) {
    uint64_t h = rtval_hash(key) % map->cap;
    for (size_t i = 0; i < map->cap; i++) {
        size_t idx = (h + i) % map->cap;
        if (map->entries[idx].used == 0) break;
        if (map->entries[idx].used == 1 && rtval_eq(map->entries[idx].key, key)) {
            return true;
        }
    }
    return false;
}

bool bp_map_delete(BpRtMap *map, RtVal key) {
    uint64_t h = rtval_hash(key) % map->cap;
    for (size_t i = 0; i < map->cap; i++) {
        size_t idx = (h + i) % map->cap;
        if (map->entries[idx].used == 0) break;
        if (map->entries[idx].used == 1 && rtval_eq(map->entries[idx].key, key)) {
            map->entries[idx].used = 2;
            map->count--;
            return true;
        }
    }
    return false;
}

BpRtArray *bp_map_keys(BpRtMap *map) {
    BpRtArray *arr = bp_array_new(map->count);
    for (size_t i = 0; i < map->cap; i++) {
        if (map->entries[i].used == 1) bp_array_push(arr, map->entries[i].key);
    }
    return arr;
}

BpRtArray *bp_map_values(BpRtMap *map) {
    BpRtArray *arr = bp_array_new(map->count);
    for (size_t i = 0; i < map->cap; i++) {
        if (map->entries[i].used == 1) bp_array_push(arr, map->entries[i].value);
    }
    return arr;
}

void bp_map_free(BpRtMap *map) {
    if (!map) return;
    free(map->entries);
    free(map);
}

/* ───────────────────────────────────────────────────── */
/*  Exception handling                                   */
/* ───────────────────────────────────────────────────── */
void bp_throw(RtVal value) {
    bp_exc.current_exception = value;
    bp_exc.has_exception = true;
    if (bp_exc.depth > 0) {
        longjmp(bp_exc.frames[bp_exc.depth - 1].buf, 1);
    } else {
        fprintf(stderr, "Unhandled exception: ");
        if (value.type == RT_STR) fprintf(stderr, "%s", value.as.s->data);
        else if (value.type == RT_INT) fprintf(stderr, "%lld", (long long)value.as.i);
        else fprintf(stderr, "<unknown>");
        fprintf(stderr, "\n");
        exit(1);
    }
}

/* ───────────────────────────────────────────────────── */
/*  Printing                                             */
/* ───────────────────────────────────────────────────── */
static void print_rtval(RtVal v) {
    switch (v.type) {
        case RT_INT:   printf("%lld", (long long)v.as.i); break;
        case RT_FLOAT: printf("%g", v.as.f); break;
        case RT_BOOL:  printf("%s", v.as.b ? "true" : "false"); break;
        case RT_NULL:  printf("null"); break;
        case RT_STR:   printf("%s", v.as.s->data); break;
        case RT_ARRAY: {
            printf("[");
            for (size_t i = 0; i < v.as.arr->len; i++) {
                if (i) printf(", ");
                print_rtval(v.as.arr->data[i]);
            }
            printf("]");
            break;
        }
        case RT_MAP: {
            printf("{");
            bool first = true;
            for (size_t i = 0; i < v.as.map->cap; i++) {
                if (v.as.map->entries[i].used == 1) {
                    if (!first) printf(", ");
                    print_rtval(v.as.map->entries[i].key);
                    printf(": ");
                    print_rtval(v.as.map->entries[i].value);
                    first = false;
                }
            }
            printf("}");
            break;
        }
        case RT_PTR: printf("<ptr:%p>", v.as.ptr); break;
    }
}

void bp_print_val(RtVal v) {
    print_rtval(v);
    printf("\n");
}

void bp_print_vals(int count, ...) {
    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++) {
        if (i) printf(" ");
        RtVal v = va_arg(ap, RtVal);
        print_rtval(v);
    }
    va_end(ap);
    printf("\n");
}

/* ───────────────────────────────────────────────────── */
/*  Conversion                                           */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_to_str(RtVal v) {
    char buf[128];
    switch (v.type) {
        case RT_INT:   snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i); return bp_str_from_cstr(buf);
        case RT_FLOAT: snprintf(buf, sizeof(buf), "%g", v.as.f); return bp_str_from_cstr(buf);
        case RT_BOOL:  return bp_str_from_cstr(v.as.b ? "true" : "false");
        case RT_NULL:  return bp_str_from_cstr("null");
        case RT_STR:   return v.as.s;
        default:       return bp_str_from_cstr("<?>");
    }
}

int64_t bp_to_int(RtVal v) {
    switch (v.type) {
        case RT_INT: return v.as.i;
        case RT_FLOAT: return (int64_t)v.as.f;
        case RT_BOOL: return v.as.b ? 1 : 0;
        default: return 0;
    }
}

/* ───────────────────────────────────────────────────── */
/*  String operations                                    */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_read_line(void) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) { free(line); return bp_str_new("", 0); }
    while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) { line[n-1] = '\0'; n--; }
    BpRtStr *s = bp_str_new(line, (size_t)n);
    free(line);
    return s;
}

int64_t bp_len(BpRtStr *s) { return (int64_t)s->len; }

BpRtStr *bp_substr(BpRtStr *s, int64_t start, int64_t length) {
    if (start < 0) start = 0;
    if (length < 0) length = 0;
    if ((size_t)start > s->len) start = (int64_t)s->len;
    size_t max_len = s->len - (size_t)start;
    size_t n = (size_t)length;
    if (n > max_len) n = max_len;
    return bp_str_new(s->data + start, n);
}

BpRtStr *bp_str_upper(BpRtStr *s) {
    BpRtStr *r = bp_str_new(s->data, s->len);
    for (size_t i = 0; i < r->len; i++) r->data[i] = (char)toupper((unsigned char)r->data[i]);
    return r;
}

BpRtStr *bp_str_lower(BpRtStr *s) {
    BpRtStr *r = bp_str_new(s->data, s->len);
    for (size_t i = 0; i < r->len; i++) r->data[i] = (char)tolower((unsigned char)r->data[i]);
    return r;
}

BpRtStr *bp_str_trim(BpRtStr *s) {
    const char *start = s->data;
    const char *end = s->data + s->len;
    while (start < end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    return bp_str_new(start, (size_t)(end - start));
}

bool bp_starts_with(BpRtStr *s, BpRtStr *prefix) {
    if (prefix->len > s->len) return false;
    return memcmp(s->data, prefix->data, prefix->len) == 0;
}

bool bp_ends_with(BpRtStr *s, BpRtStr *suffix) {
    if (suffix->len > s->len) return false;
    return memcmp(s->data + s->len - suffix->len, suffix->data, suffix->len) == 0;
}

int64_t bp_str_find(BpRtStr *s, BpRtStr *sub) {
    char *p = strstr(s->data, sub->data);
    if (!p) return -1;
    return (int64_t)(p - s->data);
}

BpRtStr *bp_str_replace(BpRtStr *s, BpRtStr *old_s, BpRtStr *new_s) {
    if (old_s->len == 0) return bp_str_new(s->data, s->len);
    /* Count occurrences */
    size_t count = 0;
    const char *p = s->data;
    while ((p = strstr(p, old_s->data)) != NULL) { count++; p += old_s->len; }
    if (count == 0) return bp_str_new(s->data, s->len);

    size_t new_len = s->len + count * (new_s->len - old_s->len);
    BpRtStr *r = bp_str_new(NULL, new_len);
    char *dst = r->data;
    p = s->data;
    const char *q;
    while ((q = strstr(p, old_s->data)) != NULL) {
        size_t chunk = (size_t)(q - p);
        memcpy(dst, p, chunk);
        dst += chunk;
        memcpy(dst, new_s->data, new_s->len);
        dst += new_s->len;
        p = q + old_s->len;
    }
    size_t remaining = (size_t)(s->data + s->len - p);
    memcpy(dst, p, remaining);
    r->data[new_len] = '\0';
    return r;
}

BpRtStr *bp_str_reverse(BpRtStr *s) {
    BpRtStr *r = bp_str_new(s->data, s->len);
    for (size_t i = 0; i < r->len / 2; i++) {
        char tmp = r->data[i];
        r->data[i] = r->data[r->len - 1 - i];
        r->data[r->len - 1 - i] = tmp;
    }
    return r;
}

BpRtStr *bp_str_repeat(BpRtStr *s, int64_t count) {
    if (count <= 0) return bp_str_new("", 0);
    size_t newlen = s->len * (size_t)count;
    BpRtStr *r = bp_str_new(NULL, newlen);
    for (int64_t i = 0; i < count; i++) memcpy(r->data + i * s->len, s->data, s->len);
    r->data[newlen] = '\0';
    return r;
}

BpRtStr *bp_str_pad_left(BpRtStr *s, int64_t width, BpRtStr *pad) {
    if ((int64_t)s->len >= width) return bp_str_new(s->data, s->len);
    size_t pad_count = (size_t)(width - (int64_t)s->len);
    BpRtStr *r = bp_str_new(NULL, (size_t)width);
    for (size_t i = 0; i < pad_count; i++) r->data[i] = pad->data[i % pad->len];
    memcpy(r->data + pad_count, s->data, s->len);
    r->data[(size_t)width] = '\0';
    return r;
}

BpRtStr *bp_str_pad_right(BpRtStr *s, int64_t width, BpRtStr *pad) {
    if ((int64_t)s->len >= width) return bp_str_new(s->data, s->len);
    size_t pad_count = (size_t)(width - (int64_t)s->len);
    BpRtStr *r = bp_str_new(NULL, (size_t)width);
    memcpy(r->data, s->data, s->len);
    for (size_t i = 0; i < pad_count; i++) r->data[s->len + i] = pad->data[i % pad->len];
    r->data[(size_t)width] = '\0';
    return r;
}

bool bp_str_contains(BpRtStr *s, BpRtStr *sub) {
    return strstr(s->data, sub->data) != NULL;
}

int64_t bp_str_count(BpRtStr *s, BpRtStr *sub) {
    if (sub->len == 0) return 0;
    int64_t count = 0;
    const char *p = s->data;
    while ((p = strstr(p, sub->data)) != NULL) { count++; p += sub->len; }
    return count;
}

BpRtStr *bp_str_char_at(BpRtStr *s, int64_t idx) {
    if (idx < 0 || (size_t)idx >= s->len) return bp_str_new("", 0);
    return bp_str_new(s->data + idx, 1);
}

int64_t bp_str_index_of(BpRtStr *s, BpRtStr *sub) {
    return bp_str_find(s, sub);
}

BpRtStr *bp_chr(int64_t code) {
    char c = (char)code;
    return bp_str_new(&c, 1);
}

int64_t bp_ord(BpRtStr *s) {
    if (s->len == 0) return 0;
    return (int64_t)(unsigned char)s->data[0];
}

BpRtStr *bp_int_to_hex(int64_t x) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llx", (long long)x);
    return bp_str_from_cstr(buf);
}

int64_t bp_hex_to_int(BpRtStr *s) {
    return (int64_t)strtoll(s->data, NULL, 16);
}

BpRtArray *bp_str_split(BpRtStr *s, BpRtStr *delim) {
    return bp_str_split_str(s, delim);
}

BpRtStr *bp_str_join(BpRtStr *delim, BpRtArray *arr) {
    return bp_str_join_arr(delim, arr);
}

BpRtArray *bp_str_split_str(BpRtStr *s, BpRtStr *delim) {
    BpRtArray *arr = bp_array_new(8);
    if (delim->len == 0) {
        bp_array_push(arr, rv_str(bp_str_new(s->data, s->len)));
        return arr;
    }
    const char *p = s->data;
    const char *end = s->data + s->len;
    const char *q;
    while ((q = strstr(p, delim->data)) != NULL) {
        bp_array_push(arr, rv_str(bp_str_new(p, (size_t)(q - p))));
        p = q + delim->len;
    }
    bp_array_push(arr, rv_str(bp_str_new(p, (size_t)(end - p))));
    return arr;
}

BpRtStr *bp_str_join_arr(BpRtStr *delim, BpRtArray *arr) {
    if (arr->len == 0) return bp_str_new("", 0);
    size_t total = 0;
    for (size_t i = 0; i < arr->len; i++) {
        if (arr->data[i].type == RT_STR) total += arr->data[i].as.s->len;
        if (i > 0) total += delim->len;
    }
    BpRtStr *r = bp_str_new(NULL, total);
    char *dst = r->data;
    for (size_t i = 0; i < arr->len; i++) {
        if (i > 0) { memcpy(dst, delim->data, delim->len); dst += delim->len; }
        if (arr->data[i].type == RT_STR) {
            memcpy(dst, arr->data[i].as.s->data, arr->data[i].as.s->len);
            dst += arr->data[i].as.s->len;
        }
    }
    r->data[total] = '\0';
    return r;
}

BpRtStr *bp_str_concat_all(BpRtArray *arr) {
    size_t total = 0;
    for (size_t i = 0; i < arr->len; i++) {
        if (arr->data[i].type == RT_STR) total += arr->data[i].as.s->len;
    }
    BpRtStr *r = bp_str_new(NULL, total);
    char *dst = r->data;
    for (size_t i = 0; i < arr->len; i++) {
        if (arr->data[i].type == RT_STR) {
            memcpy(dst, arr->data[i].as.s->data, arr->data[i].as.s->len);
            dst += arr->data[i].as.s->len;
        }
    }
    r->data[total] = '\0';
    return r;
}

/* ───────────────────────────────────────────────────── */
/*  Validation                                           */
/* ───────────────────────────────────────────────────── */
bool bp_is_digit(BpRtStr *s) {
    if (s->len == 0) return false;
    for (size_t i = 0; i < s->len; i++) if (!isdigit((unsigned char)s->data[i])) return false;
    return true;
}
bool bp_is_alpha(BpRtStr *s) {
    if (s->len == 0) return false;
    for (size_t i = 0; i < s->len; i++) if (!isalpha((unsigned char)s->data[i])) return false;
    return true;
}
bool bp_is_alnum(BpRtStr *s) {
    if (s->len == 0) return false;
    for (size_t i = 0; i < s->len; i++) if (!isalnum((unsigned char)s->data[i])) return false;
    return true;
}
bool bp_is_space(BpRtStr *s) {
    if (s->len == 0) return false;
    for (size_t i = 0; i < s->len; i++) if (!isspace((unsigned char)s->data[i])) return false;
    return true;
}

/* ───────────────────────────────────────────────────── */
/*  Math (int)                                           */
/* ───────────────────────────────────────────────────── */
int64_t bp_abs(int64_t x) { return x < 0 ? -x : x; }
int64_t bp_min(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t bp_max(int64_t a, int64_t b) { return a > b ? a : b; }
int64_t bp_pow_int(int64_t base, int64_t exp) {
    int64_t result = 1;
    for (int64_t i = 0; i < exp; i++) result *= base;
    return result;
}
int64_t bp_sqrt_int(int64_t x) { return (int64_t)sqrt((double)x); }
int64_t bp_clamp(int64_t x, int64_t lo, int64_t hi) { return x < lo ? lo : (x > hi ? hi : x); }
int64_t bp_sign(int64_t x) { return x > 0 ? 1 : (x < 0 ? -1 : 0); }
int64_t bp_floor_int(int64_t x) { return x; }
int64_t bp_ceil_int(int64_t x) { return x; }
int64_t bp_round_int(int64_t x) { return x; }

/* ───────────────────────────────────────────────────── */
/*  Math (float)                                         */
/* ───────────────────────────────────────────────────── */
double bp_fabs_f(double x)    { return fabs(x); }
double bp_ffloor(double x)    { return floor(x); }
double bp_fceil(double x)     { return ceil(x); }
double bp_fround(double x)    { return round(x); }
double bp_fsqrt(double x)     { return sqrt(x); }
double bp_fpow(double b, double e) { return pow(b, e); }

/* Trig */
double bp_sin(double x)        { return sin(x); }
double bp_cos(double x)        { return cos(x); }
double bp_tan(double x)        { return tan(x); }
double bp_asin_f(double x)     { return asin(x); }
double bp_acos_f(double x)     { return acos(x); }
double bp_atan_f(double x)     { return atan(x); }
double bp_atan2_f(double y, double x) { return atan2(y, x); }

/* Logarithms */
double bp_log_f(double x)     { return log(x); }
double bp_log10_f(double x)   { return log10(x); }
double bp_log2_f(double x)    { return log2(x); }
double bp_exp_f(double x)     { return exp(x); }

/* ───────────────────────────────────────────────────── */
/*  Float conversion                                     */
/* ───────────────────────────────────────────────────── */
double bp_int_to_float(int64_t x) { return (double)x; }
int64_t bp_float_to_int(double x) { return (int64_t)x; }
BpRtStr *bp_float_to_str(double x) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", x);
    return bp_str_from_cstr(buf);
}
double bp_str_to_float(BpRtStr *s) { return strtod(s->data, NULL); }
bool bp_is_nan(double x) { return isnan(x); }
bool bp_is_inf(double x) { return isinf(x); }

/* ───────────────────────────────────────────────────── */
/*  File I/O                                             */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_file_read(BpRtStr *path) {
    FILE *f = fopen(path->data, "rb");
    if (!f) return bp_str_new("", 0);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return bp_str_new("", 0); }
    char *buf = (char *)malloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    BpRtStr *s = bp_str_new(buf, n);
    free(buf);
    return s;
}

void bp_file_write(BpRtStr *path, BpRtStr *content) {
    FILE *f = fopen(path->data, "wb");
    if (!f) { fprintf(stderr, "cannot open %s for writing\n", path->data); exit(1); }
    fwrite(content->data, 1, content->len, f);
    fclose(f);
}

void bp_file_append(BpRtStr *path, BpRtStr *content) {
    FILE *f = fopen(path->data, "ab");
    if (!f) { fprintf(stderr, "cannot open %s for appending\n", path->data); exit(1); }
    fwrite(content->data, 1, content->len, f);
    fclose(f);
}

bool bp_file_exists(BpRtStr *path) {
    return access(path->data, F_OK) == 0;
}

void bp_file_delete(BpRtStr *path) {
    unlink(path->data);
}

int64_t bp_file_size(BpRtStr *path) {
    struct stat st;
    if (stat(path->data, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

void bp_file_copy(BpRtStr *src, BpRtStr *dst) {
    BpRtStr *content = bp_file_read(src);
    bp_file_write(dst, content);
    bp_str_free(content);
}

/* ───────────────────────────────────────────────────── */
/*  Base64                                               */
/* ───────────────────────────────────────────────────── */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

BpRtStr *bp_base64_encode(BpRtStr *s) {
    size_t olen = 4 * ((s->len + 2) / 3);
    BpRtStr *r = bp_str_new(NULL, olen);
    size_t i, j;
    for (i = 0, j = 0; i + 2 < s->len; i += 3) {
        unsigned int n = ((unsigned char)s->data[i] << 16) | ((unsigned char)s->data[i+1] << 8) | (unsigned char)s->data[i+2];
        r->data[j++] = b64_table[(n >> 18) & 63];
        r->data[j++] = b64_table[(n >> 12) & 63];
        r->data[j++] = b64_table[(n >> 6) & 63];
        r->data[j++] = b64_table[n & 63];
    }
    if (i < s->len) {
        unsigned int n = (unsigned char)s->data[i] << 16;
        if (i + 1 < s->len) n |= (unsigned char)s->data[i + 1] << 8;
        r->data[j++] = b64_table[(n >> 18) & 63];
        r->data[j++] = b64_table[(n >> 12) & 63];
        r->data[j++] = (i + 1 < s->len) ? b64_table[(n >> 6) & 63] : '=';
        r->data[j++] = '=';
    }
    r->data[j] = '\0';
    r->len = j;
    return r;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

BpRtStr *bp_base64_decode(BpRtStr *s) {
    size_t olen = s->len / 4 * 3;
    BpRtStr *r = bp_str_new(NULL, olen);
    size_t i, j = 0;
    for (i = 0; i + 3 < s->len; i += 4) {
        int a = b64_decode_char(s->data[i]);
        int b = b64_decode_char(s->data[i+1]);
        int c = b64_decode_char(s->data[i+2]);
        int d = b64_decode_char(s->data[i+3]);
        if (a < 0 || b < 0) break;
        unsigned int n = ((unsigned)a << 18) | ((unsigned)b << 12);
        if (c >= 0) n |= (unsigned)c << 6;
        if (d >= 0) n |= (unsigned)d;
        r->data[j++] = (char)(n >> 16);
        if (s->data[i+2] != '=') r->data[j++] = (char)((n >> 8) & 0xFF);
        if (s->data[i+3] != '=') r->data[j++] = (char)(n & 0xFF);
    }
    r->data[j] = '\0';
    r->len = j;
    return r;
}

/* ───────────────────────────────────────────────────── */
/*  Random                                               */
/* ───────────────────────────────────────────────────── */
int64_t bp_rand(void) { return (int64_t)rand(); }
int64_t bp_rand_range(int64_t lo, int64_t hi) {
    if (lo >= hi) return lo;
    return lo + (int64_t)(rand() % (int)(hi - lo));
}
void bp_rand_seed(int64_t seed) { srand((unsigned)seed); }

/* ───────────────────────────────────────────────────── */
/*  System                                               */
/* ───────────────────────────────────────────────────── */
int64_t bp_clock_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

void bp_sleep_ms(int64_t ms) {
    usleep((useconds_t)(ms * 1000));
}

BpRtStr *bp_getenv_str(BpRtStr *name) {
    const char *val = getenv(name->data);
    return val ? bp_str_from_cstr(val) : bp_str_new("", 0);
}

void bp_exit(int64_t code) { exit((int)code); }

BpRtStr *bp_argv_get(int64_t idx) {
    if (idx < 0 || idx >= g_argc) return bp_str_new("", 0);
    return bp_str_from_cstr(g_argv[idx]);
}

int64_t bp_argc_get(void) { return (int64_t)g_argc; }

/* Array operations */
int64_t bp_array_len(BpRtArray *arr) { return (int64_t)arr->len; }

/* Map operations */
int64_t bp_map_len(BpRtMap *map) { return (int64_t)map->count; }

/* ───────────────────────────────────────────────────── */
/*  Regex                                                */
/* ───────────────────────────────────────────────────── */
bool bp_regex_match(BpRtStr *pattern, BpRtStr *text) {
    regex_t re;
    if (regcomp(&re, pattern->data, REG_EXTENDED | REG_NOSUB) != 0) return false;
    int result = regexec(&re, text->data, 0, NULL, 0);
    regfree(&re);
    return result == 0;
}

BpRtStr *bp_regex_search(BpRtStr *pattern, BpRtStr *text) {
    regex_t re;
    regmatch_t match;
    if (regcomp(&re, pattern->data, REG_EXTENDED) != 0) return bp_str_new("", 0);
    if (regexec(&re, text->data, 1, &match, 0) != 0) { regfree(&re); return bp_str_new("", 0); }
    BpRtStr *result = bp_str_new(text->data + match.rm_so, (size_t)(match.rm_eo - match.rm_so));
    regfree(&re);
    return result;
}

BpRtStr *bp_regex_replace(BpRtStr *pattern, BpRtStr *replacement, BpRtStr *text) {
    regex_t re;
    regmatch_t match;
    if (regcomp(&re, pattern->data, REG_EXTENDED) != 0) return bp_str_new(text->data, text->len);

    /* Simple single replacement for now */
    if (regexec(&re, text->data, 1, &match, 0) != 0) { regfree(&re); return bp_str_new(text->data, text->len); }

    size_t new_len = (size_t)match.rm_so + replacement->len + (text->len - (size_t)match.rm_eo);
    BpRtStr *result = bp_str_new(NULL, new_len);
    memcpy(result->data, text->data, (size_t)match.rm_so);
    memcpy(result->data + match.rm_so, replacement->data, replacement->len);
    memcpy(result->data + match.rm_so + replacement->len, text->data + match.rm_eo, text->len - (size_t)match.rm_eo);
    result->data[new_len] = '\0';
    regfree(&re);
    return result;
}

BpRtArray *bp_regex_split(BpRtStr *pattern, BpRtStr *text) {
    BpRtArray *arr = bp_array_new(8);
    regex_t re;
    regmatch_t match;
    if (regcomp(&re, pattern->data, REG_EXTENDED) != 0) {
        bp_array_push(arr, rv_str(bp_str_new(text->data, text->len)));
        return arr;
    }
    const char *p = text->data;
    while (regexec(&re, p, 1, &match, 0) == 0) {
        bp_array_push(arr, rv_str(bp_str_new(p, (size_t)match.rm_so)));
        p += match.rm_eo;
    }
    bp_array_push(arr, rv_str(bp_str_from_cstr(p)));
    regfree(&re);
    return arr;
}

BpRtArray *bp_regex_find_all(BpRtStr *pattern, BpRtStr *text) {
    BpRtArray *arr = bp_array_new(8);
    regex_t re;
    regmatch_t match;
    if (regcomp(&re, pattern->data, REG_EXTENDED) != 0) return arr;
    const char *p = text->data;
    while (regexec(&re, p, 1, &match, 0) == 0) {
        bp_array_push(arr, rv_str(bp_str_new(p + match.rm_so, (size_t)(match.rm_eo - match.rm_so))));
        p += match.rm_eo;
        if (match.rm_so == match.rm_eo) p++;  /* avoid infinite loop on zero-length match */
    }
    regfree(&re);
    return arr;
}

/* ───────────────────────────────────────────────────── */
/*  Security (stub implementations)                      */
/* ───────────────────────────────────────────────────── */
BpRtStr *bp_hash_sha256(BpRtStr *s) {
    /* Simple hash stub - replace with real SHA-256 if needed */
    (void)s;
    return bp_str_from_cstr("<sha256-not-implemented-in-transpiled-mode>");
}

BpRtStr *bp_hash_md5(BpRtStr *s) {
    (void)s;
    return bp_str_from_cstr("<md5-not-implemented-in-transpiled-mode>");
}

bool bp_secure_compare(BpRtStr *a, BpRtStr *b) {
    if (a->len != b->len) return false;
    volatile unsigned char result = 0;
    for (size_t i = 0; i < a->len; i++) {
        result |= (unsigned char)a->data[i] ^ (unsigned char)b->data[i];
    }
    return result == 0;
}

BpRtStr *bp_random_bytes(int64_t n) {
    if (n <= 0) return bp_str_new("", 0);
    BpRtStr *s = bp_str_new(NULL, (size_t)n);
    /* Read from /dev/urandom for secure random bytes */
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(s->data, 1, (size_t)n, f);
        fclose(f);
        (void)got;
    } else {
        for (int64_t i = 0; i < n; i++) s->data[i] = (char)(rand() & 0xFF);
    }
    s->data[n] = '\0';
    return s;
}
