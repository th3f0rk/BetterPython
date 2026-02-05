#pragma once
#include "bytecode.h"
#include "common.h"
#include "gc.h"

// Inline cache entry for call site caching (10-20% faster call-heavy code)
// Monomorphic cache: stores last function called at each call site
#define IC_CACHE_SIZE 256  // Must be power of 2
#define IC_HASH(code, ip) ((((size_t)(code) >> 3) ^ (ip)) & (IC_CACHE_SIZE - 1))

typedef struct {
    const uint8_t *code_base;  // Code array this cache entry belongs to
    size_t call_site_ip;       // Bytecode offset of call instruction
    uint32_t fn_idx;           // Expected function index
    BpFunc *fn_ptr;            // Cached function pointer (avoids array lookup)
} InlineCacheEntry;

typedef struct {
    BpModule mod;
    Gc gc;

    Value stack[4096];
    size_t sp;

    Value *locals;
    size_t localc;

    int exit_code;
    bool exiting;

    // Inline cache for function calls (initialized on first run)
    InlineCacheEntry *ic_cache;
} Vm;

void vm_init(Vm *vm, BpModule mod);
void vm_free(Vm *vm);

int vm_run(Vm *vm);
