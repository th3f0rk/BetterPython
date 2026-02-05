# BetterPython Tutorial

**A Step-by-Step Guide to Learning BetterPython**

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Hello World](#hello-world)
3. [Variables and Types](#variables-and-types)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Arrays](#arrays)
8. [Hash Maps](#hash-maps)
9. [Structs](#structs)
10. [Exception Handling](#exception-handling)
11. [File I/O](#file-io)
12. [Command Line Arguments](#command-line-arguments)
13. [Working with Floats](#working-with-floats)
14. [String Operations](#string-operations)
15. [Best Practices](#best-practices)

---

## Getting Started

### Installation

```bash
# Clone the repository
git clone https://github.com/5ingular1ty/BetterPython.git
cd BetterPython

# Build the compiler and VM
make

# Verify installation
./bin/betterpython --version
```

### Your First Program

Create a file called `hello.bp`:

```python
def main() -> int:
    print("Hello, BetterPython!")
    return 0
```

Run it:

```bash
./bin/betterpython hello.bp
```

Output:
```
Hello, BetterPython!
```

### Understanding the Structure

Every BetterPython program needs a `main` function:

```python
def main() -> int:
    # Your code here
    return 0  # 0 means success
```

- `def main()` - Declares the entry point
- `-> int` - The function returns an integer
- `return 0` - Return code (0 = success)

---

## Hello World

Let's break down the simplest program:

```python
def main() -> int:
    print("Hello, World!")
    return 0
```

**Key concepts:**
- Indentation matters (like Python)
- Strings use double quotes `""`
- `print()` outputs to the console
- Every statement is on its own line

### Printing Multiple Values

```python
def main() -> int:
    print("Name:")
    print("Alice")
    print("Age:")
    print(25)
    return 0
```

Output:
```
Name:
Alice
Age:
25
```

---

## Variables and Types

### Declaring Variables

Use `let` to declare variables with explicit types:

```python
def main() -> int:
    let name: str = "Alice"
    let age: int = 25
    let height: float = 1.75
    let active: bool = true

    print(name)
    print(age)
    print(height)
    print(active)
    return 0
```

### Basic Types

| Type | Description | Example |
|------|-------------|---------|
| `int` | 64-bit signed integer | `42`, `-17`, `0` |
| `float` | Double-precision float | `3.14`, `2.5e10` |
| `str` | String | `"hello"` |
| `bool` | Boolean | `true`, `false` |
| `void` | No value (for functions) | - |

### Fixed-Width Integer Types

For precise control over integer sizes:

```python
def main() -> int:
    let byte: u8 = 255       # 0 to 255
    let small: i16 = -32000  # -32768 to 32767
    let medium: u32 = 4000000000
    let large: i64 = 9223372036854775807
    return 0
```

| Type | Range |
|------|-------|
| `u8` | 0 to 255 |
| `i8` | -128 to 127 |
| `u16` | 0 to 65,535 |
| `i16` | -32,768 to 32,767 |
| `u32` | 0 to 4,294,967,295 |
| `i32` | -2,147,483,648 to 2,147,483,647 |
| `u64` | 0 to 18,446,744,073,709,551,615 |
| `i64` | Same as `int` |

### Reassigning Variables

```python
def main() -> int:
    let x: int = 10
    print(x)    # 10
    x = 20
    print(x)    # 20
    x = x + 5
    print(x)    # 25
    return 0
```

---

## Operators

### Arithmetic Operators

```python
def main() -> int:
    let a: int = 10
    let b: int = 3

    print(a + b)   # 13 (addition)
    print(a - b)   # 7  (subtraction)
    print(a * b)   # 30 (multiplication)
    print(a / b)   # 3  (integer division)
    print(a % b)   # 1  (modulo/remainder)
    return 0
```

### Comparison Operators

```python
def main() -> int:
    let x: int = 5
    let y: int = 10

    print(x == y)  # false (equal)
    print(x != y)  # true  (not equal)
    print(x < y)   # true  (less than)
    print(x <= y)  # true  (less or equal)
    print(x > y)   # false (greater than)
    print(x >= y)  # false (greater or equal)
    return 0
```

### Logical Operators

```python
def main() -> int:
    let a: bool = true
    let b: bool = false

    print(a and b)  # false
    print(a or b)   # true
    print(not a)    # false
    return 0
```

### Bitwise Operators

```python
def main() -> int:
    let x: int = 12  # 1100 in binary
    let y: int = 10  # 1010 in binary

    print(x & y)   # 8  (AND: 1000)
    print(x | y)   # 14 (OR:  1110)
    print(x ^ y)   # 6  (XOR: 0110)
    print(~x)      # -13 (NOT)
    print(x << 2)  # 48 (left shift)
    print(x >> 2)  # 3  (right shift)
    return 0
```

---

## Control Flow

### If-Elif-Else

```python
def main() -> int:
    let score: int = 85

    if score >= 90:
        print("A")
    elif score >= 80:
        print("B")
    elif score >= 70:
        print("C")
    else:
        print("F")

    return 0
```

### While Loops

```python
def main() -> int:
    let i: int = 0
    while i < 5:
        print(i)
        i = i + 1
    return 0
```

Output:
```
0
1
2
3
4
```

### For Loops

```python
def main() -> int:
    # range(start, end) - end is exclusive
    for i in range(0, 5):
        print(i)
    return 0
```

Output:
```
0
1
2
3
4
```

### Break and Continue

```python
def main() -> int:
    # Find first even number > 10
    for i in range(1, 100):
        if i <= 10:
            continue  # Skip numbers <= 10
        if i % 2 == 0:
            print("Found:")
            print(i)
            break     # Exit loop
    return 0
```

Output:
```
Found:
12
```

### Nested Loops

```python
def main() -> int:
    for i in range(1, 4):
        for j in range(1, 4):
            print(i * j)
    return 0
```

---

## Functions

### Defining Functions

```python
def greet(name: str) -> void:
    print("Hello,")
    print(name)

def main() -> int:
    greet("Alice")
    greet("Bob")
    return 0
```

### Functions with Return Values

```python
def square(n: int) -> int:
    return n * n

def add(a: int, b: int) -> int:
    return a + b

def main() -> int:
    let x: int = square(5)
    print(x)  # 25

    let sum: int = add(10, 20)
    print(sum)  # 30
    return 0
```

### Recursive Functions

```python
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)

def fibonacci(n: int) -> int:
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

def main() -> int:
    print(factorial(5))   # 120
    print(fibonacci(10))  # 55
    return 0
```

### Function Call Order

Functions must be defined before they are called:

```python
# Helper function defined first
def helper(x: int) -> int:
    return x * 2

# Main function uses helper
def main() -> int:
    print(helper(5))  # 10
    return 0
```

---

## Arrays

### Creating Arrays

```python
def main() -> int:
    # Array literal
    let numbers: [int] = [1, 2, 3, 4, 5]

    # Empty array
    let empty: [str] = []

    print(array_len(numbers))  # 5
    return 0
```

### Accessing Elements

```python
def main() -> int:
    let fruits: [str] = ["apple", "banana", "cherry"]

    print(fruits[0])  # apple
    print(fruits[1])  # banana
    print(fruits[2])  # cherry
    return 0
```

### Modifying Arrays

```python
def main() -> int:
    let nums: [int] = [10, 20, 30]

    # Change element
    nums[1] = 25
    print(nums[1])  # 25

    # Add element
    array_push(nums, 40)
    print(array_len(nums))  # 4

    # Remove last element
    let last: int = array_pop(nums)
    print(last)  # 40
    print(array_len(nums))  # 3

    return 0
```

### Iterating Over Arrays

```python
def main() -> int:
    let colors: [str] = ["red", "green", "blue"]

    for i in range(0, array_len(colors)):
        print(colors[i])

    return 0
```

### Array of Arrays (2D Arrays)

```python
def main() -> int:
    let matrix: [[int]] = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]

    for i in range(0, 3):
        for j in range(0, 3):
            print(matrix[i][j])

    return 0
```

---

## Hash Maps

### Creating Maps

```python
def main() -> int:
    # Map literal
    let ages: {str: int} = {"alice": 25, "bob": 30}

    # Access by key
    print(ages["alice"])  # 25

    return 0
```

### Modifying Maps

```python
def main() -> int:
    let scores: {str: int} = {}

    # Add entries
    scores["alice"] = 95
    scores["bob"] = 87
    scores["charlie"] = 92

    print(scores["alice"])  # 95

    # Update entry
    scores["bob"] = 90
    print(scores["bob"])  # 90

    return 0
```

### Map Operations

```python
def main() -> int:
    let data: {str: int} = {"a": 1, "b": 2, "c": 3}

    # Check if key exists
    if map_has_key(data, "a"):
        print("Found key 'a'")

    # Get all keys
    let keys: [str] = map_keys(data)
    for i in range(0, array_len(keys)):
        print(keys[i])

    # Get all values
    let values: [int] = map_values(data)
    for i in range(0, array_len(values)):
        print(values[i])

    # Delete entry
    map_delete(data, "b")
    print(map_len(data))  # 2

    return 0
```

---

## Structs

### Defining Structs

```python
struct Point:
    x: int
    y: int

struct Person:
    name: str
    age: int

def main() -> int:
    # Create struct instances
    let p: Point = Point{x: 10, y: 20}
    let alice: Person = Person{name: "Alice", age: 25}

    # Access fields
    print(p.x)       # 10
    print(p.y)       # 20
    print(alice.name)  # Alice
    print(alice.age)   # 25

    return 0
```

### Modifying Struct Fields

```python
struct Counter:
    value: int

def main() -> int:
    let c: Counter = Counter{value: 0}

    c.value = c.value + 1
    print(c.value)  # 1

    c.value = c.value + 1
    print(c.value)  # 2

    return 0
```

### Structs with Functions

```python
struct Rectangle:
    width: int
    height: int

def area(r: Rectangle) -> int:
    return r.width * r.height

def perimeter(r: Rectangle) -> int:
    return 2 * (r.width + r.height)

def main() -> int:
    let rect: Rectangle = Rectangle{width: 10, height: 5}

    print(area(rect))       # 50
    print(perimeter(rect))  # 30

    return 0
```

### Nested Structs

```python
struct Address:
    city: str
    zip: str

struct Employee:
    name: str
    address: Address

def main() -> int:
    let addr: Address = Address{city: "Boston", zip: "02101"}
    let emp: Employee = Employee{name: "Alice", address: addr}

    print(emp.name)
    print(emp.address.city)

    return 0
```

---

## Exception Handling

### Try-Catch

```python
def divide(a: int, b: int) -> int:
    if b == 0:
        throw "Division by zero"
    return a / b

def main() -> int:
    try:
        let result: int = divide(10, 0)
        print(result)
    catch e:
        print("Error:")
        print(e)
    return 0
```

Output:
```
Error:
Division by zero
```

### Try-Catch-Finally

```python
def risky_operation() -> int:
    throw "Something went wrong"
    return 42

def main() -> int:
    try:
        let x: int = risky_operation()
        print(x)
    catch e:
        print("Caught error")
    finally:
        print("Cleanup complete")
    return 0
```

Output:
```
Caught error
Cleanup complete
```

### Propagating Exceptions

```python
def inner() -> void:
    throw "Inner error"

def middle() -> void:
    inner()

def main() -> int:
    try:
        middle()
    catch e:
        print("Caught in main:")
        print(e)
    return 0
```

---

## File I/O

### Reading Files

```python
def main() -> int:
    # Read entire file
    let content: str = file_read("data.txt")
    print(content)

    # Check if file exists first
    if file_exists("data.txt"):
        let data: str = file_read("data.txt")
        print(data)
    else:
        print("File not found")

    return 0
```

### Writing Files

```python
def main() -> int:
    # Write to file (overwrites)
    file_write("output.txt", "Hello, World!")

    # Append to file
    file_append("log.txt", "New log entry\n")

    print("Done!")
    return 0
```

### File Information

```python
def main() -> int:
    let path: str = "test.txt"

    if file_exists(path):
        let size: int = file_size(path)
        print("File size:")
        print(size)

    return 0
```

### Copying and Deleting

```python
def main() -> int:
    # Copy a file
    file_copy("source.txt", "backup.txt")

    # Delete a file
    file_delete("temp.txt")

    return 0
```

---

## Command Line Arguments

### Using argv() and argc()

```python
def main() -> int:
    let args: [str] = argv()
    let count: int = argc()

    print("Number of arguments:")
    print(count)

    print("Arguments:")
    for i in range(0, count):
        print(args[i])

    return 0
```

Run:
```bash
./bin/betterpython args.bp hello world 123
```

Output:
```
Number of arguments:
3
Arguments:
hello
world
123
```

### Processing Arguments

```python
def main() -> int:
    let args: [str] = argv()
    let count: int = argc()

    if count < 2:
        print("Usage: program <name>")
        return 1

    print("Hello,")
    print(args[0])

    return 0
```

---

## Working with Floats

### Float Basics

```python
def main() -> int:
    let pi: float = 3.14159
    let e: float = 2.71828
    let negative: float = -123.456

    # Scientific notation
    let big: float = 1.5e10    # 15 billion
    let small: float = 2.5e-5  # 0.000025

    print(pi)
    return 0
```

### Float Arithmetic

```python
def main() -> int:
    let a: float = 10.5
    let b: float = 3.2

    let sum: float = a + b
    let diff: float = a - b
    let prod: float = a * b
    let quot: float = a / b

    print(sum)   # 13.7
    print(diff)  # 7.3
    print(prod)  # 33.6
    print(quot)  # 3.28125

    return 0
```

### Math Functions

```python
def main() -> int:
    let x: float = 0.5

    # Trigonometry
    print(sin(x))
    print(cos(x))
    print(tan(x))

    # Inverse trig
    print(asin(x))
    print(acos(x))
    print(atan(x))

    # Logarithms
    let y: float = 100.0
    print(log(y))    # Natural log
    print(log10(y))  # Log base 10
    print(log2(y))   # Log base 2

    # Other
    print(fsqrt(16.0))      # 4.0
    print(fpow(2.0, 10.0))  # 1024.0
    print(fabs(-5.5))       # 5.5

    return 0
```

### Conversion

```python
def main() -> int:
    # Int to float
    let i: int = 42
    let f: float = int_to_float(i)
    print(f)  # 42.0

    # Float to int (truncates)
    let pi: float = 3.14159
    let n: int = float_to_int(pi)
    print(n)  # 3

    # Float to string
    let s: str = float_to_str(pi)
    print(s)

    # String to float
    let parsed: float = str_to_float("2.718")
    print(parsed)

    return 0
```

### Special Values

```python
def main() -> int:
    let x: float = 1.0 / 0.0  # Infinity

    if is_inf(x):
        print("x is infinity")

    let y: float = 0.0 / 0.0  # NaN

    if is_nan(y):
        print("y is NaN")

    return 0
```

---

## String Operations

### Basic String Functions

```python
def main() -> int:
    let s: str = "Hello, World!"

    print(len(s))              # 13
    print(str_upper(s))        # HELLO, WORLD!
    print(str_lower(s))        # hello, world!
    print(substr(s, 0, 5))     # Hello
    print(str_reverse(s))      # !dlroW ,olleH

    return 0
```

### Searching Strings

```python
def main() -> int:
    let s: str = "Hello, World!"

    print(str_contains(s, "World"))  # true
    print(str_find(s, "World"))      # 7
    print(starts_with(s, "Hello"))   # true
    print(ends_with(s, "!"))         # true

    return 0
```

### Modifying Strings

```python
def main() -> int:
    let s: str = "  Hello  "
    print(str_trim(s))  # "Hello"

    let r: str = str_replace("hello world", "world", "BetterPython")
    print(r)  # "hello BetterPython"

    let repeated: str = str_repeat("ab", 3)
    print(repeated)  # "ababab"

    return 0
```

### Splitting and Joining

```python
def main() -> int:
    # Split
    let csv: str = "apple,banana,cherry"
    let parts: [str] = str_split(csv, ",")

    for i in range(0, array_len(parts)):
        print(parts[i])

    # Join
    let words: [str] = ["one", "two", "three"]
    let joined: str = str_join(words, "-")
    print(joined)  # "one-two-three"

    return 0
```

### Character Operations

```python
def main() -> int:
    let s: str = "Hello"

    # Get character at index
    let c: str = str_char_at(s, 0)
    print(c)  # "H"

    # Character to ASCII code
    print(ord("A"))  # 65

    # ASCII code to character
    print(chr(65))  # "A"

    return 0
```

---

## Best Practices

### 1. Use Meaningful Names

```python
# Good
let user_count: int = 0
let max_retries: int = 3

def calculate_total(items: [int]) -> int:
    ...

# Bad
let x: int = 0
let n: int = 3

def calc(a: [int]) -> int:
    ...
```

### 2. Handle Errors Gracefully

```python
def safe_divide(a: int, b: int) -> int:
    if b == 0:
        throw "Cannot divide by zero"
    return a / b

def main() -> int:
    try:
        let result: int = safe_divide(10, 0)
        print(result)
    catch e:
        print("Error occurred:")
        print(e)
    return 0
```

### 3. Document Your Functions

```python
# Calculates factorial of n
# Returns n! (n * (n-1) * ... * 1)
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)
```

### 4. Validate Input

```python
def main() -> int:
    let args: [str] = argv()
    let count: int = argc()

    if count < 1:
        print("Error: No input file specified")
        return 1

    let filename: str = args[0]
    if not file_exists(filename):
        print("Error: File not found")
        return 1

    # Process file...
    return 0
```

### 5. Use Functions for Reusable Logic

```python
# Instead of duplicating code:
def format_name(first: str, last: str) -> str:
    return first + " " + last

def main() -> int:
    print(format_name("Alice", "Smith"))
    print(format_name("Bob", "Jones"))
    return 0
```

### 6. Keep Functions Small

```python
# Good: Small, focused functions
def is_even(n: int) -> bool:
    return n % 2 == 0

def sum_evens(numbers: [int]) -> int:
    let total: int = 0
    for i in range(0, array_len(numbers)):
        if is_even(numbers[i]):
            total = total + numbers[i]
    return total
```

### 7. Clean Up Resources

```python
def main() -> int:
    try:
        # Open resources
        let data: str = file_read("data.txt")
        # Process...
    catch e:
        print("Error processing file")
    finally:
        # Cleanup always runs
        print("Cleanup complete")
    return 0
```

---

## Next Steps

1. **Read the API Reference** - See `docs/API_REFERENCE.md` for all built-in functions
2. **Explore Examples** - Check the `examples/` folder for more complex programs
3. **Try the REPL** - Use `./bin/bprepl` for interactive experimentation
4. **Read the Architecture** - See `docs/ARCHITECTURE.md` to understand how BetterPython works

---

## Getting Help

- **Documentation:** See the `docs/` folder
- **Examples:** Browse `examples/` for working code
- **Issues:** Report bugs at https://github.com/5ingular1ty/BetterPython/issues

---

**Happy coding with BetterPython!**
