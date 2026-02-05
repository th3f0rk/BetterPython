/*
 * BetterPython JIT Compiler
 * Just-In-Time compilation for hot functions
 */

#pragma once
#include "../bytecode.h"
#include "../common.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// JIT compilation thresholds
#define JIT_FUNC_THRESHOLD    100    // Compile after 100 calls
#define JIT_LOOP_THRESHOLD    1000   // Hot loop threshold
#define JIT_OSR_THRESHOLD     5000   // On-stack replacement

// JIT state values
typedef enum {
    JIT_STATE_COLD = 0,         // Not profiled enough
    JIT_STATE_WARM = 1,         // Approaching threshold
    JIT_STATE_HOT = 2,          // Ready to compile
    JIT_STATE_COMPILING = 3,    // Being compiled
    JIT_STATE_COMPILED = 4,     // Native code ready
    JIT_STATE_FAILED = 5        // Compilation failed
} JitState;

// Profile data for a function
typedef struct {
    uint32_t call_count;        // Invocation count
    uint32_t *loop_counts;      // Per-loop iteration counts
    size_t loop_count_len;      // Number of loops
    JitState state;             // Current JIT state
    void *native_code;          // Compiled native code
    size_t native_size;         // Size of native code
} JitProfile;

// JIT context (attached to VM)
typedef struct {
    JitProfile *profiles;       // Profile data per function
    size_t profile_count;       // Number of functions

    // Code cache
    uint8_t *code_cache;        // Executable memory pool
    size_t cache_size;          // Total cache size
    size_t cache_used;          // Bytes used

    // Statistics
    uint64_t total_compilations;
    uint64_t total_executions;
    uint64_t native_executions;
    uint64_t interp_executions;

    bool enabled;               // JIT enabled flag
    bool debug;                 // Debug output
} JitContext;

// Forward declaration
typedef struct Vm Vm;

// Initialize JIT subsystem
void jit_init(JitContext *jit, size_t func_count);

// Shutdown JIT subsystem
void jit_shutdown(JitContext *jit);

// Record function call (for profiling)
void jit_record_call(JitContext *jit, uint32_t func_idx);

// Check if function should be JIT-compiled
bool jit_should_compile(JitContext *jit, uint32_t func_idx);

// Check if function has native code ready
bool jit_has_native(JitContext *jit, uint32_t func_idx);

// Compile function to native code
bool jit_compile(JitContext *jit, BpModule *mod, uint32_t func_idx);

// Get native code pointer for function
void *jit_get_native(JitContext *jit, uint32_t func_idx);

// Execute JIT-compiled function
// Returns result in regs[0]
int jit_execute(JitContext *jit, BpFunc *func, Value *regs, size_t reg_count);

// Invalidate JIT-compiled code (for deoptimization)
void jit_invalidate(JitContext *jit, uint32_t func_idx);

// Print JIT statistics
void jit_print_stats(JitContext *jit);
