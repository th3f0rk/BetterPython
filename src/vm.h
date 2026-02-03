#pragma once
#include "bytecode.h"
#include "common.h"
#include "gc.h"

typedef struct {
    BpModule mod;
    Gc gc;

    Value stack[4096];
    size_t sp;

    Value *locals;
    size_t localc;

    int exit_code;
    bool exiting;
} Vm;

void vm_init(Vm *vm, BpModule mod);
void vm_free(Vm *vm);

int vm_run(Vm *vm);
