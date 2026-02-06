/*
 * Module Resolver for BetterPython
 * Handles module discovery, dependency graph construction, and cycle detection.
 */

#include "module_resolver.h"
#include "parser.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>

// Read file content
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }

    char *buf = bp_xmalloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

// Check if file exists
static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// Check if directory exists
static bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

char *path_dirname(const char *path) {
    if (!path) return bp_xstrdup(".");

    char *copy = bp_xstrdup(path);
    char *dir = dirname(copy);
    char *result = bp_xstrdup(dir);
    free(copy);
    return result;
}

char *path_join(const char *dir, const char *file) {
    if (!dir || strlen(dir) == 0) return bp_xstrdup(file);
    if (!file || strlen(file) == 0) return bp_xstrdup(dir);

    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    bool need_slash = (dir[dir_len - 1] != '/');

    size_t total = dir_len + (need_slash ? 1 : 0) + file_len + 1;
    char *result = bp_xmalloc(total);

    strcpy(result, dir);
    if (need_slash) strcat(result, "/");
    strcat(result, file);

    return result;
}

char *path_normalize(const char *path) {
    if (!path) return bp_xstrdup(".");

    // Use realpath if file exists
    char resolved[4096];
    if (realpath(path, resolved)) {
        return bp_xstrdup(resolved);
    }

    // Otherwise, do basic normalization
    char *result = bp_xstrdup(path);
    size_t len = strlen(result);

    // Remove trailing slashes (except root)
    while (len > 1 && result[len - 1] == '/') {
        result[--len] = '\0';
    }

    return result;
}

void module_graph_init(ModuleGraph *g) {
    memset(g, 0, sizeof(*g));

    // Add default search paths
    // 1. Current directory (added during resolution based on entry file)
    // 2. BETTERPYTHON_PATH environment variable
    const char *bp_path = getenv("BETTERPYTHON_PATH");
    if (bp_path) {
        // Parse colon-separated paths
        char *copy = bp_xstrdup(bp_path);
        char *tok = strtok(copy, ":");
        while (tok && g->search_path_count < MAX_SEARCH_PATHS) {
            g->search_paths[g->search_path_count++] = bp_xstrdup(tok);
            tok = strtok(NULL, ":");
        }
        free(copy);
    }

    // 3. Standard library path (if it exists)
    const char *stdlib_paths[] = {
        "/usr/local/lib/betterpython/stdlib",
        "/usr/lib/betterpython/stdlib",
        "./stdlib"
    };
    for (size_t i = 0; i < sizeof(stdlib_paths) / sizeof(stdlib_paths[0]); i++) {
        if (dir_exists(stdlib_paths[i]) && g->search_path_count < MAX_SEARCH_PATHS) {
            g->search_paths[g->search_path_count++] = bp_xstrdup(stdlib_paths[i]);
        }
    }

    // 4. Package manager directory (bppkg installs packages here)
    if (dir_exists("./packages") && g->search_path_count < MAX_SEARCH_PATHS) {
        g->search_paths[g->search_path_count++] = bp_xstrdup("./packages");
    }
}

void module_graph_add_search_path(ModuleGraph *g, const char *path) {
    if (g->search_path_count >= MAX_SEARCH_PATHS) return;
    g->search_paths[g->search_path_count++] = bp_xstrdup(path);
}

// Convert module name to file path (e.g., "utils/math" -> "utils/math.bp")
static char *module_name_to_file(const char *name) {
    size_t len = strlen(name);
    char *file = bp_xmalloc(len + 4);  // +4 for ".bp\0"
    strcpy(file, name);
    strcat(file, ".bp");
    return file;
}

// Try to find module file
static char *find_module_file(ModuleGraph *g, const char *name, const char *from_dir) {
    char *file = module_name_to_file(name);
    char *path;

    // 1. Try relative to current module's directory
    if (from_dir) {
        path = path_join(from_dir, file);
        if (file_exists(path)) {
            free(file);
            return path_normalize(path);
        }
        free(path);
    }

    // 2. Try search paths
    for (size_t i = 0; i < g->search_path_count; i++) {
        path = path_join(g->search_paths[i], file);
        if (file_exists(path)) {
            free(file);
            return path_normalize(path);
        }
        free(path);
    }

    free(file);
    return NULL;
}

// Add new module to graph
static size_t add_module(ModuleGraph *g, const char *name, const char *path) {
    if (g->count + 1 > g->cap) {
        g->cap = g->cap ? g->cap * 2 : 8;
        g->modules = bp_xrealloc(g->modules, g->cap * sizeof(*g->modules));
    }

    ModuleInfo *m = &g->modules[g->count];
    memset(m, 0, sizeof(*m));
    m->name = bp_xstrdup(name);
    m->path = bp_xstrdup(path);
    m->parsed = false;
    m->compiled = false;

    return g->count++;
}

// Add dependency to module
static void add_dependency(ModuleInfo *m, size_t dep_idx) {
    // Check if already exists
    for (size_t i = 0; i < m->dep_count; i++) {
        if (m->deps[i] == dep_idx) return;
    }

    if (m->dep_count + 1 > m->dep_cap) {
        m->dep_cap = m->dep_cap ? m->dep_cap * 2 : 4;
        m->deps = bp_xrealloc(m->deps, m->dep_cap * sizeof(*m->deps));
    }
    m->deps[m->dep_count++] = dep_idx;
}

int module_graph_find(ModuleGraph *g, const char *name) {
    for (size_t i = 0; i < g->count; i++) {
        if (strcmp(g->modules[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Cycle detection during resolution
typedef struct {
    size_t *stack;
    size_t len;
    size_t cap;
} VisitStack;

static bool is_in_stack(VisitStack *vs, size_t idx) {
    for (size_t i = 0; i < vs->len; i++) {
        if (vs->stack[i] == idx) return true;
    }
    return false;
}

static void push_stack(VisitStack *vs, size_t idx) {
    if (vs->len + 1 > vs->cap) {
        vs->cap = vs->cap ? vs->cap * 2 : 8;
        vs->stack = bp_xrealloc(vs->stack, vs->cap * sizeof(*vs->stack));
    }
    vs->stack[vs->len++] = idx;
}

static void pop_stack(VisitStack *vs) {
    if (vs->len > 0) vs->len--;
}

// Resolve module recursively
static int resolve_module_internal(ModuleGraph *g, const char *name, const char *from_dir, VisitStack *vs) {
    // Check if already resolved
    int existing = module_graph_find(g, name);
    if (existing >= 0) {
        // Check for circular dependency
        if (is_in_stack(vs, (size_t)existing) && !g->modules[existing].parsed) {
            fprintf(stderr, "error: circular dependency detected: %s\n", name);
            return -1;
        }
        return existing;
    }

    // Find module file
    char *path = find_module_file(g, name, from_dir);
    if (!path) {
        fprintf(stderr, "error: cannot find module '%s'\n", name);
        return -1;
    }

    // Add module to graph
    size_t idx = add_module(g, name, path);
    ModuleInfo *m = &g->modules[idx];

    // Push to visit stack for cycle detection
    push_stack(vs, idx);

    // Read and parse module
    m->source = read_file(path);
    if (!m->source) {
        fprintf(stderr, "error: cannot read module '%s' at %s\n", name, path);
        free(path);
        pop_stack(vs);
        return -1;
    }

    // Parse the module
    m->ast = parse_module(m->source);
    m->parsed = true;

    // Get directory for relative imports
    char *mod_dir = path_dirname(path);

    // Resolve all imports
    for (size_t i = 0; i < m->ast.importc; i++) {
        const char *import_name = m->ast.imports[i].module_name;
        int dep_idx = resolve_module_internal(g, import_name, mod_dir, vs);
        if (dep_idx < 0) {
            free(mod_dir);
            free(path);
            pop_stack(vs);
            return -1;
        }
        add_dependency(m, (size_t)dep_idx);
    }

    free(mod_dir);
    free(path);
    pop_stack(vs);
    return (int)idx;
}

int module_graph_resolve(ModuleGraph *g, const char *name, const char *from_dir) {
    VisitStack vs = {0};
    int result = resolve_module_internal(g, name, from_dir, &vs);
    free(vs.stack);
    return result;
}

bool module_graph_resolve_all(ModuleGraph *g, const char *entry_path) {
    // Normalize entry path
    char *norm_path = path_normalize(entry_path);
    if (!norm_path || !file_exists(norm_path)) {
        fprintf(stderr, "error: cannot find entry file '%s'\n", entry_path);
        free(norm_path);
        return false;
    }

    // Get directory of entry file and add to search paths
    char *entry_dir = path_dirname(norm_path);
    module_graph_add_search_path(g, entry_dir);

    // Add entry module with special name "__main__"
    size_t idx = add_module(g, "__main__", norm_path);
    g->entry_idx = idx;

    ModuleInfo *m = &g->modules[idx];

    // Read and parse entry module
    m->source = read_file(norm_path);
    if (!m->source) {
        fprintf(stderr, "error: cannot read entry file '%s'\n", entry_path);
        free(entry_dir);
        free(norm_path);
        return false;
    }

    m->ast = parse_module(m->source);
    m->parsed = true;

    // Resolve all imports
    VisitStack vs = {0};
    push_stack(&vs, idx);

    for (size_t i = 0; i < m->ast.importc; i++) {
        const char *import_name = m->ast.imports[i].module_name;
        int dep_idx = resolve_module_internal(g, import_name, entry_dir, &vs);
        if (dep_idx < 0) {
            free(vs.stack);
            free(entry_dir);
            free(norm_path);
            return false;
        }
        add_dependency(m, (size_t)dep_idx);
    }

    free(vs.stack);
    free(entry_dir);
    free(norm_path);
    return true;
}

// Topological sort using DFS
typedef struct {
    bool *visited;
    bool *in_stack;  // For cycle detection
    size_t *result;
    size_t result_len;
    size_t result_cap;
    bool has_cycle;
} TopoState;

static void topo_visit(ModuleGraph *g, size_t idx, TopoState *state) {
    if (state->has_cycle) return;

    if (state->in_stack[idx]) {
        state->has_cycle = true;
        return;
    }

    if (state->visited[idx]) return;

    state->in_stack[idx] = true;

    // Visit dependencies first
    ModuleInfo *m = &g->modules[idx];
    for (size_t i = 0; i < m->dep_count; i++) {
        topo_visit(g, m->deps[i], state);
    }

    state->visited[idx] = true;
    state->in_stack[idx] = false;

    // Add to result
    if (state->result_len + 1 > state->result_cap) {
        state->result_cap = state->result_cap ? state->result_cap * 2 : 8;
        state->result = bp_xrealloc(state->result, state->result_cap * sizeof(*state->result));
    }
    state->result[state->result_len++] = idx;
}

size_t *module_graph_topo_sort(ModuleGraph *g, size_t *out_count) {
    if (g->count == 0) {
        *out_count = 0;
        return NULL;
    }

    TopoState state = {0};
    state.visited = bp_xmalloc(g->count * sizeof(bool));
    state.in_stack = bp_xmalloc(g->count * sizeof(bool));
    memset(state.visited, 0, g->count * sizeof(bool));
    memset(state.in_stack, 0, g->count * sizeof(bool));

    // Visit from entry module
    topo_visit(g, g->entry_idx, &state);

    free(state.visited);
    free(state.in_stack);

    if (state.has_cycle) {
        free(state.result);
        *out_count = 0;
        return NULL;
    }

    *out_count = state.result_len;
    return state.result;
}

ModuleInfo *module_graph_get(ModuleGraph *g, size_t idx) {
    if (idx >= g->count) return NULL;
    return &g->modules[idx];
}

void module_graph_free(ModuleGraph *g) {
    for (size_t i = 0; i < g->count; i++) {
        ModuleInfo *m = &g->modules[i];
        free(m->name);
        free(m->path);
        free(m->source);
        free(m->deps);
        if (m->parsed) {
            module_free(&m->ast);
        }
    }
    free(g->modules);

    for (size_t i = 0; i < g->search_path_count; i++) {
        free(g->search_paths[i]);
    }

    memset(g, 0, sizeof(*g));
}
