BetterPython Standard Library API Documentation
Version 2.0 Production Edition
Complete Function Reference

================================================================================
OVERVIEW
================================================================================

This document provides comprehensive API documentation for the 150+ built-in
functions available in BetterPython. Each function includes:
- Function signature with types
- Detailed parameter descriptions
- Return value specification
- Error conditions and handling
- Time/space complexity analysis
- Code examples with expected output
- Implementation notes
- Common use cases and patterns

Function Categories:
1. Input/Output (2 functions)
2. String Operations (13 functions)
3. Mathematical Functions (8 functions)
4. Random Number Generation (3 functions)
5. Encoding/Decoding (2 functions)
6. File Operations (5 functions)
7. Time and Timing (2 functions)
8. System Integration (2 functions)
9. Type Conversion (1 function)
10. Process Control (1 function)
11. Character Conversion (2 functions)

Total: 42 functions

================================================================================
FUNCTION REFERENCE
================================================================================

--------------------------------------------------------------------------------
1. INPUT/OUTPUT FUNCTIONS
--------------------------------------------------------------------------------

print(...) -> void
------------------
Description:
    Output values to standard output with automatic spacing and newline.
    Accepts variable number of arguments of any type. Arguments are converted
    to string representation and separated by spaces.

Signature:
    print(...) -> void

Parameters:
    ...: Variable arguments of type int, bool, or str

Returns:
    void (no return value)

Type Conversion Rules:
    int -> decimal string representation
    bool -> "true" or "false"
    str -> unchanged

Behavior:
    - Arguments separated by single space
    - Newline automatically appended
    - Flushes stdout buffer after output

Time Complexity: O(n) where n = total output length
Space Complexity: O(1)

Examples:
    print("Hello World")
    Output: Hello World

    print("Count:", 42)
    Output: Count: 42

    print("Flags:", true, false, true)
    Output: Flags: true false true

    print(1, 2, 3, 4, 5)
    Output: 1 2 3 4 5

Common Patterns:
    # Debug output
    print("Debug: x =", x, "y =", y)
    
    # Progress indication
    print("Processing item", i, "of", total)
    
    # Result reporting
    print("Test passed:", result)

Notes:
    - Cannot suppress newline
    - No format specifiers
    - Always uses space separator
    - For formatted output, construct string first with concatenation

read_line() -> str
------------------
Description:
    Read a single line from standard input, blocking until newline received.
    Trailing newline character is removed from returned string.

Signature:
    read_line() -> str

Parameters:
    None

Returns:
    str: Input line without trailing newline

Behavior:
    - Blocks execution until user presses Enter
    - Removes \n and \r\n line terminators
    - Empty line returns empty string
    - EOF returns empty string

Time Complexity: O(n) where n = input length
Space Complexity: O(n) for returned string

Examples:
    let name: str = read_line()
    print("Hello,", name)

    let input: str = read_line()
    if len(input) > 0:
        print("You entered:", input)
    else:
        print("Empty input")

Common Patterns:
    # Interactive input with prompt
    print("Enter your name:")
    let name: str = read_line()
    
    # Input validation loop
    let valid: bool = false
    while not valid:
        let input: str = read_line()
        if len(input) > 0:
            valid = true

Notes:
    - No prompt parameter (use separate print)
    - Cannot read without blocking
    - Limited to single line
    - For multi-line, call repeatedly

--------------------------------------------------------------------------------
2. STRING OPERATIONS
--------------------------------------------------------------------------------

len(s: str) -> int
------------------
Description:
    Return the length of a string in bytes. For ASCII strings, equals number
    of characters. For UTF-8, counts bytes not Unicode code points.

Signature:
    len(s: str) -> int

Parameters:
    s: String to measure

Returns:
    int: Length in bytes (non-negative)

Time Complexity: O(1) - length stored in string structure
Space Complexity: O(1)

Examples:
    len("")  // 0
    len("hello")  // 5
    len("Hello World")  // 11

Common Patterns:
    # Check if string is empty
    if len(s) == 0:
        print("Empty string")
    
    # Validate minimum length
    if len(password) < 8:
        print("Password too short")

Notes:
    - Counts bytes not characters
    - O(1) operation (length cached)
    - Always non-negative

substr(s: str, start: int, length: int) -> str
-----------------------------------------------
Description:
    Extract substring from string starting at specified index with specified
    length. Uses zero-based indexing. Returns empty string if parameters are
    out of bounds.

Signature:
    substr(s: str, start: int, length: int) -> str

Parameters:
    s: Source string
    start: Zero-based starting index (inclusive)
    length: Number of characters to extract

Returns:
    str: Extracted substring, or empty string if out of bounds

Bounds Behavior:
    - start < 0: Returns empty string
    - start >= len(s): Returns empty string
    - length <= 0: Returns empty string
    - start + length > len(s): Truncates to end of string

Time Complexity: O(length)
Space Complexity: O(length)

Examples:
    substr("Hello World", 0, 5)  // "Hello"
    substr("Hello World", 6, 5)  // "World"
    substr("Hello", 1, 3)  // "ell"
    substr("test", 10, 5)  // ""
    substr("test", 0, 100)  // "test"

Common Patterns:
    # First N characters
    let prefix: str = substr(s, 0, n)
    
    # Skip first N characters
    let suffix: str = substr(s, n, len(s) - n)
    
    # Middle section
    let middle: str = substr(s, 5, 10)

Notes:
    - Safe for out-of-bounds (returns empty)
    - Creates new string (not view)
    - Byte-based not character-based

to_str(value: int|bool|str) -> str
-----------------------------------
Description:
    Convert value to string representation. Accepts int, bool, or str.
    For str input, acts as identity function.

Signature:
    to_str(value: int|bool|str) -> str

Parameters:
    value: Value to convert (int, bool, or str)

Returns:
    str: String representation of value

Conversion Rules:
    int: Decimal representation with optional minus sign
    bool: "true" or "false" (lowercase)
    str: Returned unchanged

Time Complexity: O(log n) for int, O(1) for bool/str
Space Complexity: O(log n) for int, O(1) for bool/str

Examples:
    to_str(42)  // "42"
    to_str(-10)  // "-10"
    to_str(true)  // "true"
    to_str(false)  // "false"
    to_str("test")  // "test"

Common Patterns:
    # Build debug messages
    let msg: str = "Value: " + to_str(x)
    
    # Convert for output
    print("Result:", to_str(result))

Notes:
    - Required for int/bool -> str conversion
    - No format options (always decimal)
    - Bool always lowercase

chr(code: int) -> str
---------------------
Description:
    Convert ASCII code to single-character string. Accepts values 0-127.
    Fatal error for values outside this range.

Signature:
    chr(code: int) -> str

Parameters:
    code: Integer ASCII code (0-127)

Returns:
    str: Single-character string

Error Conditions:
    - code < 0: Fatal error
    - code > 127: Fatal error

Time Complexity: O(1)
Space Complexity: O(1)

Examples:
    chr(65)  // "A"
    chr(97)  // "a"
    chr(48)  // "0"
    chr(32)  // " "
    chr(10)  // "\n"

Common Patterns:
    # Generate alphabet
    let i: int = 65
    while i <= 90:
        print(chr(i))
        i = i + 1
    
    # Build string from codes
    let s: str = chr(72) + chr(105)  // "Hi"

Notes:
    - ASCII only (no Unicode)
    - Fatal on invalid input
    - Single character only

ord(c: str) -> int
------------------
Description:
    Convert first character of string to ASCII code. Fatal error if string
    is empty. Only first character considered for non-empty strings.

Signature:
    ord(c: str) -> int

Parameters:
    c: Non-empty string

Returns:
    int: ASCII code of first character (0-127 for ASCII)

Error Conditions:
    - Empty string: Fatal error

Time Complexity: O(1)
Space Complexity: O(1)

Examples:
    ord("A")  // 65
    ord("Hello")  // 72 (only first char)
    ord("0")  // 48

Common Patterns:
    # Check if uppercase
    let code: int = ord(c)
    if code >= 65 and code <= 90:
        print("Uppercase")
    
    # Case conversion offset
    let offset: int = ord("a") - ord("A")  // 32

Notes:
    - Only first character used
    - Fatal on empty string
    - Returns byte value (not Unicode code point)

str_upper(s: str) -> str
------------------------
Description:
    Convert string to uppercase. Transforms lowercase letters (a-z) to
    uppercase (A-Z). Other characters unchanged.

Signature:
    str_upper(s: str) -> str

Parameters:
    s: String to convert

Returns:
    str: New string with lowercase letters converted to uppercase

Transformation:
    a-z -> A-Z
    All other bytes unchanged

Time Complexity: O(n) where n = string length
Space Complexity: O(n)

Examples:
    str_upper("hello")  // "HELLO"
    str_upper("Hello World")  // "HELLO WORLD"
    str_upper("123abc")  // "123ABC"
    str_upper("ALREADY")  // "ALREADY"

Common Patterns:
    # Case-insensitive comparison
    if str_upper(s1) == str_upper(s2):
        print("Equal (ignoring case)")
    
    # Normalize input
    let normalized: str = str_upper(input)

Notes:
    - ASCII only
    - Creates new string
    - Non-letters unchanged

str_lower(s: str) -> str
------------------------
Description:
    Convert string to lowercase. Transforms uppercase letters (A-Z) to
    lowercase (a-z). Other characters unchanged.

Signature:
    str_lower(s: str) -> str

Parameters:
    s: String to convert

Returns:
    str: New string with uppercase letters converted to lowercase

Transformation:
    A-Z -> a-z
    All other bytes unchanged

Time Complexity: O(n)
Space Complexity: O(n)

Examples:
    str_lower("HELLO")  // "hello"
    str_lower("Hello World")  // "hello world"
    str_lower("123ABC")  // "123abc"

Common Patterns:
    # Normalize for comparison
    if str_lower(command) == "quit":
        exit(0)

Notes:
    - ASCII only
    - Creates new string
    - Non-letters unchanged

str_trim(s: str) -> str
-----------------------
Description:
    Remove leading and trailing whitespace. Whitespace includes space,
    tab, newline, carriage return, and other whitespace characters.

Signature:
    str_trim(s: str) -> str

Parameters:
    s: String to trim

Returns:
    str: New string with whitespace removed from both ends

Whitespace Characters:
    - Space (32)
    - Tab (9)
    - Newline (10)
    - Carriage return (13)
    - Form feed (12)
    - Vertical tab (11)

Time Complexity: O(n)
Space Complexity: O(n)

Examples:
    str_trim("  hello  ")  // "hello"
    str_trim("\t\ntest\r\n")  // "test"
    str_trim("no_spaces")  // "no_spaces"
    str_trim("   ")  // ""

Common Patterns:
    # Clean user input
    let input: str = str_trim(read_line())
    
    # Process lines
    let line: str = str_trim(file_read("data.txt"))

Notes:
    - Only affects leading/trailing
    - Internal whitespace preserved
    - Creates new string

starts_with(s: str, prefix: str) -> bool
-----------------------------------------
Description:
    Check if string begins with specified prefix. Case-sensitive comparison.

Signature:
    starts_with(s: str, prefix: str) -> bool

Parameters:
    s: String to check
    prefix: Prefix to look for

Returns:
    bool: true if s starts with prefix, false otherwise

Special Cases:
    - Empty prefix always returns true
    - Prefix longer than s returns false

Time Complexity: O(m) where m = len(prefix)
Space Complexity: O(1)

Examples:
    starts_with("Hello World", "Hello")  // true
    starts_with("Hello World", "World")  // false
    starts_with("test", "")  // true
    starts_with("", "test")  // false

Common Patterns:
    # Command parsing
    if starts_with(input, "/help"):
        show_help()
    
    # File type checking
    if starts_with(filename, "test_"):
        run_test(filename)

Notes:
    - Case-sensitive
    - Byte comparison
    - O(1) space

ends_with(s: str, suffix: str) -> bool
---------------------------------------
Description:
    Check if string ends with specified suffix. Case-sensitive comparison.

Signature:
    ends_with(s: str, suffix: str) -> bool

Parameters:
    s: String to check
    suffix: Suffix to look for

Returns:
    bool: true if s ends with suffix, false otherwise

Special Cases:
    - Empty suffix always returns true
    - Suffix longer than s returns false

Time Complexity: O(m) where m = len(suffix)
Space Complexity: O(1)

Examples:
    ends_with("Hello World", "World")  // true
    ends_with("Hello World", "Hello")  // false
    ends_with("test.txt", ".txt")  // true

Common Patterns:
    # File extension checking
    if ends_with(filename, ".bp"):
        compile(filename)
    
    # URL validation
    if ends_with(url, "/"):
        url = substr(url, 0, len(url) - 1)

Notes:
    - Case-sensitive
    - Byte comparison
    - O(1) space

str_find(s: str, needle: str) -> int
------------------------------------
Description:
    Find first occurrence of substring in string. Returns index of first
    match, or -1 if not found. Zero-based indexing.

Signature:
    str_find(s: str, needle: str) -> int

Parameters:
    s: String to search in (haystack)
    needle: Substring to search for

Returns:
    int: Zero-based index of first occurrence, or -1 if not found

Special Cases:
    - Empty needle returns 0
    - Needle longer than haystack returns -1

Time Complexity: O(n*m) naive search algorithm
Space Complexity: O(1)

Examples:
    str_find("Hello World", "World")  // 6
    str_find("Hello World", "xyz")  // -1
    str_find("test", "")  // 0
    str_find("abcabc", "bc")  // 1

Common Patterns:
    # Check if substring exists
    if str_find(text, "keyword") != -1:
        print("Found")
    
    # Find and extract
    let pos: int = str_find(text, ":")
    if pos != -1:
        let value: str = substr(text, pos + 1, len(text) - pos - 1)

Notes:
    - Returns first match only
    - O(n*m) time (no optimization)
    - Case-sensitive

str_replace(s: str, old: str, new: str) -> str
-----------------------------------------------
Description:
    Replace first occurrence of substring with another string. If old not
    found, returns original string unchanged.

Signature:
    str_replace(s: str, old: str, new: str) -> str

Parameters:
    s: Source string
    old: Substring to replace
    new: Replacement string

Returns:
    str: New string with first occurrence of old replaced by new

Special Cases:
    - Empty old returns original string
    - old not found returns original string
    - new can be empty (deletion)
    - new can be longer than old

Time Complexity: O(n)
Space Complexity: O(n + len(new) - len(old))

Examples:
    str_replace("Hello World", "World", "Universe")  // "Hello Universe"
    str_replace("test test", "test", "demo")  // "demo test"
    str_replace("Hello", "xyz", "abc")  // "Hello"
    str_replace("Hello", "l", "")  // "Helo"

Common Patterns:
    # Fix typos
    let fixed: str = str_replace(text, "teh", "the")
    
    # Template substitution
    let msg: str = str_replace(template, "{name}", username)

Notes:
    - First occurrence only
    - Creates new string
    - old and new can be any length

================================================================================
BITWISE OPERATIONS (6 functions)
================================================================================

bit_and(a: int, b: int) -> int
    Bitwise AND of two integers.
    Example: bit_and(255, 15) => 15

bit_or(a: int, b: int) -> int
    Bitwise OR of two integers.
    Example: bit_or(240, 15) => 255

bit_xor(a: int, b: int) -> int
    Bitwise XOR of two integers.
    Example: bit_xor(255, 15) => 240

bit_not(a: int) -> int
    Bitwise NOT (complement) of an integer.
    Example: bit_not(0) => -1

bit_shl(a: int, n: int) -> int
    Shift left by n bits.
    Example: bit_shl(1, 4) => 16

bit_shr(a: int, n: int) -> int
    Shift right by n bits.
    Example: bit_shr(255, 4) => 15

Common Patterns:
    # Set a flag
    let flags: int = bit_or(flags, 4)

    # Check a flag
    let has_flag: bool = bit_and(flags, 4) != 0

    # Clear a flag
    flags = bit_and(flags, bit_not(4))

================================================================================
FLOAT MATH FUNCTIONS (17 functions)
================================================================================

sin(x: float) -> float     Sine of x (radians)
cos(x: float) -> float     Cosine of x (radians)
tan(x: float) -> float     Tangent of x (radians)
asin(x: float) -> float    Arcsine (-1 to 1)
acos(x: float) -> float    Arccosine (-1 to 1)
atan(x: float) -> float    Arctangent
atan2(y: float, x: float) -> float   Two-argument arctangent
log(x: float) -> float     Natural logarithm
log10(x: float) -> float   Base-10 logarithm
log2(x: float) -> float    Base-2 logarithm
exp(x: float) -> float     e^x
fabs(x: float) -> float    Absolute value
ffloor(x: float) -> float  Floor (round down)
fceil(x: float) -> float   Ceiling (round up)
fround(x: float) -> float  Round to nearest
fsqrt(x: float) -> float   Square root
fpow(base: float, exp: float) -> float  Exponentiation

================================================================================
TYPE CONVERSION FUNCTIONS
================================================================================

to_str(value: any) -> str           Convert any value to string
int_to_float(n: int) -> float       Integer to float
float_to_int(f: float) -> int       Float to integer (truncates)
float_to_str(f: float) -> str       Float to string
str_to_float(s: str) -> float       String to float
int_to_hex(n: int) -> str           Integer to hexadecimal string
hex_to_int(s: str) -> int           Hexadecimal string to integer

================================================================================
ENUM TYPES
================================================================================

Enums define named integer constants:

    enum Color:
        RED        # 0 (auto-incremented)
        GREEN      # 1
        BLUE       # 2

    enum HttpStatus:
        OK = 200
        NOT_FOUND = 404
        SERVER_ERROR = 500

Access: Color.RED => 0, HttpStatus.NOT_FOUND => 404

================================================================================
STRUCT FIELD MUTATION
================================================================================

Struct fields can be mutated after creation:

    struct Point:
        x: int
        y: int

    def main() -> int:
        let p: Point = Point{x: 10, y: 20}
        p.x = 42          # Mutate field
        p.y = p.x + 1     # Expression assignment
        print(p.x)         # 42
        return 0

================================================================================
END OF API REFERENCE
================================================================================
