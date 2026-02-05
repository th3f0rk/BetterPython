/*
 * BetterPython Register-Based Virtual Machine
 * Executes register-based bytecode (BPR1 format)
 */

#pragma once
#include "vm.h"

// Run register-based bytecode
// Returns exit code
int reg_vm_run(Vm *vm);
