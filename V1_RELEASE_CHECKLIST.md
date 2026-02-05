# BetterPython v1.0.0 Release Checklist

## Overview

This checklist addresses all issues identified in the Developer Experience Assessment and codebase audit. Items are prioritized by severity and impact on user experience.

---

## 🔴 CRITICAL BUGS (Dealbreakers - Must Fix)

### 1. Variable Scope Bug
**Status:** ✅ FIXED
**Severity:** CRITICAL - Prevents normal coding patterns
**User Impact:** "Would drive developers crazy"

**The Problem:**
Variables declared in different blocks (if/else, for loops, try/catch) within the SAME function cannot reuse variable names:
```python
def func() -> int:
    if true:
        let i: int = 0  # OK
    else:
        let i: int = 1  # ERROR: duplicate symbol 'i'
    return 0
```

**Root Cause:**
- `src/types.c` uses a single flat `Scope` object per function
- `src/compiler.c` uses a single flat `Locals` table per function
- Variables are never removed when exiting blocks

**Files to Modify:**
- [ ] `src/types.c` - Implement scope stack (push/pop for blocks)
- [ ] `src/compiler.c` - Mirror scope stack in code generation
- [ ] `src/common.h` - Add ScopeStack structure (if needed)

**Implementation Approach:**
1. Create `scope_push()` and `scope_pop()` functions
2. Call `scope_push()` when entering: if-then, if-else, for body, while body, try, catch, finally
3. Call `scope_pop()` when exiting those blocks
4. Modify `scope_get()` to search from current scope up to parent scopes

**Test Cases:**
```python
# Test 1: If/Else with same variable
def test1() -> int:
    if true:
        let i: int = 0
    else:
        let i: int = 1  # Should work
    return 0

# Test 2: Sequential for loops
def test2() -> int:
    for i in range(0, 5):
        print(i)
    for i in range(0, 10):  # Should work
        print(i)
    return 0

# Test 3: Nested blocks
def test3() -> int:
    for i in range(0, 5):
        for j in range(0, 3):
            let k: int = i + j
        let k: int = 99  # Should work (different block)
    return 0
```

**Estimated Effort:** 4-6 hours

---

### 2. Float Printing Bug
**Status:** ✅ FIXED
**Severity:** CRITICAL - Cannot debug float code
**User Impact:** "`int_to_float(42)` prints `<?>`"

**The Problem:**
```python
let f: float = int_to_float(42)
print(f)  # Outputs: <?>
```

**Root Cause:**
`src/stdlib.c`, function `val_to_str()` (lines 30-45) has NO case for `VAL_FLOAT`:
```c
static BpStr *val_to_str(Value v, Gc *gc) {
    if (v.type == VAL_INT) { ... }
    if (v.type == VAL_BOOL) { ... }
    if (v.type == VAL_NULL) { ... }
    if (v.type == VAL_STR) return v.as.s;
    // MISSING: if (v.type == VAL_FLOAT) { ... }
    return gc_new_str(gc, "<?>", 3);  // Falls through here
}
```

**Fix:**
- [ ] Add float case to `val_to_str()` in `src/stdlib.c`

**Implementation:**
```c
if (v.type == VAL_FLOAT) {
    snprintf(buf, sizeof(buf), "%g", v.as.f);
    return gc_new_str(gc, buf, strlen(buf));
}
```

**Test Cases:**
```python
def main() -> int:
    print(int_to_float(42))     # Should print: 42
    print(int_to_float(0))      # Should print: 0
    print(3.14159)              # Should print: 3.14159
    print(1.0e10)               # Should print: 1e+10
    return 0
```

**Estimated Effort:** 15 minutes

---

### 3. Documentation Lies
**Status:** ✅ FIXED (Option C - Aliases implemented)
**Severity:** HIGH - Destroys user trust
**User Impact:** "Have to discover by trial and error"

**Issues Found:**

| Documented Name | Actual Name | Status |
|-----------------|-------------|--------|
| `str_split` | `str_split_str` | ✅ FIXED (both names work now) |
| `str_join` | `str_join_arr` | ✅ FIXED (both names work now) |
| `bytes_to_str` | - | NOT IMPLEMENTED |
| `str_to_bytes` | - | NOT IMPLEMENTED |
| `str_concat_all` | `str_concat_all` | NOT DOCUMENTED |

**Solution Applied:** Option C - Added aliases so both short and long names work:
- `str_split` and `str_split_str` both work
- `str_join` and `str_join_arr` both work

**Files Modified:**
- [x] `src/compiler.c` - Added `str_split` and `str_join` as aliases in builtin_id()
- [x] `src/types.c` - Added aliases in is_builtin() and check_builtin_call()

**Estimated Effort:** 1-2 hours

---

### 4. Error Messages Are Terse
**Status:** ✅ FIXED (Minimum Viable)
**Severity:** HIGH - Hard to debug
**User Impact:** "duplicate symbol 'i' - doesn't tell you WHERE"

**Current State After Fix:**
- Added `bp_fatal_at(line, ...)` function for line-numbered errors
- Key type errors now include line numbers:
  - Type mismatch in let statements
  - Duplicate symbol errors
  - Undefined variable errors
  - Return type mismatches
  - Loop condition type errors
  - Function call argument errors

**Example After Fix:**
```
line 2: type error: let x: int initialized with str
line 3: duplicate symbol 'x'
line 5: undefined variable 'y'
```

**Files Modified:**
- [x] `src/util.c` - Added `bp_fatal_at()` function
- [x] `src/util.h` - Added `bp_fatal_at()` declaration
- [x] `src/types.c` - Updated error calls to include line numbers

**Example Current vs Fixed:**
```
CURRENT: type error: let x: int initialized with str
FIXED:   main.bp:5: type error: let x: int initialized with str
```

**Files to Modify:**
- [ ] `src/types.c` - Add `st->line` to all bp_fatal() calls
- [ ] `src/compiler.c` - Add line info where available
- [ ] `src/util.c` - Enhance bp_fatal() to accept file:line format

**Estimated Effort:** 2-4 hours (basic), 8+ hours (with source context)

---

## 🟡 HIGH PRIORITY (Should Fix for v1.0.0)

### 5. Implement Missing Stdlib Functions
**Status:** NOT IMPLEMENTED

**Functions to implement:**
- [ ] `bytes_to_str(bytes: [int]) -> str` - Convert byte array to string
- [ ] `str_to_bytes(s: str) -> [int]` - Convert string to byte array

OR remove from bytecode.h:
- [ ] Remove `BI_BYTES_TO_STR` enum
- [ ] Remove `BI_STR_TO_BYTES` enum

**Estimated Effort:** 1-2 hours to implement, 10 minutes to remove

---

### 6. Document str_concat_all
**Status:** NOT DOCUMENTED

**Function exists but is secret:**
```python
let arr: [str] = ["hello", " ", "world"]
let result: str = str_concat_all(arr)  # Works but undocumented
```

- [ ] Add to STDLIB_API.md
- [ ] Add to docs/STRING_API.md

**Estimated Effort:** 15 minutes

---

## 🟠 MEDIUM PRIORITY (Nice for v1.0.0)

### 7. Better Error Suggestions
**Status:** NOT IMPLEMENTED

**Example improvements:**
```
CURRENT: unknown function 'str_split'
BETTER:  unknown function 'str_split'. Did you mean 'str_split_str'?
```

- [ ] Add Levenshtein distance matching for typo suggestions
- [ ] Suggest similar function names for unknown builtins

**Estimated Effort:** 2-4 hours

---

### 8. Source Context in Errors
**Status:** NOT IMPLEMENTED

**Example:**
```
error at main.bp:5:22

    5 |     let x: int = "hello"
      |                  ^^^^^^^ type mismatch: expected int, got str
```

- [ ] Store source lines during compilation
- [ ] Display relevant line in error messages
- [ ] Add visual pointer (^^^) to error location

**Estimated Effort:** 4-6 hours

---

## 🔵 DECISIONS REQUIRED

### 9. Classes/OOP
**Status:** 30% implemented (methods don't work)

**Options:**
- [ ] A: Fully implement (OP_METHOD_CALL, constructors, inheritance) - 20+ hours
- [ ] B: Remove from v1.0.0, document as v1.1.0 feature - 2 hours
- [ ] C: Document current limitations clearly - 1 hour

**Recommendation:** Option B or C

---

### 10. FFI (Foreign Function Interface)
**Status:** 5% (stub returns null)

**Options:**
- [ ] A: Implement dlopen/dlsym for C interop - 15+ hours
- [ ] B: Remove from v1.0.0, document as v1.1.0 feature - 30 minutes

**Recommendation:** Option B

---

### 11. Enums
**Status:** 0% (type lookup unimplemented)

**Options:**
- [ ] A: Implement enum support - 10+ hours
- [ ] B: Remove from v1.0.0, document as v1.1.0 feature - 30 minutes

**Recommendation:** Option B

---

### 12. JIT Compilation
**Status:** ✅ INTEGRATED (Option A completed)

**What was done:**
- Integrated JIT profiling into register-based VM
- Hot functions (100+ calls) automatically compiled to x86-64 native code
- Native code execution when available
- JIT statistics printed when compilations occur

**Files Modified:**
- [x] `src/vm.h` - Added JitContext include and updated jit field type
- [x] `src/reg_vm.c` - Integrated JIT profiling, compilation, and execution
- [x] `src/bp_main.c` - Added JIT initialization and cleanup

**Current Limitations:**
- JIT only works with register-based bytecode (--register flag)
- Simple integer operations fully supported
- Complex operations (calls, arrays, strings) fall back to interpreter
- Value type boxing/unboxing needs refinement for full correctness

---

## 🟢 DOCUMENTATION UPDATES

### 13. README Updates
- [ ] Add "Known Limitations" section
- [ ] Remove claims about non-working features
- [ ] Update function count to accurate number
- [ ] Add v1.0.0 feature matrix

### 14. Create v1.0.0 Feature Matrix
- [ ] List what's FULLY working
- [ ] List what's PARTIALLY working (with caveats)
- [ ] List what's PLANNED for v1.1.0

### 15. Update CHANGELOG
- [ ] Document all v1.0.0 features
- [ ] Note any breaking changes
- [ ] Credit contributors

---

## 🧹 CLEANUP

### 16. Remove Backup Files
- [ ] Delete `stdlib.c.backup` if it exists

### 17. Code Quality
- [ ] Remove/update TODO comments
- [ ] Ensure zero compiler warnings
- [ ] Run full test suite

### 18. Testing
- [ ] Create test for variable scoping fix
- [ ] Create test for float printing fix
- [ ] Verify all documented functions work
- [ ] Run integration tests

---

## Summary by Priority

| Priority | Items | Est. Effort |
|----------|-------|-------------|
| 🔴 Critical | 4 | 8-12 hours |
| 🟡 High | 2 | 1-2 hours |
| 🟠 Medium | 2 | 6-10 hours |
| 🔵 Decisions | 4 | 2-4 hours |
| 🟢 Docs | 3 | 2-3 hours |
| 🧹 Cleanup | 3 | 1-2 hours |

**Total Estimated Effort: 20-33 hours**

---

## Minimum Viable v1.0.0

If time is limited, fix these at minimum:

1. ✅ Float printing bug (15 min)
2. ✅ Variable scope bug (4-6 hours)
3. ✅ Documentation accuracy (1-2 hours)
4. ✅ Add line numbers to errors (2-4 hours)

**Minimum effort: 8-12 hours**

---

## v1.0.0 Release Criteria

Before release, ALL of these must be true:

- [ ] All 22 existing tests pass
- [ ] Zero compiler warnings
- [ ] Float printing works (`print(3.14)` shows `3.14`)
- [ ] Variable names can be reused across blocks
- [ ] Documentation matches implementation
- [ ] Known limitations are documented
- [ ] Clean build completes in < 5 seconds

---

*Last updated: 2026-02-05*
*Created by: Claude (Anthropic)*
