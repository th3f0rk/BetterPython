#include "types.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    char *name;
    Type type;
} Binding;

typedef struct {
    Binding *items;
    size_t len;
    size_t cap;
} Scope;

// Scope marker for block-level scoping
// When entering a block, we save the current scope length
// When exiting, we restore to that length (removing block-local variables)
static size_t scope_mark(Scope *s) {
    return s->len;
}

static void scope_restore(Scope *s, size_t mark) {
    // Free variable names that were added after the mark
    for (size_t i = mark; i < s->len; i++) {
        free(s->items[i].name);
    }
    s->len = mark;
}

// Function signature for user-defined functions
typedef struct {
    char *name;
    Type *param_types;
    size_t param_count;
    Type ret_type;
    size_t fn_index;  // index in module's function array
} FnSig;

typedef struct {
    FnSig *items;
    size_t len;
    size_t cap;
} FnTable;

// Struct type info for type checking
typedef struct {
    char *name;
    char **field_names;
    Type *field_types;
    size_t field_count;
    size_t struct_index;
} StructInfo;

typedef struct {
    StructInfo *items;
    size_t len;
    size_t cap;
} StructTable;

// Class type info for type checking
typedef struct {
    char *name;
    char *parent_name;
    char **field_names;
    Type *field_types;
    size_t field_count;
    size_t class_index;
} ClassInfo;

typedef struct {
    ClassInfo *items;
    size_t len;
    size_t cap;
} ClassTable;

// Enum type info for type checking
typedef struct {
    char *name;
    char **variant_names;
    int64_t *variant_values;
    size_t variant_count;
} EnumInfo;

typedef struct {
    EnumInfo *items;
    size_t len;
    size_t cap;
} EnumTable;

static Module *g_module = NULL;  // Global reference for function lookup
static FnTable g_fntable = {0};
static StructTable g_structtable = {0};
static ClassTable g_classtable = {0};
static EnumTable g_enumtable = {0};

// Lambda tracking
static size_t g_lambda_counter = 0;
static size_t g_lambda_base_index = 0; // fn_index for first lambda

// Import tracking for module system
typedef struct {
    char *module_name;      // Original module name
    char *alias;            // Alias or NULL
    char **import_names;    // Specific imports or NULL for all
    size_t import_count;
} ImportInfo;

static ImportInfo *g_imports = NULL;
static size_t g_import_count = 0;
static size_t g_import_cap = 0;

// Check if a name is an imported module
static bool is_imported_module(const char *name) {
    for (size_t i = 0; i < g_import_count; i++) {
        const char *check_name = g_imports[i].alias ? g_imports[i].alias : g_imports[i].module_name;
        if (strcmp(check_name, name) == 0) {
            return true;
        }
    }
    return false;
}

// Get actual module name from alias or name
static const char *get_module_name(const char *name) {
    for (size_t i = 0; i < g_import_count; i++) {
        const char *check_name = g_imports[i].alias ? g_imports[i].alias : g_imports[i].module_name;
        if (strcmp(check_name, name) == 0) {
            return g_imports[i].module_name;
        }
    }
    return NULL;
}

static void scope_put(Scope *s, const char *name, Type t, size_t line) {
    for (size_t i = 0; i < s->len; i++) {
        if (strcmp(s->items[i].name, name) == 0) bp_fatal_at(line, "duplicate symbol '%s'", name);
    }
    if (s->len + 1 > s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->items = bp_xrealloc(s->items, s->cap * sizeof(*s->items));
    }
    s->items[s->len].name = bp_xstrdup(name);
    s->items[s->len].type = t;
    s->len++;
}

static bool scope_get(Scope *s, const char *name, Type *out) {
    for (size_t i = 0; i < s->len; i++) {
        if (strcmp(s->items[i].name, name) == 0) { *out = s->items[i].type; return true; }
    }
    return false;
}

static void fntable_add(FnTable *ft, const char *name, Type *params, size_t pc, Type ret, size_t idx) {
    if (ft->len + 1 > ft->cap) {
        ft->cap = ft->cap ? ft->cap * 2 : 16;
        ft->items = bp_xrealloc(ft->items, ft->cap * sizeof(*ft->items));
    }
    ft->items[ft->len].name = bp_xstrdup(name);
    ft->items[ft->len].param_types = params;
    ft->items[ft->len].param_count = pc;
    ft->items[ft->len].ret_type = ret;
    ft->items[ft->len].fn_index = idx;
    ft->len++;
}

static FnSig *fntable_get(FnTable *ft, const char *name) {
    for (size_t i = 0; i < ft->len; i++) {
        if (strcmp(ft->items[i].name, name) == 0) return &ft->items[i];
    }
    return NULL;
}

static void fntable_free(FnTable *ft) {
    for (size_t i = 0; i < ft->len; i++) {
        free(ft->items[i].name);
        free(ft->items[i].param_types);
    }
    free(ft->items);
    memset(ft, 0, sizeof(*ft));
}

// Public API for multi-module support
static bool g_preserve_external_functions = false;

void types_register_function(const char *name, Type *param_types, size_t param_count,
                             Type ret_type, size_t fn_index) {
    fntable_add(&g_fntable, name, param_types, param_count, ret_type, fn_index);
}

void types_clear_external_functions(void) {
    fntable_free(&g_fntable);
}

void types_set_preserve_external(bool preserve) {
    g_preserve_external_functions = preserve;
}

static void structtable_add(StructTable *st, const char *name, char **field_names, Type *field_types, size_t fc, size_t idx) {
    if (st->len + 1 > st->cap) {
        st->cap = st->cap ? st->cap * 2 : 16;
        st->items = bp_xrealloc(st->items, st->cap * sizeof(*st->items));
    }
    st->items[st->len].name = bp_xstrdup(name);
    st->items[st->len].field_names = field_names;
    st->items[st->len].field_types = field_types;
    st->items[st->len].field_count = fc;
    st->items[st->len].struct_index = idx;
    st->len++;
}

static StructInfo *structtable_get(StructTable *st, const char *name) {
    for (size_t i = 0; i < st->len; i++) {
        if (strcmp(st->items[i].name, name) == 0) return &st->items[i];
    }
    return NULL;
}

static int structtable_get_field_index(StructInfo *si, const char *field_name) {
    for (size_t i = 0; i < si->field_count; i++) {
        if (strcmp(si->field_names[i], field_name) == 0) return (int)i;
    }
    return -1;
}

static void structtable_free(StructTable *st) {
    for (size_t i = 0; i < st->len; i++) {
        free(st->items[i].name);
    }
    free(st->items);
    memset(st, 0, sizeof(*st));
}

static void enumtable_add(EnumTable *et, const char *name, char **variant_names, int64_t *variant_values, size_t vc) {
    if (et->len + 1 > et->cap) {
        et->cap = et->cap ? et->cap * 2 : 16;
        et->items = bp_xrealloc(et->items, et->cap * sizeof(*et->items));
    }
    et->items[et->len].name = bp_xstrdup(name);
    et->items[et->len].variant_names = variant_names;
    et->items[et->len].variant_values = variant_values;
    et->items[et->len].variant_count = vc;
    et->len++;
}

static EnumInfo *enumtable_get(EnumTable *et, const char *name) {
    for (size_t i = 0; i < et->len; i++) {
        if (strcmp(et->items[i].name, name) == 0) return &et->items[i];
    }
    return NULL;
}

static void enumtable_free(EnumTable *et) {
    for (size_t i = 0; i < et->len; i++) {
        free(et->items[i].name);
    }
    free(et->items);
    memset(et, 0, sizeof(*et));
}

static void classtable_add(ClassTable *ct, const char *name, const char *parent_name,
                           char **field_names, Type *field_types, size_t fc, size_t idx) {
    if (ct->len + 1 > ct->cap) {
        ct->cap = ct->cap ? ct->cap * 2 : 16;
        ct->items = bp_xrealloc(ct->items, ct->cap * sizeof(*ct->items));
    }
    ct->items[ct->len].name = bp_xstrdup(name);
    ct->items[ct->len].parent_name = parent_name ? bp_xstrdup(parent_name) : NULL;
    ct->items[ct->len].field_names = field_names;
    ct->items[ct->len].field_types = field_types;
    ct->items[ct->len].field_count = fc;
    ct->items[ct->len].class_index = idx;
    ct->len++;
}

static ClassInfo *classtable_get(ClassTable *ct, const char *name) {
    for (size_t i = 0; i < ct->len; i++) {
        if (strcmp(ct->items[i].name, name) == 0) return &ct->items[i];
    }
    return NULL;
}

static int classtable_get_field_index(ClassInfo *ci, const char *field_name) {
    for (size_t i = 0; i < ci->field_count; i++) {
        if (strcmp(ci->field_names[i], field_name) == 0) return (int)i;
    }
    return -1;
}

static void classtable_free(ClassTable *ct) {
    for (size_t i = 0; i < ct->len; i++) {
        free(ct->items[i].name);
        free(ct->items[i].parent_name);
    }
    free(ct->items);
    memset(ct, 0, sizeof(*ct));
}

static bool type_eq(Type a, Type b) {
    if (a.kind != b.kind) return false;
    if (a.kind == TY_ARRAY) {
        if (!a.elem_type || !b.elem_type) return a.elem_type == b.elem_type;
        return type_eq(*a.elem_type, *b.elem_type);
    }
    if (a.kind == TY_MAP) {
        if (!a.key_type || !b.key_type) return false;
        if (!a.elem_type || !b.elem_type) return false;
        return type_eq(*a.key_type, *b.key_type) && type_eq(*a.elem_type, *b.elem_type);
    }
    if (a.kind == TY_STRUCT || a.kind == TY_ENUM || a.kind == TY_CLASS) {
        if (!a.struct_name || !b.struct_name) return false;
        return strcmp(a.struct_name, b.struct_name) == 0;
    }
    if (a.kind == TY_PTR) {
        // Null/void pointers are compatible with any pointer
        if (!a.elem_type || !b.elem_type) return true;
        return type_eq(*a.elem_type, *b.elem_type);
    }
    if (a.kind == TY_TUPLE) {
        if (a.tuple_len != b.tuple_len) return false;
        for (size_t i = 0; i < a.tuple_len; i++) {
            if (!type_eq(*a.tuple_types[i], *b.tuple_types[i])) return false;
        }
        return true;
    }
    if (a.kind == TY_FUNC) {
        if (a.param_count != b.param_count) return false;
        for (size_t i = 0; i < a.param_count; i++) {
            if (!type_eq(*a.param_types[i], *b.param_types[i])) return false;
        }
        if (!a.return_type || !b.return_type) return a.return_type == b.return_type;
        return type_eq(*a.return_type, *b.return_type);
    }
    return true;
}

static const char *type_name(Type t) {
    static char buf[256];
    switch (t.kind) {
        case TY_INT: return "int";
        case TY_FLOAT: return "float";
        case TY_BOOL: return "bool";
        case TY_STR: return "str";
        case TY_VOID: return "void";
        case TY_I8: return "i8";
        case TY_I16: return "i16";
        case TY_I32: return "i32";
        case TY_I64: return "i64";
        case TY_U8: return "u8";
        case TY_U16: return "u16";
        case TY_U32: return "u32";
        case TY_U64: return "u64";
        case TY_ARRAY:
            if (t.elem_type) {
                snprintf(buf, sizeof(buf), "[%s]", type_name(*t.elem_type));
                return buf;
            }
            return "[?]";
        case TY_MAP:
            if (t.key_type && t.elem_type) {
                snprintf(buf, sizeof(buf), "{%s: %s}", type_name(*t.key_type), type_name(*t.elem_type));
                return buf;
            }
            return "{?: ?}";
        case TY_STRUCT:
            if (t.struct_name) return t.struct_name;
            return "<struct>";
        case TY_ENUM:
            if (t.struct_name) return t.struct_name;
            return "<enum>";
        case TY_CLASS:
            if (t.struct_name) return t.struct_name;
            return "<class>";
        case TY_PTR:
            if (t.elem_type) {
                snprintf(buf, sizeof(buf), "ptr<%s>", type_name(*t.elem_type));
                return buf;
            }
            return "ptr";
        case TY_TUPLE: {
            size_t pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "(");
            for (size_t i = 0; i < t.tuple_len; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_name(*t.tuple_types[i]));
            }
            snprintf(buf + pos, sizeof(buf) - pos, ")");
            return buf;
        }
        case TY_FUNC:
            snprintf(buf, sizeof(buf), "fn(...) -> %s", t.return_type ? type_name(*t.return_type) : "void");
            return buf;
        default: return "?";
    }
}

static Type check_expr(Expr *e, Scope *s);

// Check if a name is a builtin function
static bool is_builtin(const char *name) {
    // I/O
    if (strcmp(name, "print") == 0) return true;
    if (strcmp(name, "read_line") == 0) return true;

    // String ops
    if (strcmp(name, "len") == 0) return true;
    if (strcmp(name, "substr") == 0) return true;
    if (strcmp(name, "to_str") == 0) return true;
    if (strcmp(name, "str_upper") == 0) return true;
    if (strcmp(name, "str_lower") == 0) return true;
    if (strcmp(name, "str_trim") == 0) return true;
    if (strcmp(name, "starts_with") == 0) return true;
    if (strcmp(name, "ends_with") == 0) return true;
    if (strcmp(name, "str_find") == 0) return true;
    if (strcmp(name, "str_replace") == 0) return true;
    if (strcmp(name, "str_reverse") == 0) return true;
    if (strcmp(name, "str_repeat") == 0) return true;
    if (strcmp(name, "str_pad_left") == 0) return true;
    if (strcmp(name, "str_pad_right") == 0) return true;
    if (strcmp(name, "str_contains") == 0) return true;
    if (strcmp(name, "str_count") == 0) return true;
    if (strcmp(name, "str_char_at") == 0) return true;
    if (strcmp(name, "str_index_of") == 0) return true;

    // Type conversion
    if (strcmp(name, "chr") == 0) return true;
    if (strcmp(name, "ord") == 0) return true;
    if (strcmp(name, "int_to_hex") == 0) return true;
    if (strcmp(name, "hex_to_int") == 0) return true;

    // Math (int)
    if (strcmp(name, "abs") == 0) return true;
    if (strcmp(name, "min") == 0) return true;
    if (strcmp(name, "max") == 0) return true;
    if (strcmp(name, "pow") == 0) return true;
    if (strcmp(name, "sqrt") == 0) return true;
    if (strcmp(name, "floor") == 0) return true;
    if (strcmp(name, "ceil") == 0) return true;
    if (strcmp(name, "round") == 0) return true;
    if (strcmp(name, "clamp") == 0) return true;
    if (strcmp(name, "sign") == 0) return true;

    // Float math
    if (strcmp(name, "sin") == 0) return true;
    if (strcmp(name, "cos") == 0) return true;
    if (strcmp(name, "tan") == 0) return true;
    if (strcmp(name, "asin") == 0) return true;
    if (strcmp(name, "acos") == 0) return true;
    if (strcmp(name, "atan") == 0) return true;
    if (strcmp(name, "atan2") == 0) return true;
    if (strcmp(name, "log") == 0) return true;
    if (strcmp(name, "log10") == 0) return true;
    if (strcmp(name, "log2") == 0) return true;
    if (strcmp(name, "exp") == 0) return true;
    if (strcmp(name, "fabs") == 0) return true;
    if (strcmp(name, "ffloor") == 0) return true;
    if (strcmp(name, "fceil") == 0) return true;
    if (strcmp(name, "fround") == 0) return true;
    if (strcmp(name, "fsqrt") == 0) return true;
    if (strcmp(name, "fpow") == 0) return true;

    // Float conversion
    if (strcmp(name, "int_to_float") == 0) return true;
    if (strcmp(name, "float_to_int") == 0) return true;
    if (strcmp(name, "float_to_str") == 0) return true;
    if (strcmp(name, "str_to_float") == 0) return true;
    if (strcmp(name, "is_nan") == 0) return true;
    if (strcmp(name, "is_inf") == 0) return true;

    // Random
    if (strcmp(name, "rand") == 0) return true;
    if (strcmp(name, "rand_range") == 0) return true;
    if (strcmp(name, "rand_seed") == 0) return true;

    // File
    if (strcmp(name, "file_read") == 0) return true;
    if (strcmp(name, "file_write") == 0) return true;
    if (strcmp(name, "file_exists") == 0) return true;
    if (strcmp(name, "file_delete") == 0) return true;
    if (strcmp(name, "file_append") == 0) return true;
    if (strcmp(name, "file_size") == 0) return true;
    if (strcmp(name, "file_copy") == 0) return true;

    // System
    if (strcmp(name, "clock_ms") == 0) return true;
    if (strcmp(name, "exit") == 0) return true;
    if (strcmp(name, "sleep") == 0) return true;
    if (strcmp(name, "getenv") == 0) return true;

    // Security
    if (strcmp(name, "hash_sha256") == 0) return true;
    if (strcmp(name, "hash_md5") == 0) return true;
    if (strcmp(name, "secure_compare") == 0) return true;
    if (strcmp(name, "random_bytes") == 0) return true;

    // Encoding
    if (strcmp(name, "base64_encode") == 0) return true;
    if (strcmp(name, "base64_decode") == 0) return true;

    // Validation
    if (strcmp(name, "is_digit") == 0) return true;
    if (strcmp(name, "is_alpha") == 0) return true;
    if (strcmp(name, "is_alnum") == 0) return true;
    if (strcmp(name, "is_space") == 0) return true;

    // Array functions
    if (strcmp(name, "array_len") == 0) return true;
    if (strcmp(name, "array_push") == 0) return true;
    if (strcmp(name, "array_pop") == 0) return true;

    // Map functions
    if (strcmp(name, "map_len") == 0) return true;
    if (strcmp(name, "map_keys") == 0) return true;
    if (strcmp(name, "map_values") == 0) return true;
    if (strcmp(name, "map_has_key") == 0) return true;
    if (strcmp(name, "map_delete") == 0) return true;

    // System functions
    if (strcmp(name, "argv") == 0) return true;
    if (strcmp(name, "argc") == 0) return true;

    // Threading
    if (strcmp(name, "thread_spawn") == 0) return true;
    if (strcmp(name, "thread_join") == 0) return true;
    if (strcmp(name, "thread_detach") == 0) return true;
    if (strcmp(name, "thread_current") == 0) return true;
    if (strcmp(name, "thread_yield") == 0) return true;
    if (strcmp(name, "thread_sleep") == 0) return true;
    if (strcmp(name, "mutex_new") == 0) return true;
    if (strcmp(name, "mutex_lock") == 0) return true;
    if (strcmp(name, "mutex_trylock") == 0) return true;
    if (strcmp(name, "mutex_unlock") == 0) return true;
    if (strcmp(name, "cond_new") == 0) return true;
    if (strcmp(name, "cond_wait") == 0) return true;
    if (strcmp(name, "cond_signal") == 0) return true;
    if (strcmp(name, "cond_broadcast") == 0) return true;

    // Regex operations
    if (strcmp(name, "regex_match") == 0) return true;
    if (strcmp(name, "regex_search") == 0) return true;
    if (strcmp(name, "regex_replace") == 0) return true;
    if (strcmp(name, "regex_split") == 0) return true;
    if (strcmp(name, "regex_find_all") == 0) return true;

    // String split/join - support both short and long names
    if (strcmp(name, "str_split") == 0 || strcmp(name, "str_split_str") == 0) return true;
    if (strcmp(name, "str_join") == 0 || strcmp(name, "str_join_arr") == 0) return true;
    if (strcmp(name, "str_concat_all") == 0) return true;

    // Bitwise operations
    if (strcmp(name, "bit_and") == 0) return true;
    if (strcmp(name, "bit_or") == 0) return true;
    if (strcmp(name, "bit_xor") == 0) return true;
    if (strcmp(name, "bit_not") == 0) return true;
    if (strcmp(name, "bit_shl") == 0) return true;
    if (strcmp(name, "bit_shr") == 0) return true;

    // Byte conversion
    if (strcmp(name, "bytes_to_str") == 0) return true;
    if (strcmp(name, "str_to_bytes") == 0) return true;

    // Type conversion
    if (strcmp(name, "parse_int") == 0) return true;

    // JSON
    if (strcmp(name, "json_stringify") == 0) return true;

    // Byte arrays
    if (strcmp(name, "bytes_new") == 0) return true;
    if (strcmp(name, "bytes_get") == 0) return true;
    if (strcmp(name, "bytes_set") == 0) return true;
    if (strcmp(name, "array_sort") == 0) return true;
    if (strcmp(name, "array_slice") == 0) return true;
    if (strcmp(name, "int_to_bytes") == 0) return true;
    if (strcmp(name, "int_from_bytes") == 0) return true;

    // Byte buffer operations
    if (strcmp(name, "bytes_append") == 0) return true;
    if (strcmp(name, "bytes_len") == 0) return true;
    if (strcmp(name, "bytes_write_u16") == 0) return true;
    if (strcmp(name, "bytes_write_u32") == 0) return true;
    if (strcmp(name, "bytes_write_i64") == 0) return true;
    if (strcmp(name, "bytes_read_u16") == 0) return true;
    if (strcmp(name, "bytes_read_u32") == 0) return true;
    if (strcmp(name, "bytes_read_i64") == 0) return true;

    // Binary file I/O
    if (strcmp(name, "file_read_bytes") == 0) return true;
    if (strcmp(name, "file_write_bytes") == 0) return true;

    // Array utilities
    if (strcmp(name, "array_insert") == 0) return true;
    if (strcmp(name, "array_remove") == 0) return true;

    // Type introspection
    if (strcmp(name, "typeof") == 0) return true;

    // Array utility builtins
    if (strcmp(name, "array_concat") == 0) return true;
    if (strcmp(name, "array_copy") == 0) return true;
    if (strcmp(name, "array_clear") == 0) return true;
    if (strcmp(name, "array_index_of") == 0) return true;
    if (strcmp(name, "array_contains") == 0) return true;
    if (strcmp(name, "array_reverse") == 0) return true;
    if (strcmp(name, "array_fill") == 0) return true;
    if (strcmp(name, "str_from_chars") == 0) return true;
    if (strcmp(name, "str_bytes") == 0) return true;

    return false;
}

static Type check_builtin_call(Expr *e, Scope *s) {
    const char *name = e->as.call.name;

    if (strcmp(name, "print") == 0) {
        for (size_t i = 0; i < e->as.call.argc; i++) check_expr(e->as.call.args[i], s);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "len") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "len expects 1 arg");
        Type t0 = check_expr(e->as.call.args[0], s);
        if (t0.kind != TY_STR) bp_fatal_at(e->line, "len expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "substr") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "substr expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "substr arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "substr arg1 must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal_at(e->line, "substr arg2 must be int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "read_line") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "read_line expects 0 args");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "to_str") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "to_str expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "clock_ms") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "clock_ms expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "exit") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "exit expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "exit expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "file_read") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "file_read expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_read expects str path");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "file_write") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "file_write expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_write arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "file_write arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "base64_encode") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "base64_encode expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "base64_encode expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "base64_decode") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "base64_decode expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "base64_decode expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "chr") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "chr expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "chr expects int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "ord") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "ord expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "ord expects str");
        e->inferred = type_int();
        return e->inferred;
    }

    // Math functions
    if (strcmp(name, "abs") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "abs expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "abs expects int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "%s arg0 must be int", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "%s arg1 must be int", name);
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "pow") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "pow expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "pow arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "pow arg1 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "sqrt") == 0 || strcmp(name, "floor") == 0 ||
        strcmp(name, "ceil") == 0 || strcmp(name, "round") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "%s expects int", name);
        e->inferred = type_int();
        return e->inferred;
    }

    // String functions
    if (strcmp(name, "str_upper") == 0 || strcmp(name, "str_lower") == 0 || strcmp(name, "str_trim") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s expects str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "starts_with") == 0 || strcmp(name, "ends_with") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg1 must be str", name);
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "str_find") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "str_find expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_find arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "str_find arg1 must be str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "str_replace") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "str_replace expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_replace arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "str_replace arg1 must be str");
        if (check_expr(e->as.call.args[2], s).kind != TY_STR) bp_fatal_at(e->line, "str_replace arg2 must be str");
        e->inferred = type_str();
        return e->inferred;
    }

    // Random functions
    if (strcmp(name, "rand") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "rand expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "rand_range") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "rand_range expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "rand_range arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "rand_range arg1 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "rand_seed") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "rand_seed expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "rand_seed expects int");
        e->inferred = type_void();
        return e->inferred;
    }

    // File functions
    if (strcmp(name, "file_exists") == 0 || strcmp(name, "file_delete") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s expects str", name);
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "file_append") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "file_append expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_append arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "file_append arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }

    // System functions
    if (strcmp(name, "sleep") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "sleep expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "sleep expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "getenv") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "getenv expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "getenv expects str");
        e->inferred = type_str();
        return e->inferred;
    }

    // Security functions
    if (strcmp(name, "hash_sha256") == 0 || strcmp(name, "hash_md5") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s expects str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "secure_compare") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "secure_compare expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "secure_compare arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "secure_compare arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "random_bytes") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "random_bytes expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "random_bytes expects int");
        e->inferred = type_str();
        return e->inferred;
    }

    // String utilities
    if (strcmp(name, "str_reverse") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "str_reverse expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_reverse expects str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_repeat") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "str_repeat expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_repeat arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "str_repeat arg1 must be int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_pad_left") == 0 || strcmp(name, "str_pad_right") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "%s expects 3 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "%s arg1 must be int", name);
        if (check_expr(e->as.call.args[2], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg2 must be str", name);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_contains") == 0 || strcmp(name, "str_count") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg0 must be str", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg1 must be str", name);
        if (strcmp(name, "str_contains") == 0) {
            e->inferred = type_bool();
        } else {
            e->inferred = type_int();
        }
        return e->inferred;
    }
    if (strcmp(name, "int_to_hex") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "int_to_hex expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "int_to_hex expects int");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "hex_to_int") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "hex_to_int expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "hex_to_int expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "str_char_at") == 0 || strcmp(name, "str_index_of") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s arg0 must be str", name);
        if (strcmp(name, "str_char_at") == 0) {
            if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "str_char_at arg1 must be int");
            e->inferred = type_str();
        } else {
            if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "str_index_of arg1 must be str");
            e->inferred = type_int();
        }
        return e->inferred;
    }

    // Validation functions
    if (strcmp(name, "is_digit") == 0 || strcmp(name, "is_alpha") == 0 ||
        strcmp(name, "is_alnum") == 0 || strcmp(name, "is_space") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "%s expects str", name);
        e->inferred = type_bool();
        return e->inferred;
    }

    // Math extensions
    if (strcmp(name, "clamp") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "clamp expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "clamp arg0 must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "clamp arg1 must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal_at(e->line, "clamp arg2 must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "sign") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "sign expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "sign expects int");
        e->inferred = type_int();
        return e->inferred;
    }

    // File utilities
    if (strcmp(name, "file_size") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "file_size expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_size expects str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "file_copy") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "file_copy expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_copy arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "file_copy arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }

    // Float math functions (single arg, return float)
    if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
        strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 || strcmp(name, "atan") == 0 ||
        strcmp(name, "log") == 0 || strcmp(name, "log10") == 0 || strcmp(name, "log2") == 0 ||
        strcmp(name, "exp") == 0 || strcmp(name, "fabs") == 0 || strcmp(name, "ffloor") == 0 ||
        strcmp(name, "fceil") == 0 || strcmp(name, "fround") == 0 || strcmp(name, "fsqrt") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal_at(e->line, "%s expects float", name);
        e->inferred = type_float();
        return e->inferred;
    }

    // Float math functions (two args, return float)
    if (strcmp(name, "atan2") == 0 || strcmp(name, "fpow") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal_at(e->line, "%s arg0 must be float", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_FLOAT) bp_fatal_at(e->line, "%s arg1 must be float", name);
        e->inferred = type_float();
        return e->inferred;
    }

    // Float conversion functions
    if (strcmp(name, "int_to_float") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "int_to_float expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "int_to_float expects int");
        e->inferred = type_float();
        return e->inferred;
    }
    if (strcmp(name, "float_to_int") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "float_to_int expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal_at(e->line, "float_to_int expects float");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "float_to_str") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "float_to_str expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal_at(e->line, "float_to_str expects float");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_to_float") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "str_to_float expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_to_float expects str");
        e->inferred = type_float();
        return e->inferred;
    }

    // Float utilities
    if (strcmp(name, "is_nan") == 0 || strcmp(name, "is_inf") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "%s expects 1 arg", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_FLOAT) bp_fatal_at(e->line, "%s expects float", name);
        e->inferred = type_bool();
        return e->inferred;
    }

    // Array functions
    if (strcmp(name, "array_len") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "array_len expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_ARRAY) bp_fatal_at(e->line, "array_len expects array, got %s", type_name(t));
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "array_push") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_push expects 2 args");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "array_push arg0 must be array");
        Type val_type = check_expr(e->as.call.args[1], s);
        if (arr_type.elem_type && !type_eq(val_type, *arr_type.elem_type)) {
            bp_fatal("array_push: element type mismatch: expected %s, got %s",
                     type_name(*arr_type.elem_type), type_name(val_type));
        }
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "array_pop") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "array_pop expects 1 arg");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "array_pop expects array");
        // Return the element type of the array
        if (arr_type.elem_type) {
            e->inferred = *arr_type.elem_type;
        } else {
            e->inferred = type_void();
        }
        return e->inferred;
    }

    // Map functions
    if (strcmp(name, "map_len") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "map_len expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_MAP) bp_fatal_at(e->line, "map_len expects map, got %s", type_name(t));
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "map_keys") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "map_keys expects 1 arg");
        Type map_type = check_expr(e->as.call.args[0], s);
        if (map_type.kind != TY_MAP) bp_fatal_at(e->line, "map_keys expects map, got %s", type_name(map_type));
        // Return array of key type
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        if (map_type.key_type) {
            arr_type.elem_type = type_new(map_type.key_type->kind);
        } else {
            arr_type.elem_type = type_new(TY_VOID);
        }
        e->inferred = arr_type;
        return e->inferred;
    }
    if (strcmp(name, "map_values") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "map_values expects 1 arg");
        Type map_type = check_expr(e->as.call.args[0], s);
        if (map_type.kind != TY_MAP) bp_fatal_at(e->line, "map_values expects map, got %s", type_name(map_type));
        // Return array of value type
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        if (map_type.elem_type) {
            arr_type.elem_type = type_new(map_type.elem_type->kind);
        } else {
            arr_type.elem_type = type_new(TY_VOID);
        }
        e->inferred = arr_type;
        return e->inferred;
    }
    if (strcmp(name, "map_has_key") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "map_has_key expects 2 args");
        Type map_type = check_expr(e->as.call.args[0], s);
        if (map_type.kind != TY_MAP) bp_fatal_at(e->line, "map_has_key arg0 must be map");
        Type key_type = check_expr(e->as.call.args[1], s);
        if (map_type.key_type && !type_eq(key_type, *map_type.key_type)) {
            bp_fatal("map_has_key: key type mismatch: expected %s, got %s",
                     type_name(*map_type.key_type), type_name(key_type));
        }
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "map_delete") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "map_delete expects 2 args");
        Type map_type = check_expr(e->as.call.args[0], s);
        if (map_type.kind != TY_MAP) bp_fatal_at(e->line, "map_delete arg0 must be map");
        Type key_type = check_expr(e->as.call.args[1], s);
        if (map_type.key_type && !type_eq(key_type, *map_type.key_type)) {
            bp_fatal("map_delete: key type mismatch: expected %s, got %s",
                     type_name(*map_type.key_type), type_name(key_type));
        }
        e->inferred = type_bool();
        return e->inferred;
    }

    // argv() - returns array of strings
    if (strcmp(name, "argv") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "argv expects 0 args");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_STR);
        arr_type.struct_name = NULL;
        arr_type.tuple_types = NULL;
        arr_type.tuple_len = 0;
        arr_type.param_types = NULL;
        arr_type.param_count = 0;
        arr_type.return_type = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }

    // argc() - returns int
    if (strcmp(name, "argc") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "argc expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }

    // Threading builtins
    if (strcmp(name, "thread_spawn") == 0) {
        // thread_spawn(func_idx, ...args) -> ptr
        e->inferred.kind = TY_PTR;
        e->inferred.elem_type = NULL;
        return e->inferred;
    }
    if (strcmp(name, "thread_join") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "thread_join expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "thread_join expects ptr");
        // Returns any value (the thread's result)
        e->inferred = type_int();  // For now, assume int return
        return e->inferred;
    }
    if (strcmp(name, "thread_detach") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "thread_detach expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "thread_detach expects ptr");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "thread_current") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "thread_current expects 0 args");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "thread_yield") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "thread_yield expects 0 args");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "thread_sleep") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "thread_sleep expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_INT) bp_fatal_at(e->line, "thread_sleep expects int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "mutex_new") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "mutex_new expects 0 args");
        e->inferred.kind = TY_PTR;
        e->inferred.elem_type = NULL;
        return e->inferred;
    }
    if (strcmp(name, "mutex_lock") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "mutex_lock expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "mutex_lock expects ptr");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "mutex_trylock") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "mutex_trylock expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "mutex_trylock expects ptr");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "mutex_unlock") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "mutex_unlock expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "mutex_unlock expects ptr");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "cond_new") == 0) {
        if (e->as.call.argc != 0) bp_fatal_at(e->line, "cond_new expects 0 args");
        e->inferred.kind = TY_PTR;
        e->inferred.elem_type = NULL;
        return e->inferred;
    }
    if (strcmp(name, "cond_wait") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "cond_wait expects 2 args");
        Type t1 = check_expr(e->as.call.args[0], s);
        Type t2 = check_expr(e->as.call.args[1], s);
        if (t1.kind != TY_PTR) bp_fatal_at(e->line, "cond_wait arg1 expects ptr");
        if (t2.kind != TY_PTR) bp_fatal_at(e->line, "cond_wait arg2 expects ptr");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "cond_signal") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "cond_signal expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "cond_signal expects ptr");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "cond_broadcast") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "cond_broadcast expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        if (t.kind != TY_PTR) bp_fatal_at(e->line, "cond_broadcast expects ptr");
        e->inferred = type_void();
        return e->inferred;
    }

    // Regex operations
    if (strcmp(name, "regex_match") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "regex_match expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "regex_match arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "regex_match arg1 must be str");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "regex_search") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "regex_search expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "regex_search arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "regex_search arg1 must be str");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "regex_replace") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "regex_replace expects 3 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "regex_replace arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "regex_replace arg1 must be str");
        if (check_expr(e->as.call.args[2], s).kind != TY_STR) bp_fatal_at(e->line, "regex_replace arg2 must be str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "regex_split") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "regex_split expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "regex_split arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "regex_split arg1 must be str");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_STR);
        arr_type.struct_name = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }
    if (strcmp(name, "regex_find_all") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "regex_find_all expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "regex_find_all arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "regex_find_all arg1 must be str");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_STR);
        arr_type.struct_name = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }

    // String split/join - support both short and long names
    if (strcmp(name, "str_split") == 0 || strcmp(name, "str_split_str") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "str_split expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_split arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "str_split arg1 must be str");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_STR);
        arr_type.struct_name = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }
    if (strcmp(name, "str_join") == 0 || strcmp(name, "str_join_arr") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "str_join expects 2 args");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "str_join arg0 must be array");
        if (check_expr(e->as.call.args[1], s).kind != TY_STR) bp_fatal_at(e->line, "str_join arg1 must be str");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_concat_all") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "str_concat_all expects 1 arg");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "str_concat_all arg0 must be array");
        e->inferred = type_str();
        return e->inferred;
    }

    // Bitwise operations: bit_and(a,b), bit_or(a,b), bit_xor(a,b) -> int
    if (strcmp(name, "bit_and") == 0 || strcmp(name, "bit_or") == 0 || strcmp(name, "bit_xor") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("%s arg0 must be int", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("%s arg1 must be int", name);
        e->inferred = type_int();
        return e->inferred;
    }
    // bit_not(a) -> int
    if (strcmp(name, "bit_not") == 0) {
        if (e->as.call.argc != 1) bp_fatal("bit_not expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("bit_not arg must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    // bit_shl(a, shift), bit_shr(a, shift) -> int
    if (strcmp(name, "bit_shl") == 0 || strcmp(name, "bit_shr") == 0) {
        if (e->as.call.argc != 2) bp_fatal("%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("%s arg0 must be int", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("%s arg1 must be int", name);
        e->inferred = type_int();
        return e->inferred;
    }

    // bytes_to_str(bytes) -> str
    if (strcmp(name, "bytes_to_str") == 0) {
        if (e->as.call.argc != 1) bp_fatal("bytes_to_str expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_str();
        return e->inferred;
    }
    // str_to_bytes(str) -> [int]  (byte array represented as int array)
    if (strcmp(name, "str_to_bytes") == 0) {
        if (e->as.call.argc != 1) bp_fatal("str_to_bytes expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("str_to_bytes arg must be str");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_INT);
        arr_type.struct_name = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }

    // parse_int(str) -> int
    if (strcmp(name, "parse_int") == 0) {
        if (e->as.call.argc != 1) bp_fatal("parse_int expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal("parse_int arg must be str");
        e->inferred = type_int();
        return e->inferred;
    }

    // json_stringify(value) -> str
    if (strcmp(name, "json_stringify") == 0) {
        if (e->as.call.argc != 1) bp_fatal("json_stringify expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_str();
        return e->inferred;
    }

    // bytes_new(size: int) -> [int]
    if (strcmp(name, "bytes_new") == 0) {
        if (e->as.call.argc != 1) bp_fatal("bytes_new expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("bytes_new arg must be int");
        Type arr_type;
        arr_type.kind = TY_ARRAY;
        arr_type.key_type = NULL;
        arr_type.elem_type = type_new(TY_INT);
        arr_type.struct_name = NULL;
        e->inferred = arr_type;
        return e->inferred;
    }
    // bytes_get(arr, idx) -> int
    if (strcmp(name, "bytes_get") == 0) {
        if (e->as.call.argc != 2) bp_fatal("bytes_get expects 2 args");
        check_expr(e->as.call.args[0], s);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("bytes_get idx must be int");
        e->inferred = type_int();
        return e->inferred;
    }
    // bytes_set(arr, idx, val) -> void
    if (strcmp(name, "bytes_set") == 0) {
        if (e->as.call.argc != 3) bp_fatal("bytes_set expects 3 args");
        check_expr(e->as.call.args[0], s);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("bytes_set idx must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal("bytes_set val must be int");
        e->inferred = type_void();
        return e->inferred;
    }
    // array_sort(arr) -> void  (in-place sort)
    if (strcmp(name, "array_sort") == 0) {
        if (e->as.call.argc != 1) bp_fatal("array_sort expects 1 arg");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "array_sort expects array");
        e->inferred = type_void();
        return e->inferred;
    }
    // array_slice(arr, start, end) -> [T]
    if (strcmp(name, "array_slice") == 0) {
        if (e->as.call.argc != 3) bp_fatal("array_slice expects 3 args");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "array_slice expects array");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("array_slice start must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal("array_slice end must be int");
        e->inferred = arr_type;
        return e->inferred;
    }
    // int_to_bytes(value, num_bytes) -> [int]
    if (strcmp(name, "int_to_bytes") == 0) {
        if (e->as.call.argc != 2) bp_fatal("int_to_bytes expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal("int_to_bytes value must be int");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("int_to_bytes size must be int");
        Type arr = {TY_ARRAY, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL};
        Type *elem = bp_xmalloc(sizeof(Type));
        *elem = type_int();
        arr.elem_type = elem;
        e->inferred = arr;
        return e->inferred;
    }
    // int_from_bytes(arr, offset, num_bytes) -> int
    if (strcmp(name, "int_from_bytes") == 0) {
        if (e->as.call.argc != 3) bp_fatal("int_from_bytes expects 3 args");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (arr_type.kind != TY_ARRAY) bp_fatal_at(e->line, "int_from_bytes expects array");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal("int_from_bytes offset must be int");
        if (check_expr(e->as.call.args[2], s).kind != TY_INT) bp_fatal("int_from_bytes size must be int");
        e->inferred = type_int();
        return e->inferred;
    }

    // Byte buffer operations
    if (strcmp(name, "bytes_append") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "bytes_append expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_ARRAY) bp_fatal_at(e->line, "bytes_append arg0 must be array");
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "bytes_append arg1 must be int");
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "bytes_len") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "bytes_len expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_ARRAY) bp_fatal_at(e->line, "bytes_len arg0 must be array");
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "bytes_write_u16") == 0 || strcmp(name, "bytes_write_u32") == 0 || strcmp(name, "bytes_write_i64") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_ARRAY) bp_fatal_at(e->line, "%s arg0 must be array", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "%s arg1 must be int", name);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "bytes_read_u16") == 0 || strcmp(name, "bytes_read_u32") == 0 || strcmp(name, "bytes_read_i64") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "%s expects 2 args", name);
        if (check_expr(e->as.call.args[0], s).kind != TY_ARRAY) bp_fatal_at(e->line, "%s arg0 must be array", name);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "%s arg1 must be int", name);
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "file_read_bytes") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "file_read_bytes expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_read_bytes expects str path");
        Type t;
        t.kind = TY_ARRAY;
        t.elem_type = type_new(TY_INT);
        t.key_type = NULL;
        t.struct_name = NULL;
        t.tuple_types = NULL;
        t.tuple_len = 0;
        t.param_types = NULL;
        t.param_count = 0;
        t.return_type = NULL;
        e->inferred = t;
        return e->inferred;
    }
    if (strcmp(name, "file_write_bytes") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "file_write_bytes expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "file_write_bytes arg0 must be str");
        if (check_expr(e->as.call.args[1], s).kind != TY_ARRAY) bp_fatal_at(e->line, "file_write_bytes arg1 must be array");
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "array_insert") == 0) {
        if (e->as.call.argc != 3) bp_fatal_at(e->line, "array_insert expects 3 args");
        check_expr(e->as.call.args[0], s);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "array_insert arg1 must be int");
        check_expr(e->as.call.args[2], s);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "array_remove") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_remove expects 2 args");
        Type arr_type = check_expr(e->as.call.args[0], s);
        if (check_expr(e->as.call.args[1], s).kind != TY_INT) bp_fatal_at(e->line, "array_remove arg1 must be int");
        e->inferred = arr_type.elem_type ? *arr_type.elem_type : type_int();
        return e->inferred;
    }
    if (strcmp(name, "typeof") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "typeof expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "array_concat") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_concat expects 2 args");
        Type t = check_expr(e->as.call.args[0], s);
        check_expr(e->as.call.args[1], s);
        e->inferred = t;
        return e->inferred;
    }
    if (strcmp(name, "array_copy") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "array_copy expects 1 arg");
        Type t = check_expr(e->as.call.args[0], s);
        e->inferred = t;
        return e->inferred;
    }
    if (strcmp(name, "array_clear") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "array_clear expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "array_index_of") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_index_of expects 2 args");
        check_expr(e->as.call.args[0], s);
        check_expr(e->as.call.args[1], s);
        e->inferred = type_int();
        return e->inferred;
    }
    if (strcmp(name, "array_contains") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_contains expects 2 args");
        check_expr(e->as.call.args[0], s);
        check_expr(e->as.call.args[1], s);
        e->inferred = type_bool();
        return e->inferred;
    }
    if (strcmp(name, "array_reverse") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "array_reverse expects 1 arg");
        check_expr(e->as.call.args[0], s);
        e->inferred = type_void();
        return e->inferred;
    }
    if (strcmp(name, "array_fill") == 0) {
        if (e->as.call.argc != 2) bp_fatal_at(e->line, "array_fill expects 2 args");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT) bp_fatal_at(e->line, "array_fill arg0 must be int");
        Type val_type = check_expr(e->as.call.args[1], s);
        Type t;
        t.kind = TY_ARRAY;
        t.elem_type = type_new(val_type.kind);
        t.key_type = NULL;
        t.struct_name = NULL;
        t.tuple_types = NULL;
        t.tuple_len = 0;
        t.param_types = NULL;
        t.param_count = 0;
        t.return_type = NULL;
        e->inferred = t;
        return e->inferred;
    }
    if (strcmp(name, "str_from_chars") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "str_from_chars expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_ARRAY) bp_fatal_at(e->line, "str_from_chars arg0 must be array");
        e->inferred = type_str();
        return e->inferred;
    }
    if (strcmp(name, "str_bytes") == 0) {
        if (e->as.call.argc != 1) bp_fatal_at(e->line, "str_bytes expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_STR) bp_fatal_at(e->line, "str_bytes expects str");
        Type t;
        t.kind = TY_ARRAY;
        t.elem_type = type_new(TY_INT);
        t.key_type = NULL;
        t.struct_name = NULL;
        t.tuple_types = NULL;
        t.tuple_len = 0;
        t.param_types = NULL;
        t.param_count = 0;
        t.return_type = NULL;
        e->inferred = t;
        return e->inferred;
    }

    bp_fatal("unknown builtin '%s'", name);
    return type_void();
}

static Type check_call(Expr *e, Scope *s) {
    const char *name = e->as.call.name;

    // First check if it's a builtin
    if (is_builtin(name)) {
        e->as.call.fn_index = -1;  // Mark as builtin
        return check_builtin_call(e, s);
    }

    // Look up user-defined function
    FnSig *fn = fntable_get(&g_fntable, name);
    if (!fn) {
        // Check if it's a variable with TY_FUNC type (lambda stored in variable)
        Type var_type = type_void();
        if (scope_get(s, name, &var_type) && var_type.kind == TY_FUNC && var_type.struct_name) {
            fn = fntable_get(&g_fntable, var_type.struct_name);
        }
        if (!fn) {
            bp_fatal("unknown function '%s'", name);
        }
    }

    // Check argument count
    if (e->as.call.argc != fn->param_count) {
        bp_fatal_at(e->line, "function '%s' expects %zu args, got %zu", name, fn->param_count, e->as.call.argc);
    }

    // Check argument types
    for (size_t i = 0; i < e->as.call.argc; i++) {
        Type arg_type = check_expr(e->as.call.args[i], s);
        if (!type_eq(arg_type, fn->param_types[i])) {
            bp_fatal("function '%s' arg %zu: expected %s, got %s",
                     name, i, type_name(fn->param_types[i]), type_name(arg_type));
        }
    }

    // Set function index and return type
    e->as.call.fn_index = (int)fn->fn_index;
    e->inferred = fn->ret_type;
    return e->inferred;
}

static Type check_expr(Expr *e, Scope *s) {
    switch (e->kind) {
        case EX_INT: e->inferred = type_int(); return e->inferred;
        case EX_FLOAT: e->inferred = type_float(); return e->inferred;
        case EX_STR: e->inferred = type_str(); return e->inferred;
        case EX_BOOL: e->inferred = type_bool(); return e->inferred;
        case EX_VAR: {
            Type t = type_void();
            if (!scope_get(s, e->as.var_name, &t)) bp_fatal("undefined variable '%s'", e->as.var_name);
            e->inferred = t;
            return t;
        }
        case EX_CALL:
            return check_call(e, s);
        case EX_ARRAY: {
            // Infer element type from first element, check all others match
            Type elem_type = type_void();
            if (e->as.array.len > 0) {
                elem_type = check_expr(e->as.array.elements[0], s);
                for (size_t i = 1; i < e->as.array.len; i++) {
                    Type et = check_expr(e->as.array.elements[i], s);
                    if (!type_eq(elem_type, et)) {
                        bp_fatal("array literal: inconsistent element types: %s vs %s",
                                 type_name(elem_type), type_name(et));
                    }
                }
            }
            Type arr_type;
            arr_type.kind = TY_ARRAY;
            arr_type.elem_type = type_new(elem_type.kind);
            if (elem_type.kind == TY_ARRAY && elem_type.elem_type) {
                arr_type.elem_type->elem_type = elem_type.elem_type;
            }
            e->inferred = arr_type;
            return e->inferred;
        }
        case EX_INDEX: {
            Type container_type = check_expr(e->as.index.array, s);
            Type idx_type = check_expr(e->as.index.index, s);

            if (container_type.kind == TY_ARRAY) {
                if (idx_type.kind != TY_INT) {
                    bp_fatal_at(e->line, "array index must be int, got %s", type_name(idx_type));
                }
                if (container_type.elem_type) {
                    e->inferred = *container_type.elem_type;
                } else {
                    e->inferred = type_void();
                }
            } else if (container_type.kind == TY_MAP) {
                if (container_type.key_type && !type_eq(idx_type, *container_type.key_type)) {
                    bp_fatal("map key type mismatch: expected %s, got %s",
                             type_name(*container_type.key_type), type_name(idx_type));
                }
                if (container_type.elem_type) {
                    e->inferred = *container_type.elem_type;
                } else {
                    e->inferred = type_void();
                }
            } else {
                bp_fatal("cannot index type: %s", type_name(container_type));
            }
            return e->inferred;
        }
        case EX_MAP: {
            // Infer key and value types from first element, check all others match
            Type key_type = type_void();
            Type val_type = type_void();
            if (e->as.map.len > 0) {
                key_type = check_expr(e->as.map.keys[0], s);
                val_type = check_expr(e->as.map.values[0], s);
                for (size_t i = 1; i < e->as.map.len; i++) {
                    Type kt = check_expr(e->as.map.keys[i], s);
                    Type vt = check_expr(e->as.map.values[i], s);
                    if (!type_eq(key_type, kt)) {
                        bp_fatal("map literal: inconsistent key types: %s vs %s",
                                 type_name(key_type), type_name(kt));
                    }
                    if (!type_eq(val_type, vt)) {
                        bp_fatal("map literal: inconsistent value types: %s vs %s",
                                 type_name(val_type), type_name(vt));
                    }
                }
            }
            Type map_type;
            map_type.kind = TY_MAP;
            map_type.key_type = type_new(key_type.kind);
            map_type.elem_type = type_new(val_type.kind);
            e->inferred = map_type;
            return e->inferred;
        }
        case EX_STRUCT_LITERAL: {
            // Look up struct definition
            StructInfo *si = structtable_get(&g_structtable, e->as.struct_literal.struct_name);
            if (!si) {
                bp_fatal("unknown struct type '%s'", e->as.struct_literal.struct_name);
            }
            // Check that all fields are provided and match expected types
            for (size_t i = 0; i < e->as.struct_literal.field_count; i++) {
                const char *fname = e->as.struct_literal.field_names[i];
                int fidx = structtable_get_field_index(si, fname);
                if (fidx < 0) {
                    bp_fatal("struct '%s' has no field '%s'", si->name, fname);
                }
                Type fval_type = check_expr(e->as.struct_literal.field_values[i], s);
                if (!type_eq(si->field_types[fidx], fval_type)) {
                    bp_fatal_at(e->line, "struct field '%s.%s' expects %s, got %s",
                             si->name, fname, type_name(si->field_types[fidx]), type_name(fval_type));
                }
            }
            // Check all fields are provided
            if (e->as.struct_literal.field_count != si->field_count) {
                bp_fatal("struct '%s' requires %zu fields, got %zu",
                         si->name, si->field_count, e->as.struct_literal.field_count);
            }
            Type st_type;
            st_type.kind = TY_STRUCT;
            st_type.elem_type = NULL;
            st_type.key_type = NULL;
            st_type.struct_name = si->name;
            e->inferred = st_type;
            return e->inferred;
        }
        case EX_FIELD_ACCESS: {
            // Check if this is an enum member access (e.g., Color.RED)
            if (e->as.field_access.object->kind == EX_VAR) {
                const char *name = e->as.field_access.object->as.var_name;
                EnumInfo *ei = enumtable_get(&g_enumtable, name);
                if (ei) {
                    // Transform to EX_ENUM_MEMBER
                    const char *member = e->as.field_access.field_name;
                    int64_t val = 0;
                    bool found = false;
                    for (size_t i = 0; i < ei->variant_count; i++) {
                        if (strcmp(ei->variant_names[i], member) == 0) {
                            val = ei->variant_values[i];
                            found = true;
                            break;
                        }
                    }
                    if (!found) bp_fatal("enum '%s' has no member '%s'", name, member);
                    e->kind = EX_ENUM_MEMBER;
                    e->as.enum_member.enum_name = bp_xstrdup(name);
                    e->as.enum_member.member_name = bp_xstrdup(member);
                    e->as.enum_member.member_value = (int)val;
                    e->inferred = type_int();
                    return e->inferred;
                }
            }
            Type obj_type = check_expr(e->as.field_access.object, s);
            if (obj_type.kind == TY_STRUCT) {
                StructInfo *si = structtable_get(&g_structtable, obj_type.struct_name);
                if (!si) {
                    bp_fatal("unknown struct type '%s'", obj_type.struct_name);
                }
                int fidx = structtable_get_field_index(si, e->as.field_access.field_name);
                if (fidx < 0) {
                    bp_fatal("struct '%s' has no field '%s'", si->name, e->as.field_access.field_name);
                }
                e->as.field_access.field_index = fidx;  // Store for compiler
                e->inferred = si->field_types[fidx];
                return e->inferred;
            } else if (obj_type.kind == TY_CLASS) {
                ClassInfo *ci = classtable_get(&g_classtable, obj_type.struct_name);
                if (!ci) {
                    bp_fatal("unknown class type '%s'", obj_type.struct_name);
                }
                int fidx = classtable_get_field_index(ci, e->as.field_access.field_name);
                if (fidx < 0) {
                    bp_fatal("class '%s' has no field '%s'", ci->name, e->as.field_access.field_name);
                }
                e->as.field_access.field_index = fidx;  // Store for compiler
                e->inferred = ci->field_types[fidx];
                return e->inferred;
            } else {
                bp_fatal("cannot access field '%s' on type %s",
                         e->as.field_access.field_name, type_name(obj_type));
            }
            return e->inferred;
        }
        case EX_UNARY: {
            Type r = check_expr(e->as.unary.rhs, s);
            if (e->as.unary.op == UOP_NEG) {
                if (r.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                if (r.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                bp_fatal_at(e->line, "unary - expects int or float");
            }
            if (e->as.unary.op == UOP_NOT) {
                if (r.kind != TY_BOOL) bp_fatal_at(e->line, "not expects bool");
                e->inferred = type_bool();
                return e->inferred;
            }
            if (e->as.unary.op == UOP_BIT_NOT) {
                if (r.kind != TY_INT) bp_fatal_at(e->line, "~ expects int");
                e->inferred = type_int();
                return e->inferred;
            }
            bp_fatal("unknown unary op");
            return type_void();
        }
        case EX_BINARY: {
            Type a = check_expr(e->as.binary.lhs, s);
            Type b = check_expr(e->as.binary.rhs, s);
            switch (e->as.binary.op) {
                case BOP_ADD:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                    if (a.kind == TY_STR && b.kind == TY_STR) { e->inferred = type_str(); return e->inferred; }
                    bp_fatal("type error: + supports int+int, float+float, or str+str");
                    break;
                case BOP_SUB: case BOP_MUL: case BOP_DIV:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_int(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_float(); return e->inferred; }
                    bp_fatal_at(e->line, "arithmetic expects int or float");
                    break;
                case BOP_MOD:
                    if (a.kind == TY_FLOAT || b.kind == TY_FLOAT) {
                        e->inferred = type_float();
                    } else if (a.kind == TY_INT && b.kind == TY_INT) {
                        e->inferred = type_int();
                    } else {
                        bp_fatal_at(e->line, "mod expects int or float");
                    }
                    return e->inferred;
                case BOP_EQ: case BOP_NEQ:
                    if (!type_eq(a, b)) bp_fatal("==/!= requires same types");
                    e->inferred = type_bool();
                    return e->inferred;
                case BOP_LT: case BOP_LTE: case BOP_GT: case BOP_GTE:
                    if (a.kind == TY_INT && b.kind == TY_INT) { e->inferred = type_bool(); return e->inferred; }
                    if (a.kind == TY_FLOAT && b.kind == TY_FLOAT) { e->inferred = type_bool(); return e->inferred; }
                    bp_fatal("comparisons expect int or float");
                    break;
                case BOP_AND: case BOP_OR:
                    if (a.kind != TY_BOOL || b.kind != TY_BOOL) bp_fatal("and/or expect bool");
                    e->inferred = type_bool();
                    return e->inferred;
                case BOP_BIT_AND: case BOP_BIT_OR: case BOP_BIT_XOR:
                case BOP_BIT_SHL: case BOP_BIT_SHR:
                    if (a.kind != TY_INT || b.kind != TY_INT) bp_fatal("bitwise ops expect int");
                    e->inferred = type_int();
                    return e->inferred;
                default: break;
            }
            bp_fatal("unknown binary op");
            return type_void();
        }
        case EX_TUPLE: {
            // Tuples are compiled as arrays. Check element types and infer
            // as array type when all elements have the same type.
            if (e->as.tuple.len == 0) {
                Type arr_type;
                arr_type.kind = TY_ARRAY;
                arr_type.key_type = NULL;
                arr_type.elem_type = type_new(TY_INT);
                arr_type.struct_name = NULL;
                e->inferred = arr_type;
                return e->inferred;
            }
            Type first = check_expr(e->as.tuple.elements[0], s);
            for (size_t i = 1; i < e->as.tuple.len; i++) {
                check_expr(e->as.tuple.elements[i], s);
            }
            Type arr_type;
            arr_type.kind = TY_ARRAY;
            arr_type.key_type = NULL;
            arr_type.elem_type = type_new(first.kind);
            *arr_type.elem_type = first;
            arr_type.struct_name = NULL;
            e->inferred = arr_type;
            return e->inferred;
        }
        case EX_LAMBDA: {
            // Build function type from lambda
            Type **param_types = NULL;
            if (e->as.lambda.paramc > 0) {
                param_types = bp_xmalloc(e->as.lambda.paramc * sizeof(*param_types));
                for (size_t i = 0; i < e->as.lambda.paramc; i++) {
                    param_types[i] = type_new(e->as.lambda.params[i].type.kind);
                    *param_types[i] = e->as.lambda.params[i].type;
                }
            }
            Type *ret_type = type_new(e->as.lambda.return_type.kind);
            *ret_type = e->as.lambda.return_type;

            // Type check the body in a new scope with parameters
            Scope body_scope = {0};
            for (size_t i = 0; i < s->len; i++) {
                scope_put(&body_scope, s->items[i].name, s->items[i].type, e->line);
            }
            for (size_t i = 0; i < e->as.lambda.paramc; i++) {
                scope_put(&body_scope, e->as.lambda.params[i].name, e->as.lambda.params[i].type, e->line);
            }
            Type body_type = check_expr(e->as.lambda.body, &body_scope);
            if (!type_eq(body_type, e->as.lambda.return_type)) {
                bp_fatal("lambda body type %s doesn't match declared return type %s",
                         type_name(body_type), type_name(e->as.lambda.return_type));
            }
            for (size_t i = 0; i < body_scope.len; i++) free(body_scope.items[i].name);
            free(body_scope.items);

            // Register lambda as anonymous function for compilation
            size_t fn_idx = g_lambda_base_index + g_lambda_counter;
            char lname[64];
            snprintf(lname, sizeof(lname), "__lambda_%zu", g_lambda_counter);
            g_lambda_counter++;

            // Register in function table so calls can resolve to it
            Type *pt_copy = NULL;
            if (e->as.lambda.paramc > 0) {
                pt_copy = bp_xmalloc(e->as.lambda.paramc * sizeof(Type));
                for (size_t i = 0; i < e->as.lambda.paramc; i++)
                    pt_copy[i] = e->as.lambda.params[i].type;
            }
            fntable_add(&g_fntable, lname, pt_copy, e->as.lambda.paramc,
                        e->as.lambda.return_type, fn_idx);

            e->inferred = *type_func(param_types, e->as.lambda.paramc, ret_type);
            // Store lambda name in struct_name for resolving calls through variables
            e->inferred.struct_name = bp_xstrdup(lname);
            return e->inferred;
        }
        case EX_FSTRING: {
            // Type-check all sub-expressions in the f-string
            for (size_t i = 0; i < e->as.fstring.partc; i++) {
                check_expr(e->as.fstring.parts[i], s);
            }
            e->inferred = type_str();
            return e->inferred;
        }
        case EX_METHOD_CALL: {
            // Check if this is actually a module function call (e.g., math_utils.square())
            if (e->as.method_call.object->kind == EX_VAR) {
                const char *var_name = e->as.method_call.object->as.var_name;
                if (is_imported_module(var_name)) {
                    // Transform into a regular function call with qualified name
                    const char *module_name = get_module_name(var_name);
                    const char *func_name = e->as.method_call.method_name;

                    // Create qualified name: module$function
                    size_t qname_len = strlen(module_name) + 1 + strlen(func_name) + 1;
                    char *qname = bp_xmalloc(qname_len);
                    snprintf(qname, qname_len, "%s$%s", module_name, func_name);

                    // Transform the expression to EX_CALL
                    e->kind = EX_CALL;
                    e->as.call.name = qname;
                    e->as.call.args = e->as.method_call.args;
                    e->as.call.argc = e->as.method_call.argc;
                    e->as.call.fn_index = -2;  // Special marker: cross-module call, resolve by name

                    // Check arguments
                    for (size_t i = 0; i < e->as.call.argc; i++) {
                        check_expr(e->as.call.args[i], s);
                    }

                    // Look up function signature in function table for type checking only
                    FnSig *sig = fntable_get(&g_fntable, qname);
                    if (sig) {
                        // Keep fn_index as -2 (don't use sig->fn_index)
                        // The global index will be resolved during multi-compile
                        e->inferred = sig->ret_type;
                    } else {
                        // Function from external module - type will be resolved during multi-compile
                        e->inferred = type_int();  // Default to int for now
                    }
                    return e->inferred;
                }
            }

            // Check object type
            Type obj_type = check_expr(e->as.method_call.object, s);
            if (obj_type.kind != TY_STRUCT) {
                bp_fatal("cannot call method '%s' on non-struct type %s",
                         e->as.method_call.method_name, type_name(obj_type));
            }
            // Look up struct to find method
            StructInfo *si = structtable_get(&g_structtable, obj_type.struct_name);
            if (!si) {
                bp_fatal("unknown struct type '%s'", obj_type.struct_name);
            }
            // For now, just check the args - method lookup will happen in compiler
            for (size_t i = 0; i < e->as.method_call.argc; i++) {
                check_expr(e->as.method_call.args[i], s);
            }
            // TODO: Look up method signature and return proper type
            // For now, return void - this needs full method table implementation
            e->inferred = type_void();
            return e->inferred;
        }
        case EX_ENUM_MEMBER: {
            // Look up enum and validate member
            EnumInfo *ei = enumtable_get(&g_enumtable, e->as.enum_member.enum_name);
            if (!ei) bp_fatal("unknown enum '%s'", e->as.enum_member.enum_name);
            bool found = false;
            for (size_t i = 0; i < ei->variant_count; i++) {
                if (strcmp(ei->variant_names[i], e->as.enum_member.member_name) == 0) {
                    e->as.enum_member.member_value = (int)ei->variant_values[i];
                    found = true;
                    break;
                }
            }
            if (!found) bp_fatal("enum '%s' has no member '%s'", ei->name, e->as.enum_member.member_name);
            e->inferred = type_int();
            return e->inferred;
        }
        case EX_NEW: {
            // Look up class definition
            ClassInfo *ci = classtable_get(&g_classtable, e->as.new_expr.class_name);
            if (!ci) {
                bp_fatal("unknown class type '%s'", e->as.new_expr.class_name);
            }
            // Check constructor arguments (if __init__ method exists)
            // For now, just type check all arguments
            for (size_t i = 0; i < e->as.new_expr.argc; i++) {
                check_expr(e->as.new_expr.args[i], s);
            }
            // Store class index for compiler
            e->as.new_expr.class_index = (int)ci->class_index;
            // Return the class type
            Type cls_type;
            cls_type.kind = TY_CLASS;
            cls_type.elem_type = NULL;
            cls_type.key_type = NULL;
            cls_type.struct_name = ci->name;
            cls_type.tuple_types = NULL;
            cls_type.tuple_len = 0;
            cls_type.param_types = NULL;
            cls_type.param_count = 0;
            cls_type.return_type = NULL;
            e->inferred = cls_type;
            return e->inferred;
        }
        case EX_SUPER_CALL: {
            // super() or super.method() calls
            // Type check all arguments
            for (size_t i = 0; i < e->as.super_call.argc; i++) {
                check_expr(e->as.super_call.args[i], s);
            }
            // For now, return void - proper method lookup will be added later
            e->inferred = type_void();
            return e->inferred;
        }
        default: break;
    }
    bp_fatal("unknown expr");
    return type_void();
}

static void check_stmt(Stmt *st, Scope *s, Type ret_type) {
    switch (st->kind) {
        case ST_LET: {
            Type it = check_expr(st->as.let.init, s);
            if (!type_eq(it, st->as.let.type)) {
                bp_fatal_at(st->line, "type error: let %s: %s initialized with %s", st->as.let.name, type_name(st->as.let.type), type_name(it));
            }
            // For TY_FUNC: propagate lambda function name from inferred type
            Type let_type = st->as.let.type;
            if (let_type.kind == TY_FUNC && it.struct_name) {
                let_type.struct_name = it.struct_name;
            }
            scope_put(s, st->as.let.name, let_type, st->line);
            return;
        }
        case ST_ASSIGN: {
            Type vt = type_void();
            if (!scope_get(s, st->as.assign.name, &vt)) bp_fatal_at(st->line, "undefined variable '%s'", st->as.assign.name);
            Type it = check_expr(st->as.assign.value, s);
            if (!type_eq(vt, it)) bp_fatal_at(st->line, "type error: assigning %s to %s", type_name(it), type_name(vt));
            return;
        }
        case ST_INDEX_ASSIGN: {
            Type container_type = check_expr(st->as.idx_assign.array, s);
            Type idx_type = check_expr(st->as.idx_assign.index, s);
            Type val_type = check_expr(st->as.idx_assign.value, s);

            if (container_type.kind == TY_ARRAY) {
                if (idx_type.kind != TY_INT) {
                    bp_fatal_at(st->line, "array index must be int, got %s", type_name(idx_type));
                }
                if (container_type.elem_type && !type_eq(val_type, *container_type.elem_type)) {
                    bp_fatal_at(st->line, "type error: assigning %s to array of %s",
                             type_name(val_type), type_name(*container_type.elem_type));
                }
            } else if (container_type.kind == TY_MAP) {
                if (container_type.key_type && !type_eq(idx_type, *container_type.key_type)) {
                    bp_fatal_at(st->line, "map key type mismatch: expected %s, got %s",
                             type_name(*container_type.key_type), type_name(idx_type));
                }
                if (container_type.elem_type && !type_eq(val_type, *container_type.elem_type)) {
                    bp_fatal_at(st->line, "type error: assigning %s to map with value type %s",
                             type_name(val_type), type_name(*container_type.elem_type));
                }
            } else {
                bp_fatal_at(st->line, "cannot index type for assignment: %s", type_name(container_type));
            }
            return;
        }
        case ST_EXPR:
            check_expr(st->as.expr.expr, s);
            return;
        case ST_IF: {
            if (check_expr(st->as.ifs.cond, s).kind != TY_BOOL) bp_fatal_at(st->line, "if condition must be bool");
            // Create new scope for then-block
            size_t then_mark = scope_mark(s);
            for (size_t i = 0; i < st->as.ifs.then_len; i++) check_stmt(st->as.ifs.then_stmts[i], s, ret_type);
            scope_restore(s, then_mark);
            // Create new scope for else-block
            size_t else_mark = scope_mark(s);
            for (size_t i = 0; i < st->as.ifs.else_len; i++) check_stmt(st->as.ifs.else_stmts[i], s, ret_type);
            scope_restore(s, else_mark);
            return;
        }
        case ST_WHILE: {
            if (check_expr(st->as.wh.cond, s).kind != TY_BOOL) bp_fatal_at(st->line, "while condition must be bool");
            // Create new scope for while body
            size_t mark = scope_mark(s);
            for (size_t i = 0; i < st->as.wh.body_len; i++) check_stmt(st->as.wh.body[i], s, ret_type);
            scope_restore(s, mark);
            return;
        }
        case ST_FOR: {
            if (check_expr(st->as.forr.start, s).kind != TY_INT) bp_fatal_at(st->line, "for range start must be int");
            if (check_expr(st->as.forr.end, s).kind != TY_INT) bp_fatal_at(st->line, "for range end must be int");
            // Create new scope for for loop (includes iterator)
            size_t mark = scope_mark(s);
            // Add iterator variable to this block's scope
            scope_put(s, st->as.forr.var, type_int(), st->line);
            for (size_t i = 0; i < st->as.forr.body_len; i++) check_stmt(st->as.forr.body[i], s, ret_type);
            scope_restore(s, mark);
            return;
        }
        case ST_FOR_IN: {
            Type coll_type = check_expr(st->as.for_in.collection, s);
            Type elem_type;
            if (coll_type.kind == TY_ARRAY) {
                elem_type = coll_type.elem_type ? *coll_type.elem_type : type_int();
            } else if (coll_type.kind == TY_MAP) {
                elem_type = coll_type.key_type ? *coll_type.key_type : type_str();
            } else {
                bp_fatal_at(st->line, "for-in requires array or map, got %s", type_name(coll_type));
            }
            size_t mark = scope_mark(s);
            scope_put(s, st->as.for_in.var, elem_type, st->line);
            for (size_t i = 0; i < st->as.for_in.body_len; i++) check_stmt(st->as.for_in.body[i], s, ret_type);
            scope_restore(s, mark);
            return;
        }
        case ST_BREAK:
        case ST_CONTINUE:
            // break/continue are always valid within loops - we'll check at runtime
            return;
        case ST_RETURN:
            if (ret_type.kind == TY_VOID) {
                if (st->as.ret.value) bp_fatal_at(st->line, "return value in void function");
                return;
            }
            if (!st->as.ret.value) bp_fatal_at(st->line, "missing return value");
            if (!type_eq(check_expr(st->as.ret.value, s), ret_type)) bp_fatal_at(st->line, "return type mismatch");
            return;
        case ST_TRY: {
            // Create new scope for try body
            size_t try_mark = scope_mark(s);
            for (size_t i = 0; i < st->as.try_catch.try_len; i++) {
                check_stmt(st->as.try_catch.try_stmts[i], s, ret_type);
            }
            scope_restore(s, try_mark);

            // Create new scope for catch body (includes catch variable)
            size_t catch_mark = scope_mark(s);
            if (st->as.try_catch.catch_var) {
                scope_put(s, st->as.try_catch.catch_var, type_str(), st->line);  // Exceptions are strings
            }
            for (size_t i = 0; i < st->as.try_catch.catch_len; i++) {
                check_stmt(st->as.try_catch.catch_stmts[i], s, ret_type);
            }
            scope_restore(s, catch_mark);

            // Create new scope for finally body
            size_t finally_mark = scope_mark(s);
            for (size_t i = 0; i < st->as.try_catch.finally_len; i++) {
                check_stmt(st->as.try_catch.finally_stmts[i], s, ret_type);
            }
            scope_restore(s, finally_mark);
            return;
        }
        case ST_THROW: {
            // For now, thrown value must be a string
            Type t = check_expr(st->as.throw.value, s);
            if (t.kind != TY_STR) {
                bp_fatal_at(st->line, "throw value must be string, got %s", type_name(t));
            }
            return;
        }
        case ST_MATCH: {
            check_expr(st->as.match.expr, s);
            for (size_t i = 0; i < st->as.match.case_count; i++) {
                if (st->as.match.case_values[i]) {
                    check_expr(st->as.match.case_values[i], s);
                }
                size_t mark = scope_mark(s);
                for (size_t j = 0; j < st->as.match.case_body_lens[i]; j++) {
                    check_stmt(st->as.match.case_bodies[i][j], s, ret_type);
                }
                scope_restore(s, mark);
            }
            return;
        }
        case ST_FIELD_ASSIGN: {
            // Type check the field access (object.field)
            Expr *fa = st->as.field_assign.object;
            if (fa->kind != EX_FIELD_ACCESS) bp_fatal("invalid field assignment target");
            Type obj_type = check_expr(fa, s);
            Type val_type = check_expr(st->as.field_assign.value, s);
            if (!type_eq(obj_type, val_type)) {
                bp_fatal("type error: assigning %s to field of type %s",
                         type_name(val_type), type_name(obj_type));
            }
            return;
        }
        default: break;
    }
    bp_fatal("unknown stmt");
}

void typecheck_module(Module *m) {
    // Build function table first (for forward references and recursion)
    g_module = m;

    // When preserving external functions (multi-module mode), don't clear the function table
    // External functions were pre-registered with qualified names
    if (!g_preserve_external_functions) {
        fntable_free(&g_fntable);  // Clean any previous state
    }
    structtable_free(&g_structtable);  // Clean any previous state
    classtable_free(&g_classtable);  // Clean any previous state
    enumtable_free(&g_enumtable);  // Clean any previous state

    // Clear and rebuild import table
    for (size_t i = 0; i < g_import_count; i++) {
        free(g_imports[i].module_name);
        free(g_imports[i].alias);
        for (size_t j = 0; j < g_imports[i].import_count; j++) {
            free(g_imports[i].import_names[j]);
        }
        free(g_imports[i].import_names);
    }
    free(g_imports);
    g_imports = NULL;
    g_import_count = 0;
    g_import_cap = 0;

    // Load imports from module
    for (size_t i = 0; i < m->importc; i++) {
        ImportDef *id = &m->imports[i];
        if (g_import_count + 1 > g_import_cap) {
            g_import_cap = g_import_cap ? g_import_cap * 2 : 8;
            g_imports = bp_xrealloc(g_imports, g_import_cap * sizeof(*g_imports));
        }
        ImportInfo *ii = &g_imports[g_import_count++];
        ii->module_name = bp_xstrdup(id->module_name);
        ii->alias = id->alias ? bp_xstrdup(id->alias) : NULL;
        ii->import_count = id->import_count;
        if (id->import_count > 0) {
            ii->import_names = bp_xmalloc(id->import_count * sizeof(char *));
            for (size_t j = 0; j < id->import_count; j++) {
                ii->import_names[j] = bp_xstrdup(id->import_names[j]);
            }
        } else {
            ii->import_names = NULL;
        }
    }

    // Build struct table first (structs must be defined before use)
    for (size_t i = 0; i < m->structc; i++) {
        StructDef *sd = &m->structs[i];
        // Note: we don't copy field names/types, just reference them
        structtable_add(&g_structtable, sd->name, sd->field_names, sd->field_types, sd->field_count, i);
    }

    // Build enum table
    for (size_t i = 0; i < m->enumc; i++) {
        EnumDef *ed = &m->enums[i];
        enumtable_add(&g_enumtable, ed->name, ed->variant_names, ed->variant_values, ed->variant_count);
    }

    // Build class table (classes must be defined before use)
    for (size_t i = 0; i < m->classc; i++) {
        ClassDef *cd = &m->classes[i];
        classtable_add(&g_classtable, cd->name, cd->parent_name, cd->field_names, cd->field_types, cd->field_count, i);
    }

    for (size_t i = 0; i < m->fnc; i++) {
        Function *f = &m->fns[i];

        // Copy parameter types
        Type *param_types = NULL;
        if (f->paramc > 0) {
            param_types = bp_xmalloc(f->paramc * sizeof(Type));
            for (size_t p = 0; p < f->paramc; p++) {
                param_types[p] = f->params[p].type;
            }
        }

        fntable_add(&g_fntable, f->name, param_types, f->paramc, f->ret_type, i);
    }

    // Register extern functions (FFI) so calls to them are recognized
    for (size_t i = 0; i < m->externc; i++) {
        ExternDef *ed = &m->externs[i];
        Type *param_types = NULL;
        if (ed->paramc > 0) {
            param_types = bp_xmalloc(ed->paramc * sizeof(Type));
            for (size_t p = 0; p < ed->paramc; p++) {
                param_types[p] = ed->params[p].type;
            }
        }
        // Use fn_index = -(100 + i) to encode as FFI call
        fntable_add(&g_fntable, ed->name, param_types, ed->paramc, ed->ret_type, (size_t)(int)(-(100 + (int)i)));
    }

    // Initialize lambda counter - lambdas get indices after functions + methods
    g_lambda_counter = 0;
    {
        size_t total_methods = 0;
        for (size_t i = 0; i < m->classc; i++)
            total_methods += m->classes[i].method_count;
        g_lambda_base_index = m->fnc + total_methods;
    }

    // Type check global variables
    {
        Scope scope = {0};
        for (size_t i = 0; i < m->global_varc; i++) {
            check_stmt(m->global_vars[i], &scope, type_void());
        }
        for (size_t k = 0; k < scope.len; k++) free(scope.items[k].name);
        free(scope.items);
    }

    // Now type-check all function bodies
    for (size_t i = 0; i < m->fnc; i++) {
        Function *f = &m->fns[i];

        Scope s = {0};

        // Add global variables to scope so functions can reference them
        for (size_t g = 0; g < m->global_varc; g++) {
            if (m->global_vars[g]->kind == ST_LET) {
                scope_put(&s, m->global_vars[g]->as.let.name,
                          m->global_vars[g]->as.let.init->inferred,
                          m->global_vars[g]->line);
            }
        }

        for (size_t p = 0; p < f->paramc; p++) scope_put(&s, f->params[p].name, f->params[p].type, f->line);

        for (size_t j = 0; j < f->body_len; j++) check_stmt(f->body[j], &s, f->ret_type);

        for (size_t k = 0; k < s.len; k++) free(s.items[k].name);
        free(s.items);
    }

    // Keep function/struct tables around for compiler (don't free yet)
}

size_t typecheck_lambda_count(void) {
    return g_lambda_counter;
}
