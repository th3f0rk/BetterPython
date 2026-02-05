/*
 * BetterPython Threading Support
 * Provides thread creation, synchronization primitives, and thread-safe operations
 */

#pragma once
#include "common.h"
#include "gc.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// Maximum number of concurrent threads
#define BP_MAX_THREADS 256

// Thread states
typedef enum {
    THREAD_CREATED = 0,     // Thread created but not started
    THREAD_RUNNING = 1,     // Thread is executing
    THREAD_BLOCKED = 2,     // Thread is waiting (on mutex, cond, etc.)
    THREAD_FINISHED = 3,    // Thread completed execution
    THREAD_DETACHED = 4     // Thread is detached (cannot join)
} BpThreadState;

// Forward declarations
struct Vm;
struct BpModule;

// Thread handle - represents a thread of execution
typedef struct BpThread {
    pthread_t handle;               // Native thread handle
    uint32_t id;                    // BetterPython thread ID (1-based)
    BpThreadState state;            // Current state
    bool joinable;                  // Can be joined

    Value result;                   // Return value from thread function

    // Thread function info
    uint32_t func_idx;              // Index of function to execute
    Value *args;                    // Arguments array
    size_t argc;                    // Argument count

    // Thread-local VM state
    Value *regs;                    // Register file for this thread
    size_t reg_cap;                 // Register capacity
    size_t reg_count;               // Registers in use

    // Reference to shared state
    struct Vm *vm;                  // Shared VM (module, GC)

    // Synchronization for thread state
    pthread_mutex_t state_lock;     // Protects state transitions
    pthread_cond_t done_cond;       // Signaled when thread completes
} BpThread;

// Mutex - mutual exclusion lock
typedef struct BpMutex {
    pthread_mutex_t lock;           // Native mutex
    uint32_t owner_thread;          // Thread ID holding lock (0 = unlocked)
    bool locked;                    // Is currently locked
} BpMutex;

// Condition variable - for thread signaling
typedef struct BpCondition {
    pthread_cond_t cond;            // Native condition variable
} BpCondition;

// Global threading context
typedef struct {
    // Thread registry
    BpThread *threads[BP_MAX_THREADS];
    atomic_uint thread_count;
    atomic_uint next_id;

    // Main thread ID
    pthread_t main_thread;
    uint32_t main_thread_id;

    // Global lock for thread registry
    pthread_mutex_t registry_lock;

    // GC synchronization
    pthread_mutex_t gc_lock;
    pthread_cond_t gc_start_cond;
    pthread_cond_t gc_done_cond;
    atomic_int threads_at_safepoint;
    atomic_bool gc_requested;
    atomic_bool gc_in_progress;

    // Global allocation lock for thread safety
    pthread_mutex_t alloc_lock;

    // Initialized flag
    bool initialized;
} ThreadContext;

// Global thread context
extern ThreadContext g_thread_ctx;

// ============================================================================
// Thread Context Management
// ============================================================================

// Initialize threading subsystem (call once at startup)
void bp_thread_init(void);

// Shutdown threading subsystem (call at exit)
void bp_thread_shutdown(void);

// Get current thread ID (returns 0 for main thread before init)
uint32_t bp_thread_current_id(void);

// ============================================================================
// Thread Creation and Management
// ============================================================================

// Create a new thread (does not start it yet)
// func_idx: index of function in module to execute
// args: array of arguments to pass
// argc: number of arguments
BpThread *bp_thread_create(struct Vm *vm, uint32_t func_idx, Value *args, size_t argc);

// Start a thread (must be called after create)
bool bp_thread_start(BpThread *thread);

// Wait for thread to complete and get result
// Returns the value returned by the thread function
Value bp_thread_join(BpThread *thread);

// Detach a thread (cannot join after this)
bool bp_thread_detach(BpThread *thread);

// Check if thread is still running
bool bp_thread_is_running(BpThread *thread);

// Yield execution to other threads
void bp_thread_yield(void);

// Sleep for specified milliseconds
void bp_thread_sleep(uint64_t ms);

// ============================================================================
// Mutex Operations
// ============================================================================

// Create a new mutex
BpMutex *bp_mutex_new(void);

// Free a mutex
void bp_mutex_free(BpMutex *mutex);

// Acquire the mutex (blocking)
void bp_mutex_lock(BpMutex *mutex);

// Try to acquire the mutex (non-blocking)
// Returns true if lock was acquired, false otherwise
bool bp_mutex_trylock(BpMutex *mutex);

// Release the mutex
void bp_mutex_unlock(BpMutex *mutex);

// ============================================================================
// Condition Variable Operations
// ============================================================================

// Create a new condition variable
BpCondition *bp_cond_new(void);

// Free a condition variable
void bp_cond_free(BpCondition *cond);

// Wait on condition (must hold associated mutex)
// Atomically releases mutex and waits, re-acquires on return
void bp_cond_wait(BpCondition *cond, BpMutex *mutex);

// Wait with timeout (milliseconds)
// Returns true if signaled, false if timeout
bool bp_cond_timedwait(BpCondition *cond, BpMutex *mutex, uint64_t timeout_ms);

// Signal one waiting thread
void bp_cond_signal(BpCondition *cond);

// Signal all waiting threads
void bp_cond_broadcast(BpCondition *cond);

// ============================================================================
// GC Integration
// ============================================================================

// Enter a safepoint (thread can be paused for GC here)
void bp_thread_safepoint(void);

// Request a garbage collection (will stop all threads)
void bp_thread_request_gc(Gc *gc, Value *roots, size_t root_count);

// Mark thread-local roots for GC
void bp_thread_mark_roots(BpThread *thread, Gc *gc);

// Thread-safe allocation wrapper
void *bp_thread_alloc(size_t size);
void bp_thread_free(void *ptr);
