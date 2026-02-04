#pragma once
#include "common.h"
#include "bytecode.h"
#include "gc.h"

Value stdlib_call(BuiltinId id, Value *args, uint16_t argc, Gc *gc, int *exit_code, bool *exiting);
