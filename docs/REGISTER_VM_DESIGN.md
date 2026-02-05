# Register-Based VM Design Document

## Overview

This document describes the conversion of BetterPython from a stack-based VM to a register-based VM. This is expected to provide 20-40% performance improvement by reducing memory traffic and enabling better CPU cache utilization.

## Current Architecture (Stack-Based)

### Stack-Based Instruction Format
```
OP_ADD_I64          ; pop two values, push result
OP_CONST_I64 <val>  ; push constant
OP_LOAD_LOCAL <slot> ; push local variable
OP_STORE_LOCAL <slot> ; pop and store to local
```

### Example: `a = b + c`
```
OP_LOAD_LOCAL 1     ; push b
OP_LOAD_LOCAL 2     ; push c
OP_ADD_I64          ; pop b,c; push b+c
OP_STORE_LOCAL 0    ; pop result to a
```
**4 instructions, 4 stack operations**

## New Architecture (Register-Based)

### Design Decisions

**Decision 1: Virtual Register Count**
- Use 256 virtual registers (r0-r255)
- Registers 0-N are function parameters
- Registers N+1 onwards are locals and temporaries
- Compiler handles register allocation

**Decision 2: Instruction Format**
- 3-address format: `OP dst, src1, src2`
- All operands are register indices (1 byte each)
- Variable-length instructions (1-10 bytes)

**Decision 3: Backward Compatibility**
- Keep old stack-based bytecode format as `.bpc`
- New register-based format as `.bpr`
- VM can execute both (detect via magic number)

### Register-Based Instruction Format
```
R_ADD_I64 <dst> <src1> <src2>  ; r[dst] = r[src1] + r[src2]
R_CONST_I64 <dst> <val>        ; r[dst] = val
R_MOV <dst> <src>              ; r[dst] = r[src]
R_CALL <dst> <fn_idx> <argc>   ; r[dst] = call fn with argc args starting at r[dst+1]
```

### Example: `a = b + c` (assuming a=r0, b=r1, c=r2)
```
R_ADD_I64 0 1 2    ; r0 = r1 + r2
```
**1 instruction, 0 stack operations**

## New Opcode Definitions

### Arithmetic (3-address)
```c
R_ADD_I64 = 128,    // dst = src1 + src2
R_SUB_I64,          // dst = src1 - src2
R_MUL_I64,          // dst = src1 * src2
R_DIV_I64,          // dst = src1 / src2
R_MOD_I64,          // dst = src1 % src2
R_ADD_F64,          // dst = src1 + src2 (float)
R_SUB_F64,
R_MUL_F64,
R_DIV_F64,
R_NEG_I64,          // dst = -src1 (2-address)
R_NEG_F64,
```

### Constants and Movement
```c
R_CONST_I64,        // dst, imm64
R_CONST_F64,        // dst, imm64 (as double bits)
R_CONST_BOOL,       // dst, imm8
R_CONST_STR,        // dst, str_idx
R_CONST_NULL,       // dst
R_MOV,              // dst = src
```

### Comparisons (result in dst register as bool)
```c
R_EQ,               // dst = (src1 == src2)
R_NEQ,              // dst = (src1 != src2)
R_LT,               // dst = (src1 < src2)
R_LTE,              // dst = (src1 <= src2)
R_GT,               // dst = (src1 > src2)
R_GTE,              // dst = (src1 >= src2)
R_LT_F64,           // float comparisons
R_LTE_F64,
R_GT_F64,
R_GTE_F64,
```

### Logic
```c
R_NOT,              // dst = !src
R_AND,              // dst = src1 && src2
R_OR,               // dst = src1 || src2
```

### Control Flow
```c
R_JMP,              // unconditional jump
R_JMP_IF_FALSE,     // if (!r[cond]) jump
R_JMP_IF_TRUE,      // if (r[cond]) jump
```

### Function Calls
```c
R_CALL,             // dst = call(fn_idx, argc) - args in r[dst+1..dst+argc]
R_CALL_BUILTIN,     // dst = builtin(id, argc)
R_RET,              // return r[src]
```

### Arrays and Maps
```c
R_ARRAY_NEW,        // dst = new array from r[src..src+count]
R_ARRAY_GET,        // dst = arr[idx]
R_ARRAY_SET,        // arr[idx] = val
R_MAP_NEW,          // dst = new map
R_MAP_GET,          // dst = map[key]
R_MAP_SET,          // map[key] = val
```

### Structs and Classes
```c
R_STRUCT_NEW,       // dst = new struct
R_STRUCT_GET,       // dst = struct.field
R_STRUCT_SET,       // struct.field = val
R_CLASS_NEW,        // dst = new class instance
R_CLASS_GET,        // dst = instance.field
R_CLASS_SET,        // instance.field = val
R_METHOD_CALL,      // dst = instance.method(args)
```

## Bytecode Format

### File Header (Register-Based)
```
Magic: "BPR1" (4 bytes) - distinguishes from stack-based "BPC1"
Version: u16
Entry function index: u32
String pool count: u32
Function count: u32
Struct type count: u32
Class type count: u32
```

### Instruction Encoding

**1-byte instructions:**
```
[opcode]
```

**2-byte instructions (1 register):**
```
[opcode][dst]
```

**3-byte instructions (2 registers):**
```
[opcode][dst][src]
```

**4-byte instructions (3 registers):**
```
[opcode][dst][src1][src2]
```

**Variable-length (constants):**
```
[opcode][dst][...data...]
R_CONST_I64: [op][dst][i64 - 8 bytes] = 10 bytes
R_CONST_STR: [op][dst][str_idx - 4 bytes] = 6 bytes
```

## Register Allocation Strategy

### Linear Scan Algorithm

The compiler will use a simplified linear scan register allocation:

1. **Live Range Analysis**: For each variable, compute first use and last use
2. **Register Assignment**: Assign registers in order of first use
3. **Spilling**: If we exceed 256 registers (unlikely), spill to memory

### Example Allocation

```python
def foo(a: int, b: int) -> int:
    let x: int = a + b      # x = r2 (r0=a, r1=b)
    let y: int = x * 2      # y = r3
    return x + y            # result in r0 (return register)
```

Allocation:
- r0: parameter `a`, then reused for return value
- r1: parameter `b`
- r2: local `x`
- r3: local `y`

Generated bytecode:
```
R_ADD_I64 2 0 1      ; r2 = r0 + r1 (x = a + b)
R_CONST_I64 4 2      ; r4 = 2
R_MUL_I64 3 2 4      ; r3 = r2 * r4 (y = x * 2)
R_ADD_I64 0 2 3      ; r0 = r2 + r3 (return x + y)
R_RET 0              ; return r0
```

## Implementation Plan

### Phase 1: Bytecode Format (This Session)
- [ ] Add new opcodes to bytecode.h
- [ ] Create bytecode reader/writer for register format
- [ ] Add magic number detection for format switching

### Phase 2: Register Allocator (This Session)
- [ ] Add register allocation pass to compiler
- [ ] Track live ranges for variables
- [ ] Assign registers to variables and temporaries

### Phase 3: Code Generator (Next Session)
- [ ] Modify compiler to emit register-based bytecode
- [ ] Handle all expression types
- [ ] Handle control flow (if/while/for)
- [ ] Handle function calls

### Phase 4: VM Execution (Next Session)
- [ ] Add register file to VM state
- [ ] Implement all register-based opcode handlers
- [ ] Computed goto dispatch for register opcodes

### Phase 5: Testing & Optimization (Final Session)
- [ ] Run all existing tests with register VM
- [ ] Benchmark comparison
- [ ] Optimize hot paths

## Progress Log

### Session 1 (Current)
- Created this design document
- Next: Define opcodes in bytecode.h

---
*Last updated: Session 1*
*Status: Design phase*
