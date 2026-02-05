# Threading Design Document

## Overview

This document describes the multithreading implementation for BetterPython. The goal is to provide safe, ergonomic threading primitives that allow concurrent execution while maintaining memory safety.

## Design Goals

1. **Safety**: Prevent data races and deadlocks through proper synchronization
2. **Simplicity**: Easy-to-use API similar to Python's threading module
3. **Performance**: Minimal overhead for thread management
4. **GC Integration**: Thread-safe garbage collection
5. **Portability**: Use POSIX threads (pthreads) on Unix, Windows threads on Windows

## Architecture

### Thread Model

```
Main Thread (VM)
    |
    +-- Worker Thread 1 (separate VM state)
    |       |
    |       +-- Thread-local registers
    |       +-- Thread-local stack
    |       +-- Shared heap (GC managed)
    |
    +-- Worker Thread 2
    |
    +-- Worker Thread N
```

### Key Components

1. **Thread**: Represents a single thread of execution
2. **Mutex**: Mutual exclusion lock for protecting shared data
3. **Condition**: Condition variable for thread signaling
4. **Channel**: Thread-safe message passing (future enhancement)
5. **Atomic**: Atomic operations for lock-free programming (future)

## Data Structures

### Thread Handle

```c
typedef struct BpThread {
    pthread_t handle;           // Native thread handle
    uint32_t id;                // BetterPython thread ID
    bool joinable;              // Can be joined
    bool detached;              // Is detached
    bool running;               // Currently running
    Value result;               // Return value from thread function

    // Thread-local VM state
    Value *regs;                // Register file
    size_t reg_count;           // Number of registers
    Value *stack;               // Operand stack (for builtins)
    size_t sp;                  // Stack pointer

    // Thread function info
    uint32_t func_idx;          // Function to execute
    Value *args;                // Arguments
    size_t argc;                // Argument count

    // Synchronization
    pthread_mutex_t state_lock; // Protects thread state
    pthread_cond_t done_cond;   // Signaled when thread completes
} BpThread;
```

### Mutex

```c
typedef struct BpMutex {
    GcHeader gc;                // For garbage collection
    pthread_mutex_t lock;       // Native mutex
    uint32_t owner_thread;      // Thread ID holding the lock (0 = unlocked)
    bool recursive;             // Allow recursive locking
    uint32_t lock_count;        // For recursive mutexes
} BpMutex;
```

### Condition Variable

```c
typedef struct BpCondition {
    GcHeader gc;                // For garbage collection
    pthread_cond_t cond;        // Native condition variable
    BpMutex *mutex;             // Associated mutex
} BpCondition;
```

## Thread-Safe Garbage Collection

### Stop-The-World Strategy

For initial implementation, use a simple stop-the-world approach:

1. GC thread requests collection
2. All threads reach a safepoint
3. GC marks and sweeps
4. All threads resume

### Safepoints

Safepoints are locations where threads can safely pause for GC:
- Function entry
- Loop back-edges
- Allocation sites

```c
typedef struct {
    pthread_mutex_t gc_lock;        // Global GC lock
    pthread_cond_t gc_start_cond;   // Signal to start GC
    pthread_cond_t gc_done_cond;    // Signal GC complete

    atomic_int threads_stopped;     // Count of stopped threads
    atomic_int total_threads;       // Total thread count
    atomic_bool gc_requested;       // GC has been requested
    atomic_bool gc_in_progress;     // GC is running
} GcSync;
```

### Thread-Safe Allocation

All allocations go through a global allocator with locking:

```c
void *gc_alloc_threadsafe(Gc *gc, size_t size) {
    pthread_mutex_lock(&gc->alloc_lock);
    void *ptr = gc_alloc(gc, size);
    pthread_mutex_unlock(&gc->alloc_lock);
    return ptr;
}
```

## API Design

### Thread Creation

```python
import thread

def worker(x: int) -> int:
    print("Worker: ")
    print(x)
    return x * 2

def main() -> int:
    # Create and start a thread
    let t: Thread = thread.spawn(worker, 42)

    # Wait for thread to complete and get result
    let result: int = thread.join(t)
    print(result)  # 84

    return 0
```

### Mutex Usage

```python
import thread

let counter: int = 0
let lock: Mutex = thread.mutex_new()

def increment() -> int:
    thread.mutex_lock(lock)
    counter = counter + 1
    thread.mutex_unlock(lock)
    return 0

def main() -> int:
    let threads: [Thread] = []

    for i in range(10):
        let t: Thread = thread.spawn(increment)
        array_push(threads, t)

    for t in threads:
        thread.join(t)

    print(counter)  # 10
    return 0
```

### Condition Variables

```python
import thread

let ready: bool = false
let lock: Mutex = thread.mutex_new()
let cond: Condition = thread.cond_new(lock)

def producer() -> int:
    thread.sleep(100)  # Simulate work

    thread.mutex_lock(lock)
    ready = true
    thread.cond_signal(cond)
    thread.mutex_unlock(lock)
    return 0

def consumer() -> int:
    thread.mutex_lock(lock)
    while not ready:
        thread.cond_wait(cond)
    print("Data is ready!")
    thread.mutex_unlock(lock)
    return 0

def main() -> int:
    let p: Thread = thread.spawn(producer)
    let c: Thread = thread.spawn(consumer)

    thread.join(p)
    thread.join(c)
    return 0
```

## Built-in Functions

### Thread Management

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread_spawn` | `(func, args...) -> Thread` | Create and start a new thread |
| `thread_join` | `(Thread) -> any` | Wait for thread and get result |
| `thread_detach` | `(Thread) -> bool` | Detach thread (cannot join) |
| `thread_current` | `() -> int` | Get current thread ID |
| `thread_yield` | `() -> void` | Yield to other threads |
| `thread_sleep` | `(int) -> void` | Sleep for milliseconds |

### Mutex Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `mutex_new` | `() -> Mutex` | Create a new mutex |
| `mutex_lock` | `(Mutex) -> void` | Acquire the lock |
| `mutex_trylock` | `(Mutex) -> bool` | Try to acquire (non-blocking) |
| `mutex_unlock` | `(Mutex) -> void` | Release the lock |

### Condition Variables

| Function | Signature | Description |
|----------|-----------|-------------|
| `cond_new` | `(Mutex) -> Condition` | Create condition variable |
| `cond_wait` | `(Condition) -> void` | Wait on condition |
| `cond_signal` | `(Condition) -> void` | Signal one waiting thread |
| `cond_broadcast` | `(Condition) -> void` | Signal all waiting threads |

## Implementation Plan

### Phase 1: Basic Threading
- [x] Design document (this file)
- [ ] Thread data structures
- [ ] Thread creation (pthread_create wrapper)
- [ ] Thread joining (pthread_join wrapper)
- [ ] Thread-local VM state

### Phase 2: Synchronization
- [ ] Mutex implementation
- [ ] Condition variable implementation
- [ ] Thread-safe built-in registration

### Phase 3: GC Integration
- [ ] Stop-the-world synchronization
- [ ] Safepoint insertion
- [ ] Thread-safe allocation

### Phase 4: Testing
- [ ] Basic thread tests
- [ ] Mutex contention tests
- [ ] Producer-consumer tests
- [ ] Stress tests

## Safety Considerations

### Avoiding Deadlocks

1. **Lock ordering**: Document and enforce consistent lock ordering
2. **Timeout locks**: Provide trylock with timeout
3. **Deadlock detection**: Debug mode to detect cycles (future)

### Avoiding Data Races

1. **All shared data must be protected by mutex**
2. **Immutable data is safe to share**
3. **Use atomic operations for simple counters** (future)

### Memory Safety

1. **Threads reference GC-managed objects**: Objects won't be freed while referenced
2. **Thread-local data**: Each thread has its own stack and registers
3. **Shared heap**: All threads share the same GC heap

## Error Handling

Threading errors will throw exceptions:
- `ThreadError`: General threading error
- `LockError`: Mutex-related error
- `DeadlockError`: Deadlock detected (future)

## Platform Support

### POSIX (Linux, macOS)
- Use pthreads directly
- Condition variables: `pthread_cond_t`
- Mutexes: `pthread_mutex_t`

### Windows (Future)
- Use Windows threads API
- `CreateThread`, `WaitForSingleObject`
- `CRITICAL_SECTION` for mutexes

## Performance Notes

1. **Thread pool** (future): Reuse threads instead of creating/destroying
2. **Lock-free structures** (future): Use atomics for high-contention scenarios
3. **Work stealing** (future): Load balancing for parallel workloads

---
*Last updated: Session 3*
*Status: Design complete, implementation pending*
