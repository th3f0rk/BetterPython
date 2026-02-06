#pragma once
#include "ast.h"
#include <stdbool.h>

// Transpile a type-checked Module AST to a C source file.
// Returns true on success, false on failure.
bool transpile_module_to_c(const Module *m, const char *output_path);
