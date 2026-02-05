# BetterPython Performance Optimizations

## The Singularity - v1.0.0

This document describes the performance optimizations implemented in the BetterPython virtual machine.

## Implemented Optimizations

### 1. Computed Goto Dispatch (v1.0.0)

**Status:** Implemented
**Performance Gain:** 15-25% faster opcode dispatch

The VM uses GCC/Clang's computed goto extension (`&&label` and `goto *ptr`) for opcode dispatch instead of a traditional switch statement. This eliminates branch misprediction overhead because each opcode handler directly jumps to the next handler.

**How it works:**
```c
// Dispatch table maps opcodes to handler labels
static const void *dispatch_table[256] = {
    [OP_ADD_I64] = &&L_OP_ADD_I64,
    [OP_SUB_I64] = &&L_OP_SUB_I64,
    // ...
};

// Direct jump to handler
goto *dispatch_table[opcode];
```

**Compatibility:**
- GCC and Clang: Uses computed goto (fast path)
- Other compilers: Falls back to switch statement (compatible path)

The optimization is automatically selected at compile time using `#if defined(__GNUC__) || defined(__clang__)`.

### 2. Branch Prediction Hints

**Status:** Implemented
**Performance Gain:** 5-10% for conditional branches

The VM uses `LIKELY()` and `UNLIKELY()` macros (`__builtin_expect`) to hint the compiler about branch probabilities:

```c
if (UNLIKELY(vm->exiting)) goto vm_exit;
if (UNLIKELY(ip >= fn->code_len)) goto vm_end_of_function;
```

This helps the CPU's branch predictor and reduces pipeline stalls.

### 3. Inline Stack Operations

**Status:** Implemented
**Performance Gain:** 5-10% for stack-heavy code

Stack push/pop operations use inline macros instead of function calls in the computed goto path:

```c
// Inline macros - no function call overhead
#define PUSH(v) (vm->stack[vm->sp++] = (v))
#define POP() (vm->stack[--vm->sp])
```

Safety is maintained because:
- Bytecode is validated at compile time
- Stack depth is predictable for valid programs
- Bounds checking remains in the switch fallback path

### 4. Inline Caching (v1.0.0)

**Status:** Implemented
**Performance Gain:** 10-20% for call-heavy code

The VM uses a monomorphic inline cache to speed up repeated function calls at the same call site:

```c
// Inline cache structure
typedef struct {
    const uint8_t *code_base;  // Code array this entry belongs to
    size_t call_site_ip;       // Bytecode offset of call instruction
    uint32_t fn_idx;           // Expected function index
    BpFunc *fn_ptr;            // Cached function pointer
} InlineCacheEntry;

// On function call, check cache first
if (cache_hit) {
    callee = cached_fn_ptr;  // Skip bounds check + array lookup
} else {
    callee = &mod->funcs[fn_idx];  // Normal lookup
    update_cache();
}
```

**How it works:**
- 256-slot hash table keyed by (code_base, call_site_ip)
- On first call at a site, cache the function pointer
- On subsequent calls, if cache hit, use cached pointer directly
- Avoids bounds checking and array indexing on cache hits

**Benefits:**
- Eliminates bounds check for cached call sites
- Avoids `vm->mod.funcs[fn_idx]` array lookup
- Cache-friendly: hot call sites stay in L1 cache
- Foundation for future polymorphic inline caching

## Planned Optimizations

### 5. Register-Based VM

**Status:** Planned for v2.0
**Expected Gain:** 20-40% overall performance

Convert from stack-based to register-based architecture:
- Reduces memory traffic (fewer push/pop operations)
- Better CPU cache utilization
- Enables more efficient instruction encoding

### 6. Polymorphic Inline Caching

**Status:** Planned for v2.0
**Expected Gain:** 2-5x faster method dispatch

Extend the monomorphic cache to handle polymorphic call sites:
- Polymorphic inline caching for 2-4 types
- Megamorphic fallback for highly polymorphic sites
- Full method dispatch caching when classes are fully implemented

### 7. JIT Compilation

**Status:** Planned for v2.0
**Expected Gain:** 5-50x for hot functions

Compile frequently-executed bytecode to native machine code:
- Profile-guided optimization
- LLVM backend for portable code generation
- Fallback to interpreter for cold code

### 8. Multithreading

**Status:** Planned for v2.0
**Expected Gain:** Linear scaling on multicore

Add parallel execution support:
- Thread-safe GC with read barriers
- Lock-free data structures where possible
- Work-stealing scheduler for load balancing

## Benchmarks

### Micro-benchmarks (i7-12700K, Ubuntu 22.04)

| Operation | v0.9 (switch) | v1.0 (optimized) | Improvement |
|-----------|---------------|------------------|-------------|
| Empty loop (1M iter) | 45ms | 38ms | 18% faster |
| Integer arithmetic (1M ops) | 52ms | 42ms | 24% faster |
| Function calls (100K) | 78ms | 58ms | 26% faster |
| Recursive calls (10K depth) | 125ms | 95ms | 24% faster |
| String concat (100K) | 180ms | 175ms | 3% faster |

**Note:** Function call improvements include inline caching benefits (10-20% improvement for repeated call sites).

### Comparison with Other Languages

BetterPython v1.0 is approximately:
- 10-50x slower than native C (expected for interpreted language)
- 2-3x slower than LuaJIT (with JIT disabled)
- Comparable to CPython 3.11
- 2-5x faster than Ruby 2.7

With planned JIT compilation, we expect to close the gap significantly.

## Building with Optimizations

The default build enables all available optimizations:

```bash
make              # Optimized build (-O2)
make DEBUG=1      # Debug build (no optimizations)
```

## Architecture Notes

### Stack vs Register VM Trade-offs

**Stack-based (current):**
- Simpler bytecode generation
- More compact bytecode
- Higher memory traffic
- Branch-heavy dispatch

**Register-based (planned):**
- More complex bytecode generation
- Larger bytecode size
- Lower memory traffic
- Better for JIT compilation

### Memory Layout

The VM is designed for cache efficiency:
- Hot data (ip, sp, stack top) kept in registers when possible
- Locals array is contiguous for cache-friendly access
- Bytecode is read sequentially (prefetcher-friendly)

## Contributing

Performance improvements are welcome. When submitting optimizations:
1. Include before/after benchmarks
2. Test on multiple platforms
3. Ensure no regression in test suite
4. Document the optimization in this file
