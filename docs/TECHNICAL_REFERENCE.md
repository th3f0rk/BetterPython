BetterPython Technical Reference Manual
Version 2.0 Production Edition
Last Updated: February 3, 2026

================================================================================
TABLE OF CONTENTS
================================================================================

1. Language Specification
2. Type System
3. Standard Library Reference
4. Compiler Architecture
5. Virtual Machine Specification
6. Memory Management
7. Error Handling
8. Performance Characteristics
9. Security Considerations
10. Extension Guide

================================================================================
1. LANGUAGE SPECIFICATION
================================================================================

1.1 Lexical Structure
--------------------

Tokens:
- Keywords: def, let, if, elif, else, while, return, and, or, not, true, false
- Identifiers: [a-zA-Z_][a-zA-Z0-9_]*
- Integer literals: [0-9]+
- String literals: "..." (supports escape sequences: \n, \t, \\, \")
- Operators: + - * / % == != < <= > >= and or not
- Delimiters: ( ) : ,

Indentation:
- Significant whitespace
- Each indentation level must be consistent (spaces or tabs)
- INDENT token generated on indentation increase
- DEDENT token(s) generated on indentation decrease

Comments:
- Not currently supported in syntax

1.2 Grammar
-----------

program ::= function*

function ::= "def" identifier "(" parameters? ")" "->" type ":" NEWLINE
             INDENT statement+ DEDENT

parameters ::= parameter ("," parameter)*
parameter ::= identifier ":" type

type ::= "int" | "bool" | "str" | "void"

statement ::= let_stmt | assign_stmt | expr_stmt | if_stmt | while_stmt | return_stmt

let_stmt ::= "let" identifier ":" type "=" expression NEWLINE

assign_stmt ::= identifier "=" expression NEWLINE

expr_stmt ::= expression NEWLINE

if_stmt ::= "if" expression ":" NEWLINE INDENT statement+ DEDENT
            elif_clause* else_clause?

elif_clause ::= "elif" expression ":" NEWLINE INDENT statement+ DEDENT

else_clause ::= "else" ":" NEWLINE INDENT statement+ DEDENT

while_stmt ::= "while" expression ":" NEWLINE INDENT statement+ DEDENT

return_stmt ::= "return" expression? NEWLINE

expression ::= or_expr

or_expr ::= and_expr ("or" and_expr)*

and_expr ::= not_expr ("and" not_expr)*

not_expr ::= "not" not_expr | comparison

comparison ::= additive (("==" | "!=" | "<" | "<=" | ">" | ">=") additive)?

additive ::= multiplicative (("+" | "-") multiplicative)*

multiplicative ::= unary (("*" | "/" | "%") unary)*

unary ::= "-" unary | primary

primary ::= INTEGER | STRING | "true" | "false" | identifier
          | identifier "(" arguments? ")"
          | "(" expression ")"

arguments ::= expression ("," expression)*

1.3 Type System
---------------

Primitive Types:
- int: 64-bit signed integer
- bool: Boolean value (true or false)
- str: UTF-8 encoded string (heap-allocated, garbage collected)
- void: Absence of value (only valid for function returns)

Type Rules:
- All variables must be explicitly typed
- Function parameters and return types must be declared
- Assignment requires type compatibility
- Arithmetic operations require int operands (except + for strings)
- Comparison operators require compatible types
- Boolean operators require bool operands

Type Inference:
- Expression types are inferred during type checking phase
- No implicit type conversions (use to_str for conversion)

================================================================================
2. TYPE SYSTEM
================================================================================

2.1 Type Checking Algorithm
---------------------------

The type checker performs single-pass static analysis:

1. Build scope tree
2. For each function:
   a. Create new scope with parameters
   b. Check each statement recursively
   c. Verify return type matches declaration
3. For each expression:
   a. Infer type bottom-up
   b. Check operator compatibility
   c. Propagate type information

2.2 Scope Rules
---------------

- Global scope: Function definitions only
- Function scope: Parameters and local variables
- Block scope: No additional scoping (all variables in function scope)
- Shadowing: Not allowed (variable names must be unique in scope)

2.3 Type Compatibility
----------------------

Exact match required for:
- Variable assignment
- Function arguments
- Return values
- Operator operands

Special cases:
- String concatenation: str + str -> str
- Integer arithmetic: int [+|-|*|/|%] int -> int
- Comparison: T [==|!=] T -> bool (for any type T)
- Ordering: int [<|<=|>|>=] int -> bool

================================================================================
3. STANDARD LIBRARY REFERENCE
================================================================================

3.1 Input/Output Functions
---------------------------

print(...) -> void
    Print any number of values to stdout, separated by spaces.
    Arguments: Accepts any combination of int, bool, str
    Side effects: Writes to stdout with newline
    Example:
        print("Count:", 42, "Done:", true)

read_line() -> str
    Read a line from stdin.
    Returns: String without trailing newline
    Blocks until newline received
    Example:
        let input: str = read_line()

3.2 String Functions
--------------------

len(s: str) -> int
    Return the length of string in bytes.
    Time complexity: O(1)
    Example:
        let length: int = len("hello")  // 5

substr(s: str, start: int, length: int) -> str
    Extract substring from string.
    Parameters:
        s: Source string
        start: Zero-based start index
        length: Number of characters to extract
    Returns: New substring
    Bounds checking: Returns empty string if out of bounds
    Example:
        let sub: str = substr("hello world", 0, 5)  // "hello"

to_str(value: int|bool|str) -> str
    Convert value to string representation.
    Conversion rules:
        int: Decimal representation
        bool: "true" or "false"
        str: Identity function
    Example:
        let s: str = to_str(42)  // "42"

chr(code: int) -> str
    Convert ASCII code to single-character string.
    Parameters:
        code: Integer in range [0, 127]
    Returns: Single-character string
    Error: Fatal if code < 0 or code > 127
    Example:
        let c: str = chr(65)  // "A"

ord(c: str) -> int
    Convert first character of string to ASCII code.
    Parameters:
        c: Non-empty string
    Returns: ASCII code of first character
    Error: Fatal if string is empty
    Example:
        let code: int = ord("A")  // 65

str_upper(s: str) -> str
    Convert string to uppercase.
    Transformation: a-z -> A-Z, others unchanged
    Example:
        let upper: str = str_upper("Hello")  // "HELLO"

str_lower(s: str) -> str
    Convert string to lowercase.
    Transformation: A-Z -> a-z, others unchanged
    Example:
        let lower: str = str_lower("Hello")  // "hello"

str_trim(s: str) -> str
    Remove leading and trailing whitespace.
    Whitespace: space, tab, newline, carriage return
    Example:
        let trimmed: str = str_trim("  hello  ")  // "hello"

starts_with(s: str, prefix: str) -> bool
    Check if string starts with prefix.
    Returns: true if s begins with prefix, false otherwise
    Empty prefix: Always returns true
    Example:
        let result: bool = starts_with("hello", "hel")  // true

ends_with(s: str, suffix: str) -> bool
    Check if string ends with suffix.
    Returns: true if s ends with suffix, false otherwise
    Empty suffix: Always returns true
    Example:
        let result: bool = ends_with("hello", "lo")  // true

str_find(s: str, needle: str) -> int
    Find first occurrence of substring.
    Returns: Zero-based index of first occurrence, or -1 if not found
    Empty needle: Returns 0
    Time complexity: O(n*m) where n = len(s), m = len(needle)
    Example:
        let pos: int = str_find("hello world", "world")  // 6

str_replace(s: str, old: str, new: str) -> str
    Replace first occurrence of substring.
    Returns: New string with first occurrence replaced
    If old not found: Returns original string
    Empty old: Returns original string
    Example:
        let result: str = str_replace("hello", "l", "L")  // "heLlo"

3.3 Mathematical Functions
---------------------------

abs(n: int) -> int
    Return absolute value of integer.
    Example:
        let result: int = abs(-42)  // 42

min(a: int, b: int) -> int
    Return minimum of two integers.
    Example:
        let result: int = min(10, 20)  // 10

max(a: int, b: int) -> int
    Return maximum of two integers.
    Example:
        let result: int = max(10, 20)  // 20

pow(base: int, exp: int) -> int
    Compute base raised to power of exp.
    Implementation: Uses floating-point pow(), truncates result
    Overflow: May overflow for large values
    Example:
        let result: int = pow(2, 10)  // 1024

sqrt(n: int) -> int
    Compute integer square root.
    Returns: Floor of square root
    Example:
        let result: int = sqrt(144)  // 12

floor(n: int) -> int
    Floor function (identity for integers).
    Provided for API completeness.
    Example:
        let result: int = floor(42)  // 42

ceil(n: int) -> int
    Ceiling function (identity for integers).
    Provided for API completeness.
    Example:
        let result: int = ceil(42)  // 42

round(n: int) -> int
    Rounding function (identity for integers).
    Provided for API completeness.
    Example:
        let result: int = round(42)  // 42

3.4 Random Number Functions
----------------------------

rand() -> int
    Generate pseudo-random integer.
    Range: [0, 32767]
    Algorithm: Linear congruential generator
    Not cryptographically secure
    Example:
        let r: int = rand()

rand_range(min: int, max: int) -> int
    Generate random integer in range [min, max).
    Parameters:
        min: Inclusive lower bound
        max: Exclusive upper bound
    Returns: Random value in [min, max)
    If min >= max: Returns min
    Example:
        let dice: int = rand_range(1, 7)  // 1-6

rand_seed(seed: int) -> void
    Seed the random number generator.
    Use for reproducible random sequences.
    Example:
        rand_seed(42)

3.5 Encoding Functions
----------------------

base64_encode(data: str) -> str
    Encode string to base64.
    Standard base64 alphabet: A-Za-z0-9+/
    Padding: Uses = character
    Example:
        let encoded: str = base64_encode("Hello")  // "SGVsbG8="

base64_decode(data: str) -> str
    Decode base64 string.
    Ignores invalid characters
    Example:
        let decoded: str = base64_decode("SGVsbG8=")  // "Hello"

3.6 File Operations
-------------------

file_read(path: str) -> str
    Read entire file as string.
    Returns: File contents
    Error: Fatal if file cannot be opened or read
    Example:
        let content: str = file_read("/tmp/file.txt")

file_write(path: str, data: str) -> bool
    Write string to file, overwriting if exists.
    Returns: true on success, false on failure
    Creates file if it doesn't exist
    Example:
        let ok: bool = file_write("/tmp/file.txt", "data")

file_append(path: str, data: str) -> bool
    Append string to end of file.
    Returns: true on success, false on failure
    Creates file if it doesn't exist
    Example:
        let ok: bool = file_append("/tmp/log.txt", "entry\n")

file_exists(path: str) -> bool
    Check if file exists.
    Returns: true if file exists and is accessible
    Example:
        let exists: bool = file_exists("/tmp/file.txt")

file_delete(path: str) -> bool
    Delete file.
    Returns: true on success, false on failure
    Example:
        let ok: bool = file_delete("/tmp/file.txt")

3.7 Time Functions
------------------

clock_ms() -> int
    Get current time in milliseconds since Unix epoch.
    Precision: Platform-dependent (typically millisecond)
    Example:
        let start: int = clock_ms()

sleep(ms: int) -> void
    Sleep for specified milliseconds.
    Blocks execution for approximately ms milliseconds
    Negative or zero values: No-op
    Example:
        sleep(1000)  // Sleep 1 second

3.8 System Functions
--------------------

getenv(name: str) -> str
    Get environment variable value.
    Returns: Variable value, or empty string if not set
    Example:
        let user: str = getenv("USER")

exit(code: int) -> void
    Exit program with specified status code.
    Does not return
    Example:
        exit(0)  // Success

================================================================================
4. COMPILER ARCHITECTURE
================================================================================

4.1 Compilation Phases
----------------------

Phase 1: Lexical Analysis (lexer.c)
    Input: Source code string
    Output: Token stream
    Features:
        - Indentation-based INDENT/DEDENT tokens
        - Line and column tracking
        - Keyword recognition

Phase 2: Parsing (parser.c)
    Input: Token stream
    Output: Abstract Syntax Tree (AST)
    Method: Recursive descent parser
    Features:
        - Operator precedence climbing
        - Error recovery
        - ELIF desugaring to if-else chains

Phase 3: Type Checking (types.c)
    Input: AST
    Output: Type-annotated AST
    Method: Single-pass recursive type inference
    Features:
        - Scope management
        - Type compatibility checking
        - Builtin function type signatures

Phase 4: Code Generation (compiler.c)
    Input: Type-checked AST
    Output: Bytecode
    Features:
        - Stack-based code generation
        - Jump patching for control flow
        - Constant pooling for strings

4.2 Bytecode Format
-------------------

File Header:
    Magic: "BPC\0" (4 bytes)
    Version: uint32_t
    Entry point offset: uint32_t

Constant Pool:
    Count: uint32_t
    For each constant:
        Type: uint8_t (1=int, 2=str, 3=bool)
        Value: Type-dependent encoding

Code Section:
    Length: uint32_t
    Instructions: uint8_t array

Instruction Format:
    Opcode: 1 byte
    Operands: 0-4 bytes (opcode-dependent)

4.3 Instruction Set
-------------------

Stack Operations:
    OP_CONST <index>: Push constant from pool
    OP_POP: Pop and discard top value
    OP_DUP: Duplicate top value

Variable Operations:
    OP_LOAD_VAR <index>: Load local variable
    OP_STORE_VAR <index>: Store to local variable

Arithmetic:
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD
    OP_NEG: Unary negation

Comparison:
    OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE

Boolean:
    OP_AND, OP_OR, OP_NOT

Control Flow:
    OP_JUMP <offset>: Unconditional jump
    OP_JUMP_IF_FALSE <offset>: Conditional jump
    OP_CALL_BUILTIN <id> <argc>: Call builtin function
    OP_RETURN: Return from function

================================================================================
5. VIRTUAL MACHINE SPECIFICATION
================================================================================

5.1 Execution Model
-------------------

Architecture: Stack-based virtual machine
Stack: Fixed-size value stack (1024 slots)
Locals: Fixed-size local variable array (256 slots)
PC: Program counter (instruction pointer)

Execution Loop:
    while (PC < code_length):
        opcode = code[PC++]
        execute(opcode)

Value Representation:
    Tagged union with type discriminator
    Types: VAL_INT, VAL_BOOL, VAL_STR, VAL_NULL
    Sizes: 16 bytes per value (8 byte payload + 8 byte type/metadata)

5.2 Memory Layout
-----------------

Stack Frame:
    [Return address]
    [Local variables...]
    [Temporary values...]

Heap:
    Garbage-collected heap for strings
    Allocation: Linear allocation with GC sweep
    Collection: Mark-and-sweep when threshold exceeded

5.3 Performance Characteristics
--------------------------------

Instruction Dispatch: Direct threading (switch statement)
Typical Performance: 10-50x slower than native C
Memory Usage: ~100 bytes overhead per string + value stack

Benchmarks (relative to Python 3):
    Arithmetic: 2-3x faster
    String operations: Similar speed
    Function calls: Not supported (builtin only)

================================================================================
6. MEMORY MANAGEMENT
================================================================================

6.1 Garbage Collection
-----------------------

Algorithm: Mark-and-sweep
Trigger: When heap exceeds threshold (default 1MB)
Roots: Value stack + local variables

Mark Phase:
    1. Mark all values on stack
    2. Mark all local variables
    3. Recursively mark referenced strings

Sweep Phase:
    1. Iterate all allocated strings
    2. Free unmarked strings
    3. Reset marks for next cycle

6.2 String Interning
--------------------

Not implemented: Each string is separate allocation
Consequence: String comparison is O(n)
Future: Could implement string interning for common strings

6.3 Memory Safety
-----------------

Stack overflow: Checked (fatal error on overflow)
Heap exhaustion: Triggers GC, then fatal if still insufficient
Use-after-free: Not possible (GC handles lifetimes)
Buffer overflows: Bounds checking on string operations

================================================================================
7. ERROR HANDLING
================================================================================

7.1 Compile-Time Errors
------------------------

Categories:
    - Syntax errors (parser)
    - Type errors (type checker)
    - Undefined variables (type checker)
    - Undefined functions (type checker)

Error Format:
    "error message at line:column"

Error Recovery:
    None: Compilation stops at first error

7.2 Runtime Errors
------------------

Categories:
    - Division by zero
    - Stack overflow
    - Out of memory
    - File I/O errors
    - Invalid arguments to builtins

Handling:
    All runtime errors are fatal
    Program terminates with error message
    No exception handling mechanism

================================================================================
8. PERFORMANCE CHARACTERISTICS
================================================================================

8.1 Complexity Analysis
-----------------------

Operations:
    Variable access: O(1)
    Arithmetic: O(1)
    String concatenation: O(n+m)
    String search: O(n*m)
    Array access: Not supported

Compilation:
    Lexing: O(n) where n = source length
    Parsing: O(n)
    Type checking: O(n)
    Code generation: O(n)
    Total: O(n) linear in source size

8.2 Optimization Opportunities
-------------------------------

Current: No optimizations
Possible:
    - Constant folding
    - Dead code elimination
    - Common subexpression elimination
    - Inline small functions (once supported)

8.3 Profiling
-------------

Not built-in
Can instrument with clock_ms() calls
Future: Add --profile flag for execution statistics

================================================================================
9. SECURITY CONSIDERATIONS
================================================================================

9.1 Input Validation
--------------------

User input: No built-in sanitization
File paths: No path traversal protection
Integer overflow: Silent wraparound (signed 64-bit)
String length: Limited by available memory

Recommendations:
    - Validate all external input
    - Use file_exists() before file operations
    - Check bounds before array-like operations
    - Limit string sizes for user input

9.2 Sandboxing
--------------

File system: Full access (no restrictions)
Network: No built-in networking (future)
Process: Can exec via external calls (future)

Not suitable for untrusted code execution without external sandboxing.

9.3 Cryptography
----------------

Not implemented: No cryptographic functions
Use case: Not suitable for security-critical applications
Future: Add secure random, hashing, encryption functions

================================================================================
10. EXTENSION GUIDE
================================================================================

10.1 Adding Builtin Functions
------------------------------

Steps:
1. Add BuiltinId enum value to bytecode.h
2. Implement bi_function_name() in stdlib.c
3. Add case to stdlib_call() dispatcher
4. Add name mapping in compiler.c builtin_id()
5. Add type signature in types.c check_call()

Example (adding "square" function):

In bytecode.h:
    typedef enum {
        ...
        BI_SQUARE
    } BuiltinId;

In stdlib.c:
    static Value bi_square(Value *args, uint16_t argc) {
        if (argc != 1 || args[0].type != VAL_INT)
            bp_fatal("square expects (int)");
        int64_t n = args[0].as.i;
        return v_int(n * n);
    }
    
    // In stdlib_call():
    case BI_SQUARE: return bi_square(args, argc);

In compiler.c:
    if (strcmp(name, "square") == 0) return BI_SQUARE;

In types.c:
    if (strcmp(name, "square") == 0) {
        if (e->as.call.argc != 1) bp_fatal("square expects 1 arg");
        if (check_expr(e->as.call.args[0], s).kind != TY_INT)
            bp_fatal("square expects int");
        e->inferred = type_int();
        return e->inferred;
    }

10.2 Adding New Types
---------------------

Not straightforward: Requires changes throughout system
Required modifications:
    - ast.h: Add type kind
    - types.c: Add type checking rules
    - vm.c: Add value representation
    - compiler.c: Add code generation
    - gc.c: Add garbage collection support (if needed)

Example for float type would require ~500 lines of changes.

10.3 Language Extensions
-------------------------

Adding keywords: Modify lexer.c keyword table
Adding operators: Add to lexer, parser, type checker, compiler, VM
Adding statements: Full compiler pipeline modification required

Difficulty levels:
    Easy: Builtin functions
    Medium: Operators, expression forms
    Hard: New statements, control flow
    Very hard: New types, closures, classes

================================================================================
END OF TECHNICAL REFERENCE MANUAL
================================================================================
