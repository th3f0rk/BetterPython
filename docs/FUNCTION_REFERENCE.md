# BetterPython Complete Function Reference

Total Functions: 68

## Table of Contents

1. I/O Functions (2)
2. String Functions (21)
3. Math Functions (10)
4. Random Functions (3)
5. Security Functions (4)
6. File Functions (7)
7. Time Functions (2)
8. System Functions (2)
9. Encoding Functions (2)
10. Validation Functions (4)
11. Conversion Functions (4)
12. Character Functions (3)
13. Comparison Functions (3)

---

## I/O Functions

### print
```
print(...) -> void
```
Print any number of values separated by spaces.

### read_line
```
read_line() -> str
```
Read line from stdin, removing trailing newline.

---

## String Functions

### len
```
len(s: str) -> int
```
Return string length.

### substr
```
substr(s: str, start: int, length: int) -> str
```
Extract substring.

### str_upper
```
str_upper(s: str) -> str
```
Convert to uppercase.

### str_lower
```
str_lower(s: str) -> str
```
Convert to lowercase.

### str_trim
```
str_trim(s: str) -> str
```
Remove leading/trailing whitespace.

### str_reverse
```
str_reverse(s: str) -> str
```
Reverse string.

### str_repeat
```
str_repeat(s: str, count: int) -> str
```
Repeat string count times.

### str_pad_left
```
str_pad_left(s: str, width: int, pad: str) -> str
```
Pad left to width with pad string.

### str_pad_right
```
str_pad_right(s: str, width: int, pad: str) -> str
```
Pad right to width with pad string.

### str_contains
```
str_contains(haystack: str, needle: str) -> bool
```
Check if haystack contains needle.

### str_count
```
str_count(haystack: str, needle: str) -> int
```
Count non-overlapping occurrences.

### str_find
```
str_find(haystack: str, needle: str) -> int
```
Find first occurrence, return index or -1.

### str_index_of
```
str_index_of(haystack: str, needle: str) -> int
```
Alias for str_find.

### str_replace
```
str_replace(s: str, old: str, new: str) -> str
```
Replace first occurrence of old with new.

### str_char_at
```
str_char_at(s: str, index: int) -> str
```
Get character at index.

### starts_with
```
starts_with(s: str, prefix: str) -> bool
```
Check if string starts with prefix.

### ends_with
```
ends_with(s: str, suffix: str) -> bool
```
Check if string ends with suffix.

### to_str
```
to_str(value: int|bool|str) -> str
```
Convert value to string.

---

## Math Functions

### abs
```
abs(n: int) -> int
```
Absolute value.

### min
```
min(a: int, b: int) -> int
```
Minimum of two integers.

### max
```
max(a: int, b: int) -> int
```
Maximum of two integers.

### pow
```
pow(base: int, exp: int) -> int
```
Raise base to power exp.

### sqrt
```
sqrt(n: int) -> int
```
Integer square root.

### floor
```
floor(n: int) -> int
```
Floor function (identity for integers).

### ceil
```
ceil(n: int) -> int
```
Ceiling function (identity for integers).

### round
```
round(n: int) -> int
```
Rounding function (identity for integers).

### clamp
```
clamp(value: int, min: int, max: int) -> int
```
Clamp value between min and max.

### sign
```
sign(n: int) -> int
```
Return -1, 0, or 1 based on sign of n.

---

## Random Functions

### rand
```
rand() -> int
```
Generate random integer 0-32767.

### rand_range
```
rand_range(min: int, max: int) -> int
```
Generate random integer in [min, max).

### rand_seed
```
rand_seed(seed: int) -> void
```
Seed random number generator.

---

## Security Functions

### hash_sha256
```
hash_sha256(data: str) -> str
```
Compute SHA-256 hash (64 hex chars).

### hash_md5
```
hash_md5(data: str) -> str
```
Compute MD5 hash (32 hex chars).

### secure_compare
```
secure_compare(a: str, b: str) -> bool
```
Constant-time string comparison.

### random_bytes
```
random_bytes(length: int) -> str
```
Generate cryptographically secure random bytes.

---

## File Functions

### file_read
```
file_read(path: str) -> str
```
Read entire file as string.

### file_write
```
file_write(path: str, data: str) -> bool
```
Write string to file (overwrites).

### file_append
```
file_append(path: str, data: str) -> bool
```
Append string to file.

### file_exists
```
file_exists(path: str) -> bool
```
Check if file exists.

### file_delete
```
file_delete(path: str) -> bool
```
Delete file.

### file_size
```
file_size(path: str) -> int
```
Get file size in bytes. Returns -1 if file does not exist.

### file_copy
```
file_copy(src: str, dst: str) -> bool
```
Copy file from src to dst.

---

## Time Functions

### clock_ms
```
clock_ms() -> int
```
Get current time in milliseconds since epoch.

### sleep
```
sleep(ms: int) -> void
```
Sleep for specified milliseconds.

---

## System Functions

### getenv
```
getenv(name: str) -> str
```
Get environment variable. Returns empty string if not found.

### exit
```
exit(code: int) -> void
```
Exit program with status code.

---

## Encoding Functions

### base64_encode
```
base64_encode(data: str) -> str
```
Encode string to base64.

### base64_decode
```
base64_decode(data: str) -> str
```
Decode base64 to string.

---

## Validation Functions

### is_digit
```
is_digit(s: str) -> bool
```
Check if all characters are digits.

### is_alpha
```
is_alpha(s: str) -> bool
```
Check if all characters are alphabetic.

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

## Conversion Functions

### chr
```
chr(code: int) -> str
```
Convert ASCII code to character (0-127).

### ord
```
ord(c: str) -> int
```
Convert first character to ASCII code.

### int_to_hex
```
int_to_hex(n: int) -> str
```
Convert integer to lowercase hexadecimal string.

### hex_to_int
```
hex_to_int(hex: str) -> int
```
Convert hexadecimal string to integer.

---

## Function Categories Summary

| Category | Count | Purpose |
|----------|-------|---------|
| String Operations | 21 | Text manipulation and analysis |
| Math | 10 | Numeric calculations |
| Security | 4 | Cryptography and secure operations |
| File I/O | 7 | File system operations |
| Validation | 4 | Input checking |
| Conversion | 4 | Type and format conversion |
| Random | 3 | Random number generation |
| I/O | 2 | Console input/output |
| Time | 2 | Timing and delays |
| System | 2 | System interaction |
| Encoding | 2 | Base64 encoding |
| Character | 3 | Character manipulation |

**Total:** 68 functions

---

## Alphabetical Index

abs, base64_decode, base64_encode, ceil, chr, clamp, clock_ms, ends_with, exit, file_append, file_copy, file_delete, file_exists, file_read, file_size, file_write, floor, getenv, hash_md5, hash_sha256, hex_to_int, int_to_hex, is_alnum, is_alpha, is_digit, is_space, len, max, min, ord, pow, print, rand, rand_range, rand_seed, random_bytes, read_line, round, secure_compare, sign, sleep, sqrt, starts_with, str_char_at, str_contains, str_count, str_find, str_index_of, str_lower, str_pad_left, str_pad_right, str_repeat, str_replace, str_reverse, str_trim, str_upper, substr, to_str

---

End of Function Reference
