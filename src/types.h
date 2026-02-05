#pragma once
#include "ast.h"
#include <stdbool.h>

void typecheck_module(Module *m);

// Multi-module support: register external function signatures before type-checking
// This allows cross-module calls to be type-checked correctly
void types_register_function(const char *name, Type *param_types, size_t param_count,
                             Type ret_type, size_t fn_index);

// Clear all registered external functions (call before registering new set)
void types_clear_external_functions(void);

// Set flag to preserve pre-registered functions during typecheck_module
// When true, typecheck_module will ADD to the function table instead of replacing it
void types_set_preserve_external(bool preserve);
