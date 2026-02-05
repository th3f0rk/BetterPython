# BetterPython Complete Function Reference

**Version 1.0.0 | Total Functions: 115**

This document provides comprehensive reference for all built-in functions in BetterPython. Functions are organized by category with complete signatures, descriptions, and examples.

---

## Table of Contents

1. [I/O Functions (2)](#io-functions)
2. [String Operations (25)](#string-operations)
3. [Integer Math (10)](#integer-math)
4. [Float Math (17)](#float-math)
5. [Type Conversion (6)](#type-conversion)
6. [Float Utilities (2)](#float-utilities)
7. [Encoding (4)](#encoding)
8. [Random Numbers (3)](#random-numbers)
9. [Security/Crypto (4)](#securitycrypto)
10. [File Operations (7)](#file-operations)
11. [Time Functions (2)](#time-functions)
12. [System Functions (4)](#system-functions)
13. [Validation (4)](#validation)
14. [Array Operations (3)](#array-operations)
15. [Map Operations (5)](#map-operations)
16. [Threading (14)](#threading)
17. [Regex (5)](#regex)

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

## Summary by Category

| Category | Count | Description |
|----------|-------|-------------|
| I/O | 2 | Console input/output |
| String | 25 | Text manipulation |
| Integer Math | 10 | Numeric operations |
| Float Math | 17 | Trigonometry, logarithms, etc. |
| Conversion | 6 | Type conversions |
| Float Utils | 2 | NaN/Infinity checks |
| Encoding | 4 | Base64, bytes |
| Random | 3 | Random number generation |
| Security | 4 | Hashing, secure compare |
| File | 7 | File system operations |
| Time | 2 | Timing and sleep |
| System | 4 | Environment, exit, argv |
| Validation | 4 | Character classification |
| Array | 3 | Dynamic array operations |
| Map | 5 | Hash map operations |
| Threading | 14 | Concurrency primitives |
| Regex | 5 | Pattern matching |

**Total: 115 built-in functions**

---

## Alphabetical Index

abs, acos, argc, argv, array_len, array_pop, array_push, asin, atan, atan2,
base64_decode, base64_encode, bytes_to_str, ceil, chr, clamp, clock_ms,
cond_broadcast, cond_new, cond_signal, cond_wait, cos, ends_with, exit,
exp, fabs, fceil, ffloor, file_append, file_copy, file_delete, file_exists,
file_read, file_size, file_write, float_to_int, float_to_str, floor, fpow,
fround, fsqrt, getenv, hash_md5, hash_sha256, hex_to_int, int_to_float,
int_to_hex, is_alnum, is_alpha, is_digit, is_inf, is_nan, is_space, len,
log, log10, log2, map_delete, map_has_key, map_keys, map_len, map_values,
max, min, mutex_lock, mutex_new, mutex_trylock, mutex_unlock, ord, pow,
print, rand, rand_range, rand_seed, random_bytes, read_line, regex_find_all,
regex_match, regex_replace, regex_search, regex_split, round, secure_compare,
sign, sin, sleep, sqrt, starts_with, str_char_at, str_concat_all, str_contains,
str_count, str_find, str_index_of, str_join, str_join_arr, str_lower,
str_pad_left, str_pad_right, str_repeat, str_replace, str_reverse, str_split,
str_split_str, str_to_bytes, str_to_float, str_trim, str_upper, substr,
tan, thread_current, thread_detach, thread_join, thread_sleep, thread_spawn,
thread_yield, to_str

---

*Document Version: 1.0.0*
*Last Updated: February 2026*
*Maintained by: Claude (Anthropic)*
