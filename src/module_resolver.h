#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "ast.h"

// Maximum search paths
#define MAX_SEARCH_PATHS 16

// Module info structure
typedef struct {
    char *name;              // Module name (e.g., "math", "utils/helper")
    char *path;              // Absolute path to .bp file
    char *source;            // Source code content
    Module ast;              // Parsed AST
    bool parsed;             // Has been parsed
    bool compiled;           // Has been compiled
    size_t *deps;            // Indices of dependent modules
    size_t dep_count;
    size_t dep_cap;
} ModuleInfo;

// Module graph for tracking all modules
typedef struct {
    ModuleInfo *modules;
    size_t count;
    size_t cap;

    // Search paths for modules
    char *search_paths[MAX_SEARCH_PATHS];
    size_t search_path_count;

    // Entry module index
    size_t entry_idx;
} ModuleGraph;

// Initialize module graph with search paths
void module_graph_init(ModuleGraph *g);

// Add search path for module resolution
void module_graph_add_search_path(ModuleGraph *g, const char *path);

// Resolve a module by name, returns module index or -1 if not found
// This will recursively resolve all dependencies
int module_graph_resolve(ModuleGraph *g, const char *name, const char *from_dir);

// Resolve all modules starting from entry file
// Returns true on success, false on error (circular dependency, missing module)
bool module_graph_resolve_all(ModuleGraph *g, const char *entry_path);

// Get topologically sorted order for compilation
// Returns array of module indices in compile order (caller must free)
size_t *module_graph_topo_sort(ModuleGraph *g, size_t *out_count);

// Get module info by index
ModuleInfo *module_graph_get(ModuleGraph *g, size_t idx);

// Find module by name
int module_graph_find(ModuleGraph *g, const char *name);

// Free module graph
void module_graph_free(ModuleGraph *g);

// Utility: get directory from file path
char *path_dirname(const char *path);

// Utility: join path components
char *path_join(const char *dir, const char *file);

// Utility: normalize path (resolve .., remove double slashes)
char *path_normalize(const char *path);
