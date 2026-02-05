# JIT Compilation Design Document

## Overview

This document describes the Just-In-Time (JIT) compilation system for BetterPython. The JIT compiler translates hot bytecode functions into native machine code at runtime, providing significant performance improvements for frequently executed code paths.

## Design Goals

1. **Performance**: 5-10x speedup for compute-intensive code
2. **Simplicity**: Straightforward implementation without complex optimizations initially
3. **Safety**: Maintain memory safety and type safety guarantees
4. **Incremental**: Start simple, add optimizations over time
5. **Compatibility**: Works alongside interpreter for cold code

## Architecture

### High-Level Flow

```
BetterPython Source → Bytecode → Interpreter (cold)
                                      ↓
                              Profile Counter++
                                      ↓
                              Counter > Threshold?
                                      ↓ Yes
                              JIT Compile Function
                                      ↓
                              Native Code Cache
                                      ↓
                              Direct Execution (hot)
```

### Components

1. **Profiler**: Counts function invocations and loop iterations
2. **JIT Compiler**: Translates bytecode to native code
3. **Code Cache**: Stores compiled native code
4. **Runtime**: Provides GC integration and builtin calls

## Profiling Strategy

### Compilation Thresholds

```c
#define JIT_FUNC_THRESHOLD    100    // Compile after 100 calls
#define JIT_LOOP_THRESHOLD    1000   // Compile hot loops after 1000 iterations
#define JIT_OSR_THRESHOLD     5000   // On-stack replacement threshold
```

### Profile Data Structure

```c
typedef struct {
    uint32_t call_count;        // Function invocation count
    uint32_t *loop_counts;      // Per-loop iteration counts
    uint8_t  jit_state;         // 0=cold, 1=warm, 2=hot, 3=compiled
    void    *native_code;       // Pointer to JIT-compiled code
    size_t   native_size;       // Size of native code
} JitProfile;
```

### Integration with Register VM

The register-based bytecode maps naturally to machine registers:
- Virtual registers r0-r15 → x86-64 registers (RAX, RBX, RCX, etc.)
- Overflow registers → Stack slots
- This mapping eliminates most load/store operations

## Native Code Generation

### Target Architecture

**Primary**: x86-64 (Linux, macOS)
**Future**: ARM64 (Apple Silicon, Linux ARM)

### Register Mapping (x86-64)

```
BetterPython    x86-64 Register   Usage
-----------------------------------------------
r0              RAX               Return value, temp
r1              RCX               Temp
r2              RDX               Temp
r3              RBX               Preserved (callee-save)
r4              RSI               Temp
r5              RDI               Temp
r6              R8                Temp
r7              R9                Temp
r8-r15          R10-R15           Temp/preserved mix
r16+            [RBP - offset]    Stack-spilled registers
```

### Calling Convention

```
- Arguments passed in: RDI, RSI, RDX, RCX, R8, R9
- Return value in: RAX
- Callee-saved: RBX, RBP, R12-R15
- Caller-saved: RAX, RCX, RDX, RSI, RDI, R8-R11
```

### Code Buffer Management

```c
typedef struct {
    uint8_t *code;          // Executable memory region
    size_t   capacity;      // Total buffer size
    size_t   offset;        // Current write position

    // Relocation table for fixups
    struct {
        size_t code_offset;
        size_t target_label;
    } *relocations;
    size_t reloc_count;

    // Label table
    size_t *labels;
    size_t label_count;
} CodeBuffer;
```

### Memory Management

```c
// Allocate executable memory
void *jit_alloc_code(size_t size) {
    void *mem = mmap(NULL, size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mem;
}

// Free executable memory
void jit_free_code(void *ptr, size_t size) {
    munmap(ptr, size);
}
```

## Bytecode to Native Translation

### Instruction Mapping

#### Arithmetic Operations

**Bytecode:**
```
R_ADD_I64 dst, src1, src2    ; r[dst] = r[src1] + r[src2]
```

**Native x86-64:**
```asm
mov  rax, [rbp + src1*8]     ; Load src1 (if spilled)
add  rax, [rbp + src2*8]     ; Add src2
mov  [rbp + dst*8], rax      ; Store result

; Or if both in registers:
mov  REG(dst), REG(src1)
add  REG(dst), REG(src2)
```

#### Constants

**Bytecode:**
```
R_CONST_I64 dst, imm64
```

**Native x86-64:**
```asm
mov  REG(dst), imm64         ; 10-byte mov for 64-bit immediate
; Or for small constants:
xor  eax, eax                ; Zero register
mov  eax, imm32              ; 32-bit immediate (zero-extended)
```

#### Comparisons

**Bytecode:**
```
R_LT dst, src1, src2         ; r[dst] = r[src1] < r[src2]
```

**Native x86-64:**
```asm
xor  eax, eax                ; Zero result
cmp  REG(src1), REG(src2)    ; Compare
setl al                      ; Set AL to 1 if less than
mov  REG(dst), rax           ; Store result
```

#### Control Flow

**Bytecode:**
```
R_JMP_IF_FALSE cond, offset
```

**Native x86-64:**
```asm
test REG(cond), REG(cond)    ; Test condition
jz   target_label            ; Jump if zero (false)
```

#### Function Calls

**Bytecode:**
```
R_CALL dst, fn_idx, argc     ; args in r[dst+1..dst+argc]
```

**Native x86-64:**
```asm
; Check if target is JIT-compiled
mov  rax, [jit_table + fn_idx*8]
test rax, rax
jz   .call_interp            ; Fall back to interpreter

; Direct call to native code
mov  rdi, arg1               ; First argument
mov  rsi, arg2               ; Second argument
...
call rax                     ; Call native function
mov  REG(dst), rax           ; Store return value
jmp  .continue

.call_interp:
; Call interpreter stub
...
.continue:
```

#### Array Operations

**Bytecode:**
```
R_ARRAY_GET dst, arr, idx
```

**Native x86-64:**
```asm
; Bounds check (can be eliminated with analysis)
mov  rax, [REG(arr)]         ; Load array header
cmp  REG(idx), [rax + 8]     ; Compare index with length
jae  .bounds_error           ; Jump if out of bounds

; Load element
mov  rax, [REG(arr) + 16]    ; Load array data pointer
mov  REG(dst), [rax + REG(idx)*8]  ; Load element
```

### Builtin Function Calls

For builtin functions, we generate direct calls to the C implementation:

```asm
; print(value)
mov  rdi, REG(arg0)          ; First argument
call _builtin_print          ; Direct C call
```

## Optimizations

### Phase 1: Basic JIT (Initial Implementation)

1. **Direct bytecode translation**: 1:1 mapping of bytecode to native
2. **Register allocation**: Use x86 registers for r0-r15, spill rest
3. **Constant folding**: Evaluate constant expressions at compile time

### Phase 2: Simple Optimizations (Future)

1. **Dead code elimination**: Remove unreachable code
2. **Common subexpression elimination**: Reuse computed values
3. **Strength reduction**: Replace expensive ops (multiply → shift)
4. **Bounds check elimination**: Remove redundant array bounds checks

### Phase 3: Advanced Optimizations (Future)

1. **Inlining**: Inline small, frequently called functions
2. **Loop unrolling**: Unroll small, hot loops
3. **Type specialization**: Generate type-specific code paths
4. **Escape analysis**: Stack-allocate non-escaping objects

## Runtime Integration

### GC Integration

The JIT must cooperate with the garbage collector:

```c
// GC safepoint - inserted at function entry and loop back-edges
void jit_gc_safepoint(JitFrame *frame) {
    if (gc_should_collect()) {
        // Save all registers to frame
        jit_save_registers(frame);
        gc_collect();
        // Reload registers from frame
        jit_restore_registers(frame);
    }
}
```

### Stack Walking

For debugging and GC, we need to walk the JIT stack:

```c
typedef struct {
    void *return_addr;       // Native return address
    void *bytecode_pc;       // Corresponding bytecode PC
    uint16_t reg_count;      // Number of live registers
    uint16_t spill_count;    // Number of spilled values
} JitStackMap;
```

### Exception Handling

```c
// On exception, unwind through JIT frames
void jit_unwind_frame(JitFrame *frame, Value exception) {
    // Find corresponding bytecode PC
    void *bc_pc = jit_native_to_bytecode(frame->return_addr);

    // Find exception handler (if any)
    ExceptionHandler *handler = find_handler(frame->func, bc_pc);
    if (handler) {
        // Jump to handler in native code
        jit_jump_to_handler(frame, handler, exception);
    } else {
        // Propagate to caller
        jit_unwind_frame(frame->caller, exception);
    }
}
```

## Implementation Plan

### Phase 1: Profiling Infrastructure

Files to create/modify:
- `src/jit_profile.c/h` - Profiling data structures and counting

Tasks:
- [ ] Add call counter to function objects
- [ ] Add loop iteration counters
- [ ] Detect hot functions and loops
- [ ] Trigger JIT compilation when threshold reached

### Phase 2: Code Buffer and Assembler

Files to create/modify:
- `src/jit_buffer.c/h` - Code buffer management
- `src/jit_x64.c/h` - x86-64 code generation macros/helpers

Tasks:
- [ ] Implement executable memory allocation
- [ ] Create x86-64 instruction emission macros
- [ ] Implement label and relocation handling
- [ ] Basic code buffer management

### Phase 3: Bytecode Compiler

Files to create/modify:
- `src/jit_compile.c/h` - Main JIT compilation logic

Tasks:
- [ ] Translate arithmetic operations
- [ ] Translate constants and moves
- [ ] Translate comparisons
- [ ] Translate control flow (jumps, conditionals)
- [ ] Translate function calls (native and builtin)
- [ ] Translate array/map operations

### Phase 4: Integration

Files to modify:
- `src/reg_vm.c` - Add JIT dispatch
- `src/vm.h` - Add JIT state to VM

Tasks:
- [ ] Integrate profiling into interpreter loop
- [ ] Add JIT dispatch for hot functions
- [ ] Handle mixed JIT/interpreted call stacks
- [ ] GC safepoints

### Phase 5: Testing and Debugging

Tasks:
- [ ] Unit tests for code generation
- [ ] Integration tests (run test suite with JIT)
- [ ] Benchmarks comparing interpreted vs JIT
- [ ] Debug output (disassembly of generated code)

## Code Structure

```
src/
├── jit/
│   ├── jit.h           # Public JIT API
│   ├── jit_profile.c   # Profiling and hot detection
│   ├── jit_buffer.c    # Code buffer management
│   ├── jit_x64.c       # x86-64 code generation
│   ├── jit_x64.h       # x86-64 instruction macros
│   ├── jit_compile.c   # Bytecode → native compiler
│   └── jit_runtime.c   # Runtime support (GC, exceptions)
```

## API

### Public Interface

```c
// Initialize JIT subsystem
void jit_init(Vm *vm);

// Shutdown JIT subsystem
void jit_shutdown(Vm *vm);

// Record function call (for profiling)
void jit_record_call(Vm *vm, BpFunc *func);

// Check if function should be JIT-compiled
bool jit_should_compile(BpFunc *func);

// Compile function to native code
bool jit_compile(Vm *vm, BpFunc *func);

// Execute JIT-compiled function
int64_t jit_execute(Vm *vm, BpFunc *func, Value *args, size_t argc);

// Invalidate JIT-compiled code (for deoptimization)
void jit_invalidate(BpFunc *func);
```

## Example: Fibonacci

### BetterPython Source

```python
def fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)
```

### Register Bytecode

```
fib:
    R_CONST_I64 1, 1         ; r1 = 1
    R_LTE 2, 0, 1            ; r2 = (r0 <= r1)
    R_JMP_IF_FALSE 2, +6     ; if !r2, skip
    R_RET 0                  ; return r0 (n)

    R_SUB_I64 3, 0, 1        ; r3 = n - 1
    R_CALL 4, fib, 1         ; r4 = fib(r3)
    R_CONST_I64 5, 2         ; r5 = 2
    R_SUB_I64 6, 0, 5        ; r6 = n - 2
    R_CALL 7, fib, 1         ; r7 = fib(r6)
    R_ADD_I64 0, 4, 7        ; r0 = r4 + r7
    R_RET 0                  ; return r0
```

### Generated x86-64 (Simplified)

```asm
fib:
    push rbp
    mov  rbp, rsp
    sub  rsp, 64              ; Space for spilled regs

    ; Profiling increment (removed after JIT)
    ; inc [profile_count]

    ; r0 (n) is in RDI (first arg)
    mov  rax, rdi             ; r0 = n

    ; if n <= 1, return n
    cmp  rax, 1
    jg   .recurse
    mov  rsp, rbp
    pop  rbp
    ret                       ; Return n in RAX

.recurse:
    mov  [rbp-8], rax         ; Save n

    ; fib(n-1)
    dec  rax
    mov  rdi, rax
    call fib                  ; Recursive call
    mov  [rbp-16], rax        ; Save fib(n-1)

    ; fib(n-2)
    mov  rax, [rbp-8]
    sub  rax, 2
    mov  rdi, rax
    call fib                  ; Recursive call

    ; Return fib(n-1) + fib(n-2)
    add  rax, [rbp-16]

    mov  rsp, rbp
    pop  rbp
    ret
```

## Benchmarks (Expected)

| Benchmark      | Interpreted | JIT      | Speedup |
|----------------|-------------|----------|---------|
| fib(35)        | 2.5s        | 0.3s     | 8.3x    |
| sum(1..1M)     | 150ms       | 15ms     | 10x     |
| matrix_mult    | 800ms       | 100ms    | 8x      |
| string_concat  | 200ms       | 180ms    | 1.1x    |

Note: String operations have minimal speedup due to allocation overhead.

## Safety Considerations

### Memory Safety
- JIT code uses the same GC as interpreted code
- All allocations go through the runtime
- No raw pointers exposed to user code

### Bounds Checking
- Array bounds checks are always performed
- Future optimization: eliminate provably safe checks

### Type Safety
- JIT code assumes types are correct (verified by type checker)
- No runtime type checks in JIT (performance critical)

### Code Memory
- JIT code is allocated with W^X (write XOR execute)
- Write during compilation, execute during runtime
- Never both at the same time

## Progress Log

### Session 1 (Current)
- Created this design document
- Next: Implement profiling infrastructure

---
*Last updated: Session 1*
*Status: Design phase*
