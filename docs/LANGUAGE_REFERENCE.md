# BetterPython Language Reference

**Version 2.0.0 - Complete Syntax and Feature Reference**

This document provides a complete reference for all BetterPython language features, syntax, and semantics. Essential reading for the self-hosting compiler rewrite.

---

## Table of Contents

1. [Program Structure](#program-structure)
2. [Types](#types)
3. [Variables](#variables)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Structs](#structs)
8. [Enums](#enums)
9. [Union Types](#union-types)
10. [Null](#null)
11. [Function References & Lambdas](#function-references--lambdas)
12. [Strings](#strings)
13. [Arrays](#arrays)
14. [Maps](#maps)
15. [For-In Iteration](#for-in-iteration)
16. [Match Statement](#match-statement)
17. [Exception Handling](#exception-handling)
18. [Module-Level Variables](#module-level-variables)
19. [Literals](#literals)

---

## Program Structure

Every BetterPython program requires a `main` function as its entry point:

```python
def main() -> int:
    print("Hello, World!")
    return 0
```

- Indentation-based blocks (like Python)
- Comments start with `#`
- All statements end with newline (no semicolons)
- Functions must be defined before `main` or before they are called

---

## Types

### Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | 64-bit signed integer | `42`, `-7`, `0xFF`, `0b1010` |
| `float` | 64-bit IEEE 754 double | `3.14`, `2.5e10`, `.5` |
| `str` | Immutable UTF-8 string | `"hello"`, `f"x={x}"` |
| `bool` | Boolean | `true`, `false` |

### Compound Types

| Type | Description | Example |
|------|-------------|---------|
| `[T]` | Array of type T | `[int]`, `[str]` |
| `{K: V}` | Map from K to V | `{str: int}` |
| `StructName` | User-defined struct | `Point`, `Node` |
| `UnionName` | Tagged union | `Shape`, `Result` |
| `fn(args) -> ret` | Function reference | `fn(int, int) -> int` |

### Special Values

| Value | Type | Description |
|-------|------|-------------|
| `null` | void | Null/empty value, assignable to reference types |
| `true` | bool | Boolean true |
| `false` | bool | Boolean false |

---

## Variables

### Declaration

```python
let x: int = 42
let name: str = "hello"
let pi: float = 3.14159
let active: bool = true
let nums: [int] = [1, 2, 3]
let ages: {str: int} = {"alice": 25}
```

### Assignment

```python
x = 100
name = "world"
```

### Compound Assignment

```python
x += 10    # x = x + 10
x -= 5     # x = x - 5
x *= 2     # x = x * 2
x /= 3     # x = x / 3
x %= 7     # x = x % 7
```

### Module-Level Variables (Globals)

```python
let counter: int = 0

def increment() -> void:
    counter += 1

def main() -> int:
    increment()
    print(counter)  # 1
    return 0
```

---

## Operators

### Arithmetic

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition / String concat | `3 + 4`, `"a" + "b"` |
| `-` | Subtraction / Negation | `10 - 3`, `-x` |
| `*` | Multiplication | `6 * 7` |
| `/` | Division (integer or float) | `10 / 3` |
| `%` | Modulo | `10 % 3` |

### Comparison

| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

String comparison uses lexicographic (strcmp) ordering: `"apple" < "banana"` is `true`.

### Logical

| Operator | Description |
|----------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |

### Bitwise

| Operator | Function Form | Description |
|----------|---------------|-------------|
| `&` | `bit_and(a, b)` | Bitwise AND |
| `\|` | `bit_or(a, b)` | Bitwise OR |
| `^` | `bit_xor(a, b)` | Bitwise XOR |
| `~` | `bit_not(a)` | Bitwise NOT |
| `<<` | `bit_shl(a, n)` | Shift left |
| `>>` | `bit_shr(a, n)` | Shift right |

---

## Control Flow

### If / Elif / Else

```python
if x > 0:
    print("positive")
elif x == 0:
    print("zero")
else:
    print("negative")
```

### While Loop

```python
let i: int = 0
while i < 10:
    print(i)
    i += 1
```

### For Loop (C-style)

```python
for let i: int = 0; i < 10; i += 1:
    print(i)
```

### For-In Loop

```python
# Iterate over array
let nums: [int] = [1, 2, 3]
for x in nums:
    print(x)

# Iterate over map keys
let ages: {str: int} = {"alice": 25, "bob": 30}
for name in ages:
    print(name)
```

### Break / Continue

```python
for let i: int = 0; i < 100; i += 1:
    if i % 2 == 0:
        continue
    if i > 10:
        break
    print(i)
```

---

## Functions

### Definition

```python
def add(a: int, b: int) -> int:
    return a + b

def greet(name: str) -> void:
    print("Hello " + name)
```

### Recursion

```python
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)
```

### Function References

```python
def square(x: int) -> int:
    return x * x

def main() -> int:
    let f: fn(int) -> int = &square
    print(f(5))  # 25
    return 0
```

### Dynamic Dispatch

Function references support runtime reassignment:

```python
def negate(x: int) -> int:
    return 0 - x

def double(x: int) -> int:
    return x * 2

def main() -> int:
    let f: fn(int) -> int = &negate
    print(f(5))   # -5
    f = &double
    print(f(5))   # 10
    return 0
```

### Lambda Expressions

```python
let add_one: fn(int) -> int = lambda(x: int) -> int: x + 1
let double: fn(int) -> int = lambda(x: int) -> int: x * 2
print(add_one(5))   # 6
print(double(10))   # 20
```

---

## Structs

### Definition

```python
struct Point:
    x: int
    y: int

struct Person:
    name: str
    age: int
```

### Construction

```python
let p: Point = Point{x: 10, y: 20}
let bob: Person = Person{name: "Bob", age: 30}
```

### Field Access

```python
print(p.x)       # 10
p.x = 50         # field assignment
print(p.x)       # 50
```

---

## Enums

```python
enum Color:
    RED
    GREEN
    BLUE

def main() -> int:
    let c: int = Color.RED    # 0
    let g: int = Color.GREEN  # 1
    let b: int = Color.BLUE   # 2
    return 0
```

---

## Union Types

Tagged unions (sum types) with variant constructors:

### Definition

```python
union Shape:
    Circle(radius: float)
    Rect(width: float, height: float)

union Result:
    Ok(value: int)
    Err(msg: str)
```

### Ergonomic Constructors

```python
# Auto-sets __tag and defaults missing fields
let c: Shape = Circle{radius: 3.14}
let r: Shape = Rect{width: 10.0, height: 5.0}
let ok: Result = Ok{value: 42}
let err: Result = Err{msg: "not found"}
```

### Manual Constructor (explicit __tag)

```python
let s: Shape = Shape{__tag: 0, radius: 2.5, width: 0.0, height: 0.0}
```

### Tag Access

```python
print(c.__tag)    # 0 (Circle)
print(r.__tag)    # 1 (Rect)
print(tag(c))     # "0"
```

### Pattern Matching on Unions

```python
match s.__tag:
    case 0:
        print("Circle with radius " + to_str(s.radius))
    case 1:
        print("Rect " + to_str(s.width) + "x" + to_str(s.height))
    default:
        print("Unknown shape")
```

### Implementation Details

Unions are flattened to structs with all variant fields plus `__tag`:
- `union Shape: Circle(radius: float) Rect(width: float, height: float)` becomes
- `struct Shape { __tag: int, radius: float, width: float, height: float }`
- `Circle{radius: 3.14}` rewrites to `Shape{__tag: 0, radius: 3.14, width: 0.0, height: 0.0}`

---

## Null

The `null` literal represents an absent or empty value.

### Null Assignment

Null can be assigned to reference types (str, arrays, maps, structs, unions, func refs):

```python
let s: str = null
let a: [int] = null
let n: Node = null
let f: fn(int) -> int = null
```

### Null Comparison

```python
if s == null:
    print("s is null")
if n != null:
    print("n has a value")
```

### Null Return

```python
def find(x: int) -> Node:
    if x > 0:
        return Node{value: x}
    return null
```

### typeof(null)

```python
typeof(null)  # "null"
```

---

## Function References & Lambdas

### Creating References

```python
let f: fn(int, int) -> int = &add        # Reference to named function
let g: fn(int) -> int = lambda(x: int) -> int: x * 2  # Lambda
```

### Calling Through References

```python
let result: int = f(3, 4)   # Calls add(3, 4)
let doubled: int = g(5)     # Returns 10
```

### typeof

```python
typeof(f)  # "func"
```

---

## Strings

### Literals

```python
let s: str = "hello world"
let escaped: str = "line1\nline2\ttab"
let quote: str = "say \"hi\""
```

### F-Strings (Interpolation)

```python
let name: str = "World"
let x: int = 42
let msg: str = f"Hello {name}, x={x}"
```

### Indexing

```python
let s: str = "hello"
let c: str = s[0]    # "h"
let last: str = s[-1] # "o" (negative indexing)
```

### Comparison

```python
"apple" < "banana"    # true (lexicographic)
"abc" == "abc"        # true
"z" > "a"             # true
```

### Concatenation

```python
let full: str = "Hello" + " " + "World"
```

---

## Arrays

### Literals

```python
let nums: [int] = [1, 2, 3, 4, 5]
let names: [str] = ["alice", "bob"]
let empty: [int] = array_fill(0, 0)  # typed empty array
```

### Indexing

```python
let first: int = nums[0]
let last: int = nums[-1]    # negative indexing
nums[0] = 99                # assignment
```

### Key Operations

```python
array_push(nums, 6)              # append
let popped: int = array_pop(nums) # remove last
array_insert(nums, 0, 0)         # insert at index
let removed: int = array_remove(nums, 0) # remove at index
array_sort(nums)                  # sort in-place
let sub: [int] = array_slice(nums, 1, 3) # slice
let combined: [int] = array_concat(a, b) # concatenate
```

---

## Maps

### Literals

```python
let ages: {str: int} = {"alice": 25, "bob": 30}
```

### Access

```python
let age: int = ages["alice"]   # 25
ages["charlie"] = 35           # insert/update
```

### Key Operations

```python
map_has_key(ages, "alice")     # true
let keys: [str] = map_keys(ages)
let vals: [int] = map_values(ages)
map_delete(ages, "bob")
```

---

## For-In Iteration

### Array Iteration

```python
let fruits: [str] = ["apple", "banana", "cherry"]
for fruit in fruits:
    print(fruit)
```

### Map Iteration

```python
let config: {str: str} = {"host": "localhost", "port": "8080"}
for key in config:
    print(key + ": " + config[key])
```

---

## Match Statement

Pattern matching on values:

```python
match x:
    case 0:
        print("zero")
    case 1:
        print("one")
    case 2:
        print("two")
    default:
        print("other")
```

### Match on Union Tags

```python
match shape.__tag:
    case 0:
        print("Circle")
    case 1:
        print("Rect")
    default:
        print("Unknown")
```

---

## Exception Handling

### Try / Catch / Finally

```python
try:
    let result: int = risky_operation()
    print(result)
catch e:
    print("Error: " + e)
finally:
    print("Cleanup done")
```

### Throw

```python
def divide(a: int, b: int) -> int:
    if b == 0:
        throw "Division by zero"
    return a / b
```

---

## Module-Level Variables

Variables declared at the top level (outside any function) are global:

```python
let counter: int = 0
let name: str = "default"

def increment() -> void:
    counter += 1

def main() -> int:
    counter = 10
    increment()
    print(counter)  # 11
    return 0
```

---

## Literals

### Integer Literals

```python
let decimal: int = 42
let negative: int = -7
let hex: int = 0xFF         # 255
let binary: int = 0b1010    # 10
```

### Float Literals

```python
let pi: float = 3.14159
let sci: float = 2.5e10
let small: float = 1e-5
let half: float = 0.5
```

### String Literals

```python
let simple: str = "hello"
let escaped: str = "tab\there\nnewline"
let fstr: str = f"value={x}"
```

### Boolean Literals

```python
let yes: bool = true
let no: bool = false
```

### Array Literals

```python
let ints: [int] = [1, 2, 3]
let strs: [str] = ["a", "b", "c"]
let nested: [[int]] = [[1, 2], [3, 4]]
```

### Map Literals

```python
let data: {str: int} = {"x": 1, "y": 2}
```

### Struct Literals

```python
let p: Point = Point{x: 10, y: 20}
```

---

## Type System Notes

- **Static typing**: All variables must have declared types
- **Type inference**: Compiler infers expression types for checking
- **No implicit coercion**: Use `int_to_float()`, `float_to_int()`, etc.
- **Null compatibility**: `null` is assignable to str, arrays, maps, structs, unions, func refs
- **Function signatures**: Checked at compile time including argument count and types

---

*Document Version: 2.0.0*
*Last Updated: February 2026*
*Maintained by: Claude (Anthropic)*
