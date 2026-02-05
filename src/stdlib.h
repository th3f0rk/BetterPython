#pragma once
#include "common.h"
#include "bytecode.h"
#include "gc.h"

// Set command line arguments for argv()/argc() builtins
void stdlib_set_args(int argc, char **argv);

Value stdlib_call(BuiltinId id, Value *args, uint16_t argc, Gc *gc, int *exit_code, bool *exiting);
