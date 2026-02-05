#pragma once
#include "module_resolver.h"
#include "bytecode.h"

/*
 * Multi-module compilation for BetterPython
 *
 * Handles compilation of multiple modules with import/export relationships.
 * Creates a single merged bytecode module with qualified function names.
 */

// Symbol mapping entry - maps short names to qualified names
typedef struct {
    char *short_name;      // Original name (e.g., "factorial")
    char *qualified_name;  // Full name (e.g., "utils$factorial")
    char *module_name;     // Module it came from
    size_t fn_index;       // Index in merged function list
    bool is_exported;      // Is this symbol exported?
} SymbolEntry;

// Symbol table for multi-module compilation
typedef struct {
    SymbolEntry *entries;
    size_t count;
    size_t cap;
} SymbolTable;

// Compile all modules in the graph into a single bytecode module
// Returns true on success
bool multi_compile(ModuleGraph *g, BpModule *out);

// Free symbol table
void symbol_table_free(SymbolTable *st);
