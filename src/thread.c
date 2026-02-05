/*
 * BetterPython Threading Implementation
 * Uses POSIX threads (pthreads) for cross-platform threading
 */

#define _GNU_SOURCE
#include "thread.h"
#include "vm.h"
#include "bytecode.h"
#include "util.h"
#include "stdlib.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sched.h>

// Global thread context
ThreadContext g_thread_ctx = {0};

// Thread-local storage for current thread
static __thread BpThread *tl_current_thread = NULL;
static __thread uint32_t tl_thread_id = 0;

// ============================================================================
// Thread Context Management
// ============================================================================

void bp_thread_init(void) {
    if (g_thread_ctx.initialized) return;

    memset(&g_thread_ctx, 0, sizeof(g_thread_ctx));

    pthread_mutex_init(&g_thread_ctx.registry_lock, NULL);
    pthread_mutex_init(&g_thread_ctx.gc_lock, NULL);
    pthread_mutex_init(&g_thread_ctx.alloc_lock, NULL);
    pthread_cond_init(&g_thread_ctx.gc_start_cond, NULL);
    pthread_cond_init(&g_thread_ctx.gc_done_cond, NULL);

    atomic_store(&g_thread_ctx.thread_count, 0);
    atomic_store(&g_thread_ctx.next_id, 1);  // Start at 1, 0 is reserved
    atomic_store(&g_thread_ctx.threads_at_safepoint, 0);
    atomic_store(&g_thread_ctx.gc_requested, false);
    atomic_store(&g_thread_ctx.gc_in_progress, false);

    g_thread_ctx.main_thread = pthread_self();
    g_thread_ctx.main_thread_id = 0;  // Main thread is ID 0

    g_thread_ctx.initialized = true;
}

void bp_thread_shutdown(void) {
    if (!g_thread_ctx.initialized) return;

    // Wait for all threads to complete
    pthread_mutex_lock(&g_thread_ctx.registry_lock);
    for (int i = 0; i < BP_MAX_THREADS; i++) {
        BpThread *t = g_thread_ctx.threads[i];
        if (t && t->state == THREAD_RUNNING) {
            pthread_mutex_unlock(&g_thread_ctx.registry_lock);
            bp_thread_join(t);
            pthread_mutex_lock(&g_thread_ctx.registry_lock);
        }
    }
    pthread_mutex_unlock(&g_thread_ctx.registry_lock);

    pthread_mutex_destroy(&g_thread_ctx.registry_lock);
    pthread_mutex_destroy(&g_thread_ctx.gc_lock);
    pthread_mutex_destroy(&g_thread_ctx.alloc_lock);
    pthread_cond_destroy(&g_thread_ctx.gc_start_cond);
    pthread_cond_destroy(&g_thread_ctx.gc_done_cond);

    g_thread_ctx.initialized = false;
}

uint32_t bp_thread_current_id(void) {
    return tl_thread_id;
}

// Thread-safe allocation
void *bp_thread_alloc(size_t size) {
    pthread_mutex_lock(&g_thread_ctx.alloc_lock);
    void *ptr = malloc(size);
    pthread_mutex_unlock(&g_thread_ctx.alloc_lock);
    return ptr;
}

void bp_thread_free(void *ptr) {
    pthread_mutex_lock(&g_thread_ctx.alloc_lock);
    free(ptr);
    pthread_mutex_unlock(&g_thread_ctx.alloc_lock);
}

// Register thread in global registry
static bool register_thread(BpThread *thread) {
    pthread_mutex_lock(&g_thread_ctx.registry_lock);

    // Find empty slot
    for (int i = 0; i < BP_MAX_THREADS; i++) {
        if (g_thread_ctx.threads[i] == NULL) {
            g_thread_ctx.threads[i] = thread;
            atomic_fetch_add(&g_thread_ctx.thread_count, 1);
            pthread_mutex_unlock(&g_thread_ctx.registry_lock);
            return true;
        }
    }

    pthread_mutex_unlock(&g_thread_ctx.registry_lock);
    return false;  // No slots available
}

// Unregister thread from global registry
static void unregister_thread(BpThread *thread) {
    pthread_mutex_lock(&g_thread_ctx.registry_lock);

    for (int i = 0; i < BP_MAX_THREADS; i++) {
        if (g_thread_ctx.threads[i] == thread) {
            g_thread_ctx.threads[i] = NULL;
            atomic_fetch_sub(&g_thread_ctx.thread_count, 1);
            break;
        }
    }

    pthread_mutex_unlock(&g_thread_ctx.registry_lock);
}

// ============================================================================
// Thread Entry Point
// ============================================================================

// Thread wrapper function executed by pthread
static void *thread_entry(void *arg) {
    BpThread *thread = (BpThread *)arg;

    // Set thread-local data
    tl_current_thread = thread;
    tl_thread_id = thread->id;

    // Update state
    pthread_mutex_lock(&thread->state_lock);
    thread->state = THREAD_RUNNING;
    pthread_mutex_unlock(&thread->state_lock);

    // Get function to execute
    Vm *vm = thread->vm;
    if (thread->func_idx >= vm->mod.fn_len) {
        thread->result = v_null();
        goto thread_done;
    }

    BpFunc *func = &vm->mod.funcs[thread->func_idx];

    // Allocate registers for this thread
    size_t reg_count = func->reg_count > 0 ? func->reg_count : 256;
    thread->regs = bp_xmalloc(reg_count * sizeof(Value));
    thread->reg_cap = reg_count;
    thread->reg_count = func->reg_count;

    // Initialize registers with null
    for (size_t i = 0; i < reg_count; i++) {
        thread->regs[i] = v_null();
    }

    // Copy arguments to registers (r0, r1, r2, ...)
    for (size_t i = 0; i < thread->argc && i < reg_count; i++) {
        thread->regs[i] = thread->args[i];
    }

    // Execute the function based on bytecode format
    if (func->format == BC_FORMAT_REGISTER) {
        // Execute register-based bytecode
        const uint8_t *code = func->code;
        size_t ip = 0;

        #define REG(n) thread->regs[n]

        while (ip < func->code_len) {
            // Check for GC safepoint
            bp_thread_safepoint();

            uint8_t op = code[ip++];

            switch (op) {
                case R_CONST_I64: {
                    uint8_t dst = code[ip++];
                    int64_t val;
                    memcpy(&val, code + ip, 8);
                    ip += 8;
                    REG(dst) = v_int(val);
                    break;
                }
                case R_CONST_F64: {
                    uint8_t dst = code[ip++];
                    double val;
                    memcpy(&val, code + ip, 8);
                    ip += 8;
                    REG(dst) = v_float(val);
                    break;
                }
                case R_CONST_BOOL: {
                    uint8_t dst = code[ip++];
                    uint8_t val = code[ip++];
                    REG(dst) = v_bool(val != 0);
                    break;
                }
                case R_MOV: {
                    uint8_t dst = code[ip++];
                    uint8_t src = code[ip++];
                    REG(dst) = REG(src);
                    break;
                }
                case R_ADD_I64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_int(REG(s1).as.i + REG(s2).as.i);
                    break;
                }
                case R_SUB_I64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_int(REG(s1).as.i - REG(s2).as.i);
                    break;
                }
                case R_MUL_I64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_int(REG(s1).as.i * REG(s2).as.i);
                    break;
                }
                case R_DIV_I64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    if (REG(s2).as.i == 0) {
                        thread->result = v_null();
                        goto thread_done;
                    }
                    REG(dst) = v_int(REG(s1).as.i / REG(s2).as.i);
                    break;
                }
                case R_MOD_I64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    if (REG(s2).as.i == 0) {
                        thread->result = v_null();
                        goto thread_done;
                    }
                    REG(dst) = v_int(REG(s1).as.i % REG(s2).as.i);
                    break;
                }
                case R_ADD_F64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_float(REG(s1).as.f + REG(s2).as.f);
                    break;
                }
                case R_SUB_F64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_float(REG(s1).as.f - REG(s2).as.f);
                    break;
                }
                case R_MUL_F64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_float(REG(s1).as.f * REG(s2).as.f);
                    break;
                }
                case R_DIV_F64: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_float(REG(s1).as.f / REG(s2).as.f);
                    break;
                }
                case R_LT: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(REG(s1).as.i < REG(s2).as.i);
                    break;
                }
                case R_LTE: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(REG(s1).as.i <= REG(s2).as.i);
                    break;
                }
                case R_GT: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(REG(s1).as.i > REG(s2).as.i);
                    break;
                }
                case R_GTE: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(REG(s1).as.i >= REG(s2).as.i);
                    break;
                }
                case R_EQ: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    // Simple equality for integers
                    REG(dst) = v_bool(REG(s1).as.i == REG(s2).as.i);
                    break;
                }
                case R_NEQ: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(REG(s1).as.i != REG(s2).as.i);
                    break;
                }
                case R_NOT: {
                    uint8_t dst = code[ip++];
                    uint8_t src = code[ip++];
                    REG(dst) = v_bool(!v_is_truthy(REG(src)));
                    break;
                }
                case R_AND: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(v_is_truthy(REG(s1)) && v_is_truthy(REG(s2)));
                    break;
                }
                case R_OR: {
                    uint8_t dst = code[ip++];
                    uint8_t s1 = code[ip++];
                    uint8_t s2 = code[ip++];
                    REG(dst) = v_bool(v_is_truthy(REG(s1)) || v_is_truthy(REG(s2)));
                    break;
                }
                case R_JMP: {
                    uint32_t tgt;
                    memcpy(&tgt, code + ip, 4);
                    ip = tgt;
                    break;
                }
                case R_JMP_IF_FALSE: {
                    uint8_t cond = code[ip++];
                    uint32_t tgt;
                    memcpy(&tgt, code + ip, 4);
                    ip += 4;
                    if (!v_is_truthy(REG(cond))) ip = tgt;
                    break;
                }
                case R_JMP_IF_TRUE: {
                    uint8_t cond = code[ip++];
                    uint32_t tgt;
                    memcpy(&tgt, code + ip, 4);
                    ip += 4;
                    if (v_is_truthy(REG(cond))) ip = tgt;
                    break;
                }
                case R_CALL_BUILTIN: {
                    uint8_t dst = code[ip++];
                    uint16_t id;
                    memcpy(&id, code + ip, 2);
                    ip += 2;
                    uint8_t arg_base = code[ip++];
                    uint8_t argc = code[ip++];

                    Value args[256];
                    for (uint8_t i = 0; i < argc; i++) {
                        args[i] = REG(arg_base + i);
                    }

                    int exit_code = 0;
                    bool exiting = false;

                    // Lock GC during builtin call for thread safety
                    pthread_mutex_lock(&g_thread_ctx.alloc_lock);
                    Value result = stdlib_call((BuiltinId)id, args, argc,
                                               &vm->gc, &exit_code, &exiting);
                    pthread_mutex_unlock(&g_thread_ctx.alloc_lock);

                    REG(dst) = result;
                    break;
                }
                case R_RET: {
                    uint8_t src = code[ip++];
                    thread->result = REG(src);
                    goto thread_done;
                }
                default:
                    // Unsupported opcode in thread - return null
                    thread->result = v_null();
                    goto thread_done;
            }
        }

        #undef REG

        // If we reach end of function without return
        thread->result = v_int(0);
    } else {
        // Stack-based bytecode not supported in threads yet
        thread->result = v_null();
    }

thread_done:
    // Free thread-local resources
    free(thread->regs);
    thread->regs = NULL;
    free(thread->args);
    thread->args = NULL;

    // Update state and signal completion
    pthread_mutex_lock(&thread->state_lock);
    thread->state = THREAD_FINISHED;
    pthread_cond_broadcast(&thread->done_cond);
    pthread_mutex_unlock(&thread->state_lock);

    // Clear thread-local data
    tl_current_thread = NULL;
    tl_thread_id = 0;

    return NULL;
}

// ============================================================================
// Thread Creation and Management
// ============================================================================

BpThread *bp_thread_create(Vm *vm, uint32_t func_idx, Value *args, size_t argc) {
    if (!g_thread_ctx.initialized) {
        bp_thread_init();
    }

    // Allocate thread structure
    BpThread *thread = bp_xmalloc(sizeof(BpThread));
    memset(thread, 0, sizeof(BpThread));

    thread->id = atomic_fetch_add(&g_thread_ctx.next_id, 1);
    thread->state = THREAD_CREATED;
    thread->joinable = true;
    thread->vm = vm;
    thread->func_idx = func_idx;

    // Copy arguments
    if (argc > 0 && args) {
        thread->args = bp_xmalloc(argc * sizeof(Value));
        memcpy(thread->args, args, argc * sizeof(Value));
        thread->argc = argc;
    } else {
        thread->args = NULL;
        thread->argc = 0;
    }

    thread->result = v_null();

    // Initialize synchronization
    pthread_mutex_init(&thread->state_lock, NULL);
    pthread_cond_init(&thread->done_cond, NULL);

    // Register in global registry
    if (!register_thread(thread)) {
        free(thread->args);
        pthread_mutex_destroy(&thread->state_lock);
        pthread_cond_destroy(&thread->done_cond);
        free(thread);
        return NULL;
    }

    return thread;
}

bool bp_thread_start(BpThread *thread) {
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_lock);
    if (thread->state != THREAD_CREATED) {
        pthread_mutex_unlock(&thread->state_lock);
        return false;  // Already started or finished
    }
    pthread_mutex_unlock(&thread->state_lock);

    // Create the actual thread
    int result = pthread_create(&thread->handle, NULL, thread_entry, thread);
    if (result != 0) {
        return false;
    }

    return true;
}

Value bp_thread_join(BpThread *thread) {
    if (!thread) return v_null();

    pthread_mutex_lock(&thread->state_lock);

    // Check if joinable
    if (!thread->joinable) {
        pthread_mutex_unlock(&thread->state_lock);
        return v_null();
    }

    // Wait for thread to finish
    while (thread->state != THREAD_FINISHED) {
        pthread_cond_wait(&thread->done_cond, &thread->state_lock);
    }

    Value result = thread->result;
    thread->joinable = false;

    pthread_mutex_unlock(&thread->state_lock);

    // Join the pthread
    pthread_join(thread->handle, NULL);

    // Unregister from registry
    unregister_thread(thread);

    // Clean up thread structure
    pthread_mutex_destroy(&thread->state_lock);
    pthread_cond_destroy(&thread->done_cond);
    free(thread);

    return result;
}

bool bp_thread_detach(BpThread *thread) {
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_lock);

    if (!thread->joinable) {
        pthread_mutex_unlock(&thread->state_lock);
        return false;  // Already detached
    }

    thread->joinable = false;
    thread->state = THREAD_DETACHED;

    pthread_mutex_unlock(&thread->state_lock);

    pthread_detach(thread->handle);

    return true;
}

bool bp_thread_is_running(BpThread *thread) {
    if (!thread) return false;

    pthread_mutex_lock(&thread->state_lock);
    bool running = (thread->state == THREAD_RUNNING);
    pthread_mutex_unlock(&thread->state_lock);

    return running;
}

void bp_thread_yield(void) {
    sched_yield();
}

void bp_thread_sleep(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000);
    nanosleep(&ts, NULL);
}

// ============================================================================
// Mutex Operations
// ============================================================================

BpMutex *bp_mutex_new(void) {
    BpMutex *mutex = bp_xmalloc(sizeof(BpMutex));
    pthread_mutex_init(&mutex->lock, NULL);
    mutex->owner_thread = 0;
    mutex->locked = false;
    return mutex;
}

void bp_mutex_free(BpMutex *mutex) {
    if (!mutex) return;
    pthread_mutex_destroy(&mutex->lock);
    free(mutex);
}

void bp_mutex_lock(BpMutex *mutex) {
    if (!mutex) return;

    pthread_mutex_lock(&mutex->lock);
    mutex->owner_thread = bp_thread_current_id();
    mutex->locked = true;
}

bool bp_mutex_trylock(BpMutex *mutex) {
    if (!mutex) return false;

    int result = pthread_mutex_trylock(&mutex->lock);
    if (result == 0) {
        mutex->owner_thread = bp_thread_current_id();
        mutex->locked = true;
        return true;
    }
    return false;
}

void bp_mutex_unlock(BpMutex *mutex) {
    if (!mutex) return;

    mutex->owner_thread = 0;
    mutex->locked = false;
    pthread_mutex_unlock(&mutex->lock);
}

// ============================================================================
// Condition Variable Operations
// ============================================================================

BpCondition *bp_cond_new(void) {
    BpCondition *cond = bp_xmalloc(sizeof(BpCondition));
    pthread_cond_init(&cond->cond, NULL);
    return cond;
}

void bp_cond_free(BpCondition *cond) {
    if (!cond) return;
    pthread_cond_destroy(&cond->cond);
    free(cond);
}

void bp_cond_wait(BpCondition *cond, BpMutex *mutex) {
    if (!cond || !mutex) return;

    // Release ownership while waiting
    mutex->owner_thread = 0;
    mutex->locked = false;

    pthread_cond_wait(&cond->cond, &mutex->lock);

    // Re-acquire ownership
    mutex->owner_thread = bp_thread_current_id();
    mutex->locked = true;
}

bool bp_cond_timedwait(BpCondition *cond, BpMutex *mutex, uint64_t timeout_ms) {
    if (!cond || !mutex) return false;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ms / 1000);
    ts.tv_nsec += (long)((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    mutex->owner_thread = 0;
    mutex->locked = false;

    int result = pthread_cond_timedwait(&cond->cond, &mutex->lock, &ts);

    mutex->owner_thread = bp_thread_current_id();
    mutex->locked = true;

    return (result == 0);
}

void bp_cond_signal(BpCondition *cond) {
    if (!cond) return;
    pthread_cond_signal(&cond->cond);
}

void bp_cond_broadcast(BpCondition *cond) {
    if (!cond) return;
    pthread_cond_broadcast(&cond->cond);
}

// ============================================================================
// GC Integration
// ============================================================================

void bp_thread_safepoint(void) {
    // Check if GC is requested
    if (!atomic_load(&g_thread_ctx.gc_requested)) {
        return;
    }

    // Signal that we're at a safepoint
    atomic_fetch_add(&g_thread_ctx.threads_at_safepoint, 1);

    // Wait for GC to complete
    pthread_mutex_lock(&g_thread_ctx.gc_lock);
    while (atomic_load(&g_thread_ctx.gc_in_progress)) {
        pthread_cond_wait(&g_thread_ctx.gc_done_cond, &g_thread_ctx.gc_lock);
    }
    pthread_mutex_unlock(&g_thread_ctx.gc_lock);

    // Decrement safepoint counter
    atomic_fetch_sub(&g_thread_ctx.threads_at_safepoint, 1);
}

void bp_thread_request_gc(Gc *gc, Value *roots, size_t root_count) {
    pthread_mutex_lock(&g_thread_ctx.gc_lock);

    // Set GC requested flag
    atomic_store(&g_thread_ctx.gc_requested, true);
    atomic_store(&g_thread_ctx.gc_in_progress, true);

    // Wait for all threads to reach safepoint
    uint32_t thread_count = atomic_load(&g_thread_ctx.thread_count);
    while ((uint32_t)atomic_load(&g_thread_ctx.threads_at_safepoint) < thread_count) {
        // Brief sleep to avoid busy waiting
        struct timespec ts = {0, 1000000};  // 1ms
        nanosleep(&ts, NULL);
    }

    // All threads stopped - perform GC
    gc_collect(gc, roots, root_count, NULL, 0);

    // Clear flags and signal completion
    atomic_store(&g_thread_ctx.gc_requested, false);
    atomic_store(&g_thread_ctx.gc_in_progress, false);
    pthread_cond_broadcast(&g_thread_ctx.gc_done_cond);

    pthread_mutex_unlock(&g_thread_ctx.gc_lock);
}

void bp_thread_mark_roots(BpThread *thread, Gc *gc) {
    BP_UNUSED(thread);
    BP_UNUSED(gc);
    // For future GC integration - mark thread-local roots
}
