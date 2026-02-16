# BetterPython Complete Function Reference

**Version 2.0.0 | Total Functions: 152**

This document provides comprehensive reference for all built-in functions in BetterPython. Functions are organized by category with complete signatures, descriptions, and examples.

---

## Table of Contents

1. [I/O Functions (2)](#io-functions)
2. [String Operations (27)](#string-operations)
3. [Integer Math (10)](#integer-math)
4. [Float Math (17)](#float-math)
5. [Type Conversion (7)](#type-conversion)
6. [Float Utilities (2)](#float-utilities)
7. [Encoding (4)](#encoding)
8. [Random Numbers (3)](#random-numbers)
9. [Security/Crypto (4)](#securitycrypto)
10. [File Operations (9)](#file-operations)
11. [Time Functions (2)](#time-functions)
12. [System Functions (4)](#system-functions)
13. [Validation (4)](#validation)
14. [Array Operations (14)](#array-operations)
15. [Map Operations (5)](#map-operations)
16. [Threading (14)](#threading)
17. [Regex (5)](#regex)
18. [Bitwise Functions (6)](#bitwise-functions)
19. [JSON Functions (2)](#json-functions)
20. [Bytes Buffer Operations (11)](#bytes-buffer-operations)
21. [Binary Packing (2)](#binary-packing)
22. [Type Introspection (2)](#type-introspection)

---

## I/O Functions

### print
```
print(...) -> void
```
Output any number of values to stdout, separated by spaces, with a trailing newline.

**Parameters:** Variable arguments of any type (int, float, str, bool, arrays, maps)
**Returns:** void
**Example:**
```python
print("Hello", 42, true)  # Output: Hello 42 true
print(3.14159)            # Output: 3.14159
```

### read_line
```
read_line() -> str
```
Read a single line from stdin, blocking until Enter is pressed. Removes trailing newline.

**Parameters:** None
**Returns:** str - Input line without trailing newline
**Example:**
```python
print("Enter your name:")
let name: str = read_line()
print("Hello,", name)
```

---

## String Operations

### len
```
len(s: str) -> int
```
Return the length of a string in bytes.

**Time Complexity:** O(1)
**Example:** `len("hello")` → `5`

### substr
```
substr(s: str, start: int, length: int) -> str
```
Extract a substring starting at `start` with specified `length`. Zero-indexed.

**Example:** `substr("hello world", 0, 5)` → `"hello"`

### str_upper
```
str_upper(s: str) -> str
```
Convert string to uppercase (ASCII a-z → A-Z).

**Example:** `str_upper("hello")` → `"HELLO"`

### str_lower
```
str_lower(s: str) -> str
```
Convert string to lowercase (ASCII A-Z → a-z).

**Example:** `str_lower("HELLO")` → `"hello"`

### str_trim
```
str_trim(s: str) -> str
```
Remove leading and trailing whitespace (space, tab, newline, carriage return).

**Example:** `str_trim("  hello  ")` → `"hello"`

### str_reverse
```
str_reverse(s: str) -> str
```
Reverse the string.

**Example:** `str_reverse("hello")` → `"olleh"`

### str_repeat
```
str_repeat(s: str, count: int) -> str
```
Repeat string `count` times.

**Example:** `str_repeat("ab", 3)` → `"ababab"`

### str_pad_left
```
str_pad_left(s: str, width: int, pad: str) -> str
```
Left-pad string to `width` using `pad` character.

**Example:** `str_pad_left("42", 5, "0")` → `"00042"`

### str_pad_right
```
str_pad_right(s: str, width: int, pad: str) -> str
```
Right-pad string to `width` using `pad` character.

**Example:** `str_pad_right("42", 5, "0")` → `"42000"`

### str_contains
```
str_contains(haystack: str, needle: str) -> bool
```
Check if string contains substring.

**Example:** `str_contains("hello world", "world")` → `true`

### str_count
```
str_count(haystack: str, needle: str) -> int
```
Count non-overlapping occurrences of substring.

**Example:** `str_count("ababa", "aba")` → `1`

### str_find
```
str_find(haystack: str, needle: str) -> int
```
Find first occurrence of substring. Returns -1 if not found.

**Example:** `str_find("hello world", "world")` → `6`

### str_index_of
```
str_index_of(haystack: str, needle: str) -> int
```
Alias for `str_find`.

### str_replace
```
str_replace(s: str, old: str, new: str) -> str
```
Replace first occurrence of `old` with `new`.

**Example:** `str_replace("hello", "l", "L")` → `"heLlo"`

### str_char_at
```
str_char_at(s: str, index: int) -> str
```
Get character at index as single-character string.

**Example:** `str_char_at("hello", 1)` → `"e"`

### starts_with
```
starts_with(s: str, prefix: str) -> bool
```
Check if string starts with prefix.

**Example:** `starts_with("hello", "hel")` → `true`

### ends_with
```
ends_with(s: str, suffix: str) -> bool
```
Check if string ends with suffix.

**Example:** `ends_with("hello.txt", ".txt")` → `true`

### chr
```
chr(code: int) -> str
```
Convert ASCII code (0-127) to single-character string.

**Example:** `chr(65)` → `"A"`

### ord
```
ord(c: str) -> int
```
Convert first character to ASCII code.

**Example:** `ord("A")` → `65`

### to_str
```
to_str(value: int|bool|str) -> str
```
Convert value to string representation.

**Example:** `to_str(42)` → `"42"`, `to_str(true)` → `"true"`

### str_split / str_split_str
```
str_split(s: str, delimiter: str) -> [str]
str_split_str(s: str, delimiter: str) -> [str]
```
Split string by delimiter into array.

**Example:** `str_split("a,b,c", ",")` → `["a", "b", "c"]`

### str_join / str_join_arr
```
str_join(arr: [str], delimiter: str) -> str
str_join_arr(arr: [str], delimiter: str) -> str
```
Join string array with delimiter.

**Example:** `str_join(["a", "b", "c"], "-")` → `"a-b-c"`

### str_concat_all
```
str_concat_all(arr: [str]) -> str
```
Concatenate all strings in array without delimiter.

**Example:** `str_concat_all(["hello", " ", "world"])` → `"hello world"`

### str_from_chars
```
str_from_chars(chars: [int]) -> str
```
Build string from array of ASCII character codes.

**Example:**
```python
let chars: [int] = [72, 101, 108, 108, 111]
str_from_chars(chars)  # "Hello"
```

### str_bytes
```
str_bytes(s: str) -> [int]
```
Get array of byte values from string.

**Example:**
```python
let bytes: [int] = str_bytes("ABC")  # [65, 66, 67]
```

---

## Integer Math

### abs
```
abs(n: int) -> int
```
Absolute value.

**Example:** `abs(-42)` → `42`

### min
```
min(a: int, b: int) -> int
```
Return smaller of two integers.

**Example:** `min(10, 5)` → `5`

### max
```
max(a: int, b: int) -> int
```
Return larger of two integers.

**Example:** `max(10, 5)` → `10`

### pow
```
pow(base: int, exp: int) -> int
```
Raise base to power.

**Example:** `pow(2, 10)` → `1024`

### sqrt
```
sqrt(n: int) -> int
```
Integer square root (floor).

**Example:** `sqrt(17)` → `4`

### floor
```
floor(n: int) -> int
```
Floor (identity for integers).

### ceil
```
ceil(n: int) -> int
```
Ceiling (identity for integers).

### round
```
round(n: int) -> int
```
Round (identity for integers).

### clamp
```
clamp(value: int, min: int, max: int) -> int
```
Clamp value between min and max.

**Example:** `clamp(15, 0, 10)` → `10`

### sign
```
sign(n: int) -> int
```
Return -1, 0, or 1 based on sign.

**Example:** `sign(-42)` → `-1`, `sign(0)` → `0`, `sign(42)` → `1`

---

## Float Math

All float math functions operate on IEEE 754 double-precision floats.

### sin
```
sin(x: float) -> float
```
Sine of x (radians).

### cos
```
cos(x: float) -> float
```
Cosine of x (radians).

### tan
```
tan(x: float) -> float
```
Tangent of x (radians).

### asin
```
asin(x: float) -> float
```
Arc sine (inverse sine).

### acos
```
acos(x: float) -> float
```
Arc cosine (inverse cosine).

### atan
```
atan(x: float) -> float
```
Arc tangent (inverse tangent).

### atan2
```
atan2(y: float, x: float) -> float
```
Two-argument arc tangent.

### log
```
log(x: float) -> float
```
Natural logarithm (ln).

### log10
```
log10(x: float) -> float
```
Base-10 logarithm.

### log2
```
log2(x: float) -> float
```
Base-2 logarithm.

### exp
```
exp(x: float) -> float
```
Exponential function (e^x).

### fabs
```
fabs(x: float) -> float
```
Float absolute value.

### ffloor
```
ffloor(x: float) -> float
```
Floor for floats.

**Example:** `ffloor(3.7)` → `3.0`

### fceil
```
fceil(x: float) -> float
```
Ceiling for floats.

**Example:** `fceil(3.2)` → `4.0`

### fround
```
fround(x: float) -> float
```
Round to nearest integer (float result).

### fsqrt
```
fsqrt(x: float) -> float
```
Square root for floats.

**Example:** `fsqrt(16.0)` → `4.0`

### fpow
```
fpow(base: float, exp: float) -> float
```
Power function for floats.

**Example:** `fpow(2.0, 0.5)` → `1.414...`

---

## Type Conversion

### int_to_float
```
int_to_float(n: int) -> float
```
Convert integer to float.

**Example:** `int_to_float(42)` → `42.0`

### float_to_int
```
float_to_int(f: float) -> int
```
Convert float to integer (truncates toward zero).

**Example:** `float_to_int(3.9)` → `3`

### float_to_str
```
float_to_str(f: float) -> str
```
Convert float to string representation.

**Example:** `float_to_str(3.14159)` → `"3.14159"`

### str_to_float
```
str_to_float(s: str) -> float
```
Parse string as float.

**Example:** `str_to_float("3.14")` → `3.14`

### int_to_hex
```
int_to_hex(n: int) -> str
```
Convert integer to hexadecimal string.

**Example:** `int_to_hex(255)` → `"ff"`

### hex_to_int
```
hex_to_int(hex: str) -> int
```
Parse hexadecimal string as integer.

**Example:** `hex_to_int("ff")` → `255`

### parse_int
```
parse_int(s: str) -> int
```
Parse string as integer (base 10). Supports leading signs.

**Example:** `parse_int("42")` → `42`, `parse_int("-7")` → `-7`

---

## Float Utilities

### is_nan
```
is_nan(f: float) -> bool
```
Check if float is NaN (Not a Number).

### is_inf
```
is_inf(f: float) -> bool
```
Check if float is positive or negative infinity.

---

## Encoding

### base64_encode
```
base64_encode(data: str) -> str
```
Encode string to base64.

**Example:** `base64_encode("Hello")` → `"SGVsbG8="`

### base64_decode
```
base64_decode(data: str) -> str
```
Decode base64 string.

**Example:** `base64_decode("SGVsbG8=")` → `"Hello"`

### bytes_to_str
```
bytes_to_str(bytes: [int]) -> str
```
Convert byte array to string.

### str_to_bytes
```
str_to_bytes(s: str) -> [int]
```
Convert string to byte array.

---

## Random Numbers

### rand
```
rand() -> int
```
Generate pseudo-random integer in range [0, 32767].

**Note:** Not cryptographically secure. Use `random_bytes` for security.

### rand_range
```
rand_range(min: int, max: int) -> int
```
Generate random integer in range [min, max).

**Example:** `rand_range(1, 7)` → dice roll 1-6

### rand_seed
```
rand_seed(seed: int) -> void
```
Seed the random number generator for reproducible sequences.

---

## Security/Crypto

### hash_sha256
```
hash_sha256(data: str) -> str
```
Compute SHA-256 hash, returns 64-character hex string.

**Example:** `hash_sha256("hello")` → `"2cf24dba5fb0a30e..."`

### hash_md5
```
hash_md5(data: str) -> str
```
Compute MD5 hash, returns 32-character hex string.

**Warning:** MD5 is cryptographically broken. Use SHA-256 for security.

### secure_compare
```
secure_compare(a: str, b: str) -> bool
```
Constant-time string comparison to prevent timing attacks.

### random_bytes
```
random_bytes(length: int) -> str
```
Generate cryptographically secure random bytes.

---

## File Operations

### file_read
```
file_read(path: str) -> str
```
Read entire file as string.

**Example:**
```python
let content: str = file_read("data.txt")
print(content)
```

### file_write
```
file_write(path: str, data: str) -> bool
```
Write string to file (overwrites existing). Returns true on success.

### file_append
```
file_append(path: str, data: str) -> bool
```
Append string to file. Returns true on success.

### file_exists
```
file_exists(path: str) -> bool
```
Check if file exists.

### file_delete
```
file_delete(path: str) -> bool
```
Delete file. Returns true on success.

### file_size
```
file_size(path: str) -> int
```
Get file size in bytes. Returns -1 if file doesn't exist.

### file_copy
```
file_copy(src: str, dst: str) -> bool
```
Copy file from src to dst. Returns true on success.

### file_read_bytes
```
file_read_bytes(path: str) -> [int]
```
Read entire file as byte array. Each element is 0-255.

**Example:**
```python
let data: [int] = file_read_bytes("image.bin")
print(array_len(data))  # file size in bytes
```

### file_write_bytes
```
file_write_bytes(path: str, data: [int]) -> bool
```
Write byte array to file. Returns true on success.

**Example:**
```python
let header: [int] = [0x42, 0x50, 0x43, 0x30]
file_write_bytes("output.bpc", header)
```

---

## Time Functions

### clock_ms
```
clock_ms() -> int
```
Get current time in milliseconds since Unix epoch.

**Example:**
```python
let start: int = clock_ms()
# ... do work ...
let elapsed: int = clock_ms() - start
print("Elapsed:", elapsed, "ms")
```

### sleep
```
sleep(ms: int) -> void
```
Sleep for specified milliseconds.

**Example:** `sleep(1000)` sleeps for 1 second

---

## System Functions

### exit
```
exit(code: int) -> void
```
Exit program with status code.

**Example:** `exit(0)` for success, `exit(1)` for error

### getenv
```
getenv(name: str) -> str
```
Get environment variable. Returns empty string if not set.

**Example:** `getenv("HOME")` → `"/home/user"`

### argv
```
argv() -> [str]
```
Get command-line arguments as array.

### argc
```
argc() -> int
```
Get number of command-line arguments.

---

## Validation

### is_digit
```
is_digit(s: str) -> bool
```
Check if all characters are digits (0-9).

**Example:** `is_digit("123")` → `true`, `is_digit("12a")` → `false`

### is_alpha
```
is_alpha(s: str) -> bool
```
Check if all characters are alphabetic (a-z, A-Z).

### is_alnum
```
is_alnum(s: str) -> bool
```
Check if all characters are alphanumeric.

### is_space
```
is_space(s: str) -> bool
```
Check if all characters are whitespace.

---

## Array Operations

### array_len
```
array_len(arr: [T]) -> int
```
Get array length.

**Example:**
```python
let nums: [int] = [1, 2, 3, 4, 5]
print(array_len(nums))  # 5
```

### array_push
```
array_push(arr: [T], value: T) -> void
```
Append value to array (modifies in place).

**Example:**
```python
let nums: [int] = [1, 2, 3]
array_push(nums, 4)  # nums is now [1, 2, 3, 4]
```

### array_pop
```
array_pop(arr: [T]) -> T
```
Remove and return last element.

**Example:**
```python
let nums: [int] = [1, 2, 3]
let last: int = array_pop(nums)  # last = 3, nums = [1, 2]
```

### array_insert
```
array_insert(arr: [T], index: int, value: T) -> void
```
Insert value at index, shifting elements right.

**Example:**
```python
let nums: [int] = [1, 3, 4]
array_insert(nums, 1, 2)  # nums = [1, 2, 3, 4]
```

### array_remove
```
array_remove(arr: [T], index: int) -> T
```
Remove and return element at index, shifting elements left.

**Example:**
```python
let nums: [int] = [1, 2, 3, 4]
let removed: int = array_remove(nums, 1)  # removed = 2, nums = [1, 3, 4]
```

### array_sort
```
array_sort(arr: [int]) -> void
```
Sort integer array in-place (ascending order).

**Example:**
```python
let nums: [int] = [3, 1, 4, 1, 5]
array_sort(nums)  # nums = [1, 1, 3, 4, 5]
```

### array_slice
```
array_slice(arr: [T], start: int, end: int) -> [T]
```
Return new array with elements from start (inclusive) to end (exclusive).

**Example:**
```python
let nums: [int] = [1, 2, 3, 4, 5]
let sub: [int] = array_slice(nums, 1, 4)  # [2, 3, 4]
```

### array_concat
```
array_concat(arr1: [T], arr2: [T]) -> [T]
```
Return new array concatenating both arrays.

**Example:**
```python
let a: [int] = [1, 2]
let b: [int] = [3, 4]
let c: [int] = array_concat(a, b)  # [1, 2, 3, 4]
```

### array_copy
```
array_copy(arr: [T]) -> [T]
```
Return shallow copy of array.

**Example:**
```python
let original: [int] = [1, 2, 3]
let copy: [int] = array_copy(original)
```

### array_clear
```
array_clear(arr: [T]) -> void
```
Remove all elements from array.

### array_index_of
```
array_index_of(arr: [T], value: T) -> int
```
Find first index of value in array. Returns -1 if not found.

**Example:**
```python
let nums: [int] = [10, 20, 30]
array_index_of(nums, 20)  # 1
array_index_of(nums, 99)  # -1
```

### array_contains
```
array_contains(arr: [T], value: T) -> bool
```
Check if array contains value.

**Example:**
```python
let nums: [int] = [1, 2, 3]
array_contains(nums, 2)  # true
array_contains(nums, 9)  # false
```

### array_reverse
```
array_reverse(arr: [T]) -> void
```
Reverse array in-place.

**Example:**
```python
let nums: [int] = [1, 2, 3]
array_reverse(nums)  # nums = [3, 2, 1]
```

### array_fill
```
array_fill(size: int, value: T) -> [T]
```
Create new array of given size filled with value.

**Example:**
```python
let zeros: [int] = array_fill(5, 0)  # [0, 0, 0, 0, 0]
```

---

## Map Operations

### map_len
```
map_len(m: {K: V}) -> int
```
Get number of key-value pairs.

### map_keys
```
map_keys(m: {K: V}) -> [K]
```
Get array of all keys.

**Example:**
```python
let ages: {str: int} = {"alice": 25, "bob": 30}
let names: [str] = map_keys(ages)  # ["alice", "bob"]
```

### map_values
```
map_values(m: {K: V}) -> [V]
```
Get array of all values.

### map_has_key
```
map_has_key(m: {K: V}, key: K) -> bool
```
Check if key exists in map.

**Example:**
```python
if map_has_key(ages, "alice"):
    print("Found Alice!")
```

### map_delete
```
map_delete(m: {K: V}, key: K) -> void
```
Remove key-value pair from map.

---

## Threading

BetterPython provides basic threading primitives for concurrent programming.

### thread_spawn
```
thread_spawn(fn: () -> void) -> int
```
Spawn a new thread running function. Returns thread ID.

### thread_join
```
thread_join(thread_id: int) -> void
```
Wait for thread to complete.

### thread_detach
```
thread_detach(thread_id: int) -> void
```
Detach thread (will clean up automatically when done).

### thread_current
```
thread_current() -> int
```
Get current thread ID.

### thread_yield
```
thread_yield() -> void
```
Yield CPU to other threads.

### thread_sleep
```
thread_sleep(ms: int) -> void
```
Sleep current thread for milliseconds.

### mutex_new
```
mutex_new() -> int
```
Create new mutex. Returns mutex ID.

### mutex_lock
```
mutex_lock(mutex_id: int) -> void
```
Acquire mutex (blocking).

### mutex_trylock
```
mutex_trylock(mutex_id: int) -> bool
```
Try to acquire mutex (non-blocking). Returns true if acquired.

### mutex_unlock
```
mutex_unlock(mutex_id: int) -> void
```
Release mutex.

### cond_new
```
cond_new() -> int
```
Create condition variable. Returns cond ID.

### cond_wait
```
cond_wait(cond_id: int, mutex_id: int) -> void
```
Wait on condition variable (releases mutex while waiting).

### cond_signal
```
cond_signal(cond_id: int) -> void
```
Signal one waiting thread.

### cond_broadcast
```
cond_broadcast(cond_id: int) -> void
```
Signal all waiting threads.

---

## Regex

Regular expression functions for pattern matching and manipulation.

### regex_match
```
regex_match(pattern: str, text: str) -> bool
```
Check if entire text matches pattern.

**Example:**
```python
regex_match("[0-9]+", "12345")  # true
regex_match("[0-9]+", "abc")    # false
```

### regex_search
```
regex_search(pattern: str, text: str) -> str
```
Find first match of pattern in text. Returns empty string if no match.

**Example:**
```python
regex_search("[0-9]+", "abc123def")  # "123"
```

### regex_replace
```
regex_replace(pattern: str, replacement: str, text: str) -> str
```
Replace all matches of pattern with replacement.

**Example:**
```python
regex_replace("[0-9]+", "X", "a1b2c3")  # "aXbXcX"
```

### regex_split
```
regex_split(pattern: str, text: str) -> [str]
```
Split text by pattern matches.

**Example:**
```python
regex_split("[,;]", "a,b;c")  # ["a", "b", "c"]
```

### regex_find_all
```
regex_find_all(pattern: str, text: str) -> [str]
```
Find all matches of pattern in text.

**Example:**
```python
regex_find_all("[0-9]+", "a1b22c333")  # ["1", "22", "333"]
```

---

## Bitwise Functions

Bitwise operations on integers. Also available as operators: `&`, `|`, `^`, `~`, `<<`, `>>`.

### bit_and
```
bit_and(a: int, b: int) -> int
```
Bitwise AND. Equivalent to `a & b`.

**Example:** `bit_and(0xFF, 0x0F)` → `15`

### bit_or
```
bit_or(a: int, b: int) -> int
```
Bitwise OR. Equivalent to `a | b`.

**Example:** `bit_or(0xF0, 0x0F)` → `255`

### bit_xor
```
bit_xor(a: int, b: int) -> int
```
Bitwise XOR. Equivalent to `a ^ b`.

**Example:** `bit_xor(0xFF, 0x0F)` → `240`

### bit_not
```
bit_not(a: int) -> int
```
Bitwise NOT (complement). Equivalent to `~a`.

**Example:** `bit_not(0)` → `-1`

### bit_shl
```
bit_shl(a: int, bits: int) -> int
```
Bitwise shift left. Equivalent to `a << bits`.

**Example:** `bit_shl(1, 8)` → `256`

### bit_shr
```
bit_shr(a: int, bits: int) -> int
```
Bitwise shift right. Equivalent to `a >> bits`.

**Example:** `bit_shr(256, 4)` → `16`

---

## JSON Functions

### json_stringify
```
json_stringify(value: any) -> str
```
Serialize value to JSON string. Supports int, float, str, bool, null, arrays, and maps.

**Example:**
```python
let data: {str: int} = {"x": 1, "y": 2}
let json: str = json_stringify(data)  # '{"x":1,"y":2}'
```

### json_parse
```
json_parse(json: str) -> any
```
Parse JSON string into BetterPython values. Returns maps for objects, arrays for arrays.

**Example:**
```python
let data: {str: int} = json_parse("{\"x\": 42}")
print(data["x"])  # 42
```

---

## Bytes Buffer Operations

Low-level byte buffer operations for binary data manipulation. Used for bytecode generation, binary protocols, and file format handling.

### bytes_new
```
bytes_new(size: int) -> [int]
```
Create new byte array filled with zeros.

**Example:** `let buf: [int] = bytes_new(16)`

### bytes_get
```
bytes_get(arr: [int], index: int) -> int
```
Get byte value at index (0-255).

### bytes_set
```
bytes_set(arr: [int], index: int, value: int) -> void
```
Set byte value at index.

### bytes_append
```
bytes_append(arr: [int], byte: int) -> void
```
Append single byte to array.

**Example:**
```python
let buf: [int] = bytes_new(0)
bytes_append(buf, 0x42)
bytes_append(buf, 0x50)
```

### bytes_len
```
bytes_len(arr: [int]) -> int
```
Get length of byte array. Alias for `array_len`.

### bytes_write_u16
```
bytes_write_u16(arr: [int], value: int) -> void
```
Append 16-bit unsigned integer as 2 bytes (little-endian).

### bytes_write_u32
```
bytes_write_u32(arr: [int], value: int) -> void
```
Append 32-bit unsigned integer as 4 bytes (little-endian).

### bytes_write_i64
```
bytes_write_i64(arr: [int], value: int) -> void
```
Append 64-bit signed integer as 8 bytes (little-endian).

### bytes_read_u16
```
bytes_read_u16(arr: [int], offset: int) -> int
```
Read 16-bit unsigned integer from 2 bytes at offset (little-endian).

### bytes_read_u32
```
bytes_read_u32(arr: [int], offset: int) -> int
```
Read 32-bit unsigned integer from 4 bytes at offset (little-endian).

### bytes_read_i64
```
bytes_read_i64(arr: [int], offset: int) -> int
```
Read 64-bit signed integer from 8 bytes at offset (little-endian).

---

## Binary Packing

### int_to_bytes
```
int_to_bytes(value: int, width: int) -> [int]
```
Convert integer to byte array of given width (little-endian).

**Example:**
```python
int_to_bytes(0x1234, 2)  # [0x34, 0x12]
int_to_bytes(256, 4)     # [0, 1, 0, 0]
```

### int_from_bytes
```
int_from_bytes(arr: [int], offset: int, width: int) -> int
```
Read integer from byte array at offset with given width (little-endian).

**Example:**
```python
let data: [int] = [0x34, 0x12]
int_from_bytes(data, 0, 2)  # 0x1234
```

---

## Type Introspection

### typeof
```
typeof(value: any) -> str
```
Get runtime type name of value as string.

**Returns:** One of: `"int"`, `"float"`, `"str"`, `"bool"`, `"null"`, `"array"`, `"map"`, `"struct"`, `"func"`

**Example:**
```python
typeof(42)        # "int"
typeof("hello")   # "str"
typeof(3.14)      # "float"
typeof(true)      # "bool"
typeof(null)      # "null"
typeof([1, 2])    # "array"
```

### tag
```
tag(union_value: union) -> str
```
Get the variant tag index of a union value as string.

**Example:**
```python
union Shape:
    Circle(radius: float)
    Rect(width: float, height: float)

let c: Shape = Circle{radius: 3.14}
tag(c)  # "0"
```

---

## Summary by Category

| Category | Count | Description |
|----------|-------|-------------|
| I/O | 2 | Console input/output |
| String | 27 | Text manipulation |
| Integer Math | 10 | Numeric operations |
| Float Math | 17 | Trigonometry, logarithms, etc. |
| Conversion | 7 | Type conversions |
| Float Utils | 2 | NaN/Infinity checks |
| Encoding | 4 | Base64, bytes |
| Random | 3 | Random number generation |
| Security | 4 | Hashing, secure compare |
| File | 9 | File system operations |
| Time | 2 | Timing and sleep |
| System | 4 | Environment, exit, argv |
| Validation | 4 | Character classification |
| Array | 14 | Dynamic array operations |
| Map | 5 | Hash map operations |
| Threading | 14 | Concurrency primitives |
| Regex | 5 | Pattern matching |
| Bitwise | 6 | Bit manipulation |
| JSON | 2 | JSON parse/stringify |
| Bytes Buffer | 11 | Binary data manipulation |
| Binary Packing | 2 | Integer byte conversion |
| Type Introspection | 2 | Runtime type info |

**Total: 152 built-in functions**

---

## Alphabetical Index

abs, acos, argc, argv, array_clear, array_concat, array_contains, array_copy,
array_fill, array_index_of, array_insert, array_len, array_pop, array_push,
array_remove, array_reverse, array_slice, array_sort, asin, atan, atan2,
base64_decode, base64_encode, bit_and, bit_not, bit_or, bit_shl, bit_shr,
bit_xor, bytes_append, bytes_get, bytes_len, bytes_new, bytes_read_i64,
bytes_read_u16, bytes_read_u32, bytes_set, bytes_to_str, bytes_write_i64,
bytes_write_u16, bytes_write_u32, ceil, chr, clamp, clock_ms,
cond_broadcast, cond_new, cond_signal, cond_wait, cos, ends_with, exit,
exp, fabs, fceil, ffloor, file_append, file_copy, file_delete, file_exists,
file_read, file_read_bytes, file_size, file_write, file_write_bytes,
float_to_int, float_to_str, floor, fpow, fround, fsqrt, getenv,
hash_md5, hash_sha256, hex_to_int, int_from_bytes, int_to_bytes,
int_to_float, int_to_hex, is_alnum, is_alpha, is_digit, is_inf, is_nan,
is_space, json_parse, json_stringify, len, log, log10, log2, map_delete,
map_has_key, map_keys, map_len, map_values, max, min, mutex_lock,
mutex_new, mutex_trylock, mutex_unlock, ord, parse_int, pow, print,
rand, rand_range, rand_seed, random_bytes, read_line, regex_find_all,
regex_match, regex_replace, regex_search, regex_split, round,
secure_compare, sign, sin, sleep, sqrt, starts_with, str_bytes,
str_char_at, str_concat_all, str_contains, str_count, str_find,
str_from_chars, str_index_of, str_join, str_join_arr, str_lower,
str_pad_left, str_pad_right, str_repeat, str_replace, str_reverse,
str_split, str_split_str, str_to_bytes, str_to_float, str_trim,
str_upper, substr, tag, tan, thread_current, thread_detach, thread_join,
thread_sleep, thread_spawn, thread_yield, to_str, typeof

---

*Document Version: 2.0.0*
*Last Updated: February 2026*
*Maintained by: Claude (Anthropic)*
