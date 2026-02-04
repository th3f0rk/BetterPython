# BetterPython Standard Library Reference

## Version 1.0.0 - 98+ Built-in Functions

---

## I/O Functions (2)

### `print(...) -> void`
Print any number of values to stdout, separated by spaces.
```python
print("Hello", 42, true)  # Output: Hello 42 true
```

### `read_line() -> str`
Read a line from stdin (removes trailing newline).
```python
let name: str = read_line()
```

---

## String Functions (21)

### Basic Operations

#### `len(s: str) -> int`
Get the length of a string.
```python
let length: int = len("Hello")  # 5
```

#### `substr(s: str, start: int, length: int) -> str`
Extract a substring.
```python
let sub: str = substr("Hello World", 0, 5)  # "Hello"
```

#### `to_str(value: int|bool|str|float) -> str`
Convert any value to string.
```python
let s: str = to_str(42)  # "42"
```

### Character Conversion

#### `chr(code: int) -> str`
Convert ASCII code (0-127) to character.
```python
let c: str = chr(65)  # "A"
```

#### `ord(c: str) -> int`
Convert first character to ASCII code.
```python
let code: int = ord("A")  # 65
```

#### `str_char_at(s: str, index: int) -> str`
Get character at specific index.
```python
let c: str = str_char_at("Hello", 1)  # "e"
```

#### `str_index_of(s: str, char: str) -> int`
Find index of character (-1 if not found).
```python
let pos: int = str_index_of("Hello", "l")  # 2
```

### String Manipulation

#### `str_upper(s: str) -> str`
Convert string to uppercase.
```python
let upper: str = str_upper("hello")  # "HELLO"
```

#### `str_lower(s: str) -> str`
Convert string to lowercase.
```python
let lower: str = str_lower("HELLO")  # "hello"
```

#### `str_trim(s: str) -> str`
Remove leading and trailing whitespace.
```python
let trimmed: str = str_trim("  hello  ")  # "hello"
```

#### `str_reverse(s: str) -> str`
Reverse a string.
```python
let rev: str = str_reverse("hello")  # "olleh"
```

#### `str_repeat(s: str, count: int) -> str`
Repeat string n times.
```python
let rep: str = str_repeat("ab", 3)  # "ababab"
```

#### `str_pad_left(s: str, length: int, char: str) -> str`
Pad string on the left.
```python
let padded: str = str_pad_left("42", 5, "0")  # "00042"
```

#### `str_pad_right(s: str, length: int, char: str) -> str`
Pad string on the right.
```python
let padded: str = str_pad_right("hi", 5, ".")  # "hi..."
```

### String Searching

#### `starts_with(s: str, prefix: str) -> bool`
Check if string starts with prefix.
```python
let result: bool = starts_with("Hello World", "Hello")  # true
```

#### `ends_with(s: str, suffix: str) -> bool`
Check if string ends with suffix.
```python
let result: bool = ends_with("Hello World", "World")  # true
```

#### `str_contains(s: str, needle: str) -> bool`
Check if string contains substring.
```python
let has: bool = str_contains("Hello World", "llo")  # true
```

#### `str_find(s: str, needle: str) -> int`
Find first occurrence of substring. Returns -1 if not found.
```python
let pos: int = str_find("Hello World", "World")  # 6
```

#### `str_count(s: str, needle: str) -> int`
Count occurrences of substring.
```python
let n: int = str_count("ababa", "a")  # 3
```

#### `str_replace(s: str, old: str, new: str) -> str`
Replace first occurrence of substring.
```python
let replaced: str = str_replace("Hello World", "World", "Universe")
# "Hello Universe"
```

#### `str_split(s: str, delimiter: str) -> [str]`
Split string into array.
```python
let parts: [str] = str_split("a,b,c", ",")  # ["a", "b", "c"]
```

#### `str_join(arr: [str], delimiter: str) -> str`
Join array into string.
```python
let joined: str = str_join(["a", "b", "c"], "-")  # "a-b-c"
```

---

## Integer Math Functions (10)

#### `abs(n: int) -> int`
Absolute value.
```python
let result: int = abs(-42)  # 42
```

#### `min(a: int, b: int) -> int`
Minimum of two integers.
```python
let result: int = min(10, 20)  # 10
```

#### `max(a: int, b: int) -> int`
Maximum of two integers.
```python
let result: int = max(10, 20)  # 20
```

#### `pow(base: int, exp: int) -> int`
Power function (base^exp).
```python
let result: int = pow(2, 8)  # 256
```

#### `sqrt(n: int) -> int`
Integer square root.
```python
let result: int = sqrt(144)  # 12
```

#### `floor(n: int) -> int`
Floor function (identity for integers).
```python
let result: int = floor(42)  # 42
```

#### `ceil(n: int) -> int`
Ceiling function (identity for integers).
```python
let result: int = ceil(42)  # 42
```

#### `round(n: int) -> int`
Rounding function (identity for integers).
```python
let result: int = round(42)  # 42
```

#### `clamp(value: int, min: int, max: int) -> int`
Clamp value to range.
```python
let result: int = clamp(15, 0, 10)  # 10
```

#### `sign(n: int) -> int`
Get sign (-1, 0, or 1).
```python
let s: int = sign(-42)  # -1
```

---

## Float Math Functions (17)

### Trigonometry

#### `sin(x: float) -> float`
Sine function.
```python
let s: float = sin(3.14159 / 2.0)  # ~1.0
```

#### `cos(x: float) -> float`
Cosine function.
```python
let c: float = cos(0.0)  # 1.0
```

#### `tan(x: float) -> float`
Tangent function.
```python
let t: float = tan(0.0)  # 0.0
```

#### `asin(x: float) -> float`
Arc sine function.
```python
let a: float = asin(1.0)  # ~1.5708
```

#### `acos(x: float) -> float`
Arc cosine function.
```python
let a: float = acos(1.0)  # 0.0
```

#### `atan(x: float) -> float`
Arc tangent function.
```python
let a: float = atan(1.0)  # ~0.7854
```

#### `atan2(y: float, x: float) -> float`
Two-argument arc tangent.
```python
let a: float = atan2(1.0, 1.0)  # ~0.7854
```

### Logarithms & Exponentials

#### `log(x: float) -> float`
Natural logarithm.
```python
let l: float = log(2.71828)  # ~1.0
```

#### `log10(x: float) -> float`
Base-10 logarithm.
```python
let l: float = log10(100.0)  # 2.0
```

#### `log2(x: float) -> float`
Base-2 logarithm.
```python
let l: float = log2(8.0)  # 3.0
```

#### `exp(x: float) -> float`
Exponential function (e^x).
```python
let e: float = exp(1.0)  # ~2.71828
```

### Rounding & Absolute

#### `fabs(x: float) -> float`
Absolute value (float).
```python
let a: float = fabs(-3.14)  # 3.14
```

#### `ffloor(x: float) -> float`
Floor function (float).
```python
let f: float = ffloor(3.7)  # 3.0
```

#### `fceil(x: float) -> float`
Ceiling function (float).
```python
let c: float = fceil(3.2)  # 4.0
```

#### `fround(x: float) -> float`
Round to nearest (float).
```python
let r: float = fround(3.5)  # 4.0
```

### Power & Root

#### `fsqrt(x: float) -> float`
Square root (float).
```python
let s: float = fsqrt(16.0)  # 4.0
```

#### `fpow(base: float, exp: float) -> float`
Power function (float).
```python
let p: float = fpow(2.0, 10.0)  # 1024.0
```

---

## Type Conversion Functions (7)

#### `to_str(value: any) -> str`
Convert any value to string.

#### `int_to_float(n: int) -> float`
Convert integer to float.
```python
let f: float = int_to_float(42)  # 42.0
```

#### `float_to_int(f: float) -> int`
Convert float to integer (truncates).
```python
let i: int = float_to_int(3.7)  # 3
```

#### `float_to_str(f: float) -> str`
Convert float to string.
```python
let s: str = float_to_str(3.14)  # "3.140000"
```

#### `str_to_float(s: str) -> float`
Parse string as float.
```python
let f: float = str_to_float("3.14")  # 3.14
```

#### `int_to_hex(n: int) -> str`
Convert integer to hexadecimal string.
```python
let h: str = int_to_hex(255)  # "ff"
```

#### `hex_to_int(s: str) -> int`
Parse hexadecimal string as integer.
```python
let n: int = hex_to_int("ff")  # 255
```

---

## Array Operations (3)

#### `array_len(arr: [T]) -> int`
Get array length.
```python
let arr: [int] = [1, 2, 3, 4, 5]
let length: int = array_len(arr)  # 5
```

#### `array_push(arr: [T], value: T) -> void`
Add element to end of array.
```python
let arr: [int] = [1, 2]
array_push(arr, 3)  # arr is now [1, 2, 3]
```

#### `array_pop(arr: [T]) -> T`
Remove and return last element.
```python
let arr: [int] = [1, 2, 3]
let last: int = array_pop(arr)  # 3, arr is now [1, 2]
```

---

## Map Operations (5)

#### `map_len(m: {K: V}) -> int`
Get number of entries in map.
```python
let m: {str: int} = {"a": 1, "b": 2}
let length: int = map_len(m)  # 2
```

#### `map_keys(m: {K: V}) -> [K]`
Get array of all keys.
```python
let m: {str: int} = {"a": 1, "b": 2}
let keys: [str] = map_keys(m)  # ["a", "b"]
```

#### `map_values(m: {K: V}) -> [V]`
Get array of all values.
```python
let m: {str: int} = {"a": 1, "b": 2}
let vals: [int] = map_values(m)  # [1, 2]
```

#### `map_has_key(m: {K: V}, key: K) -> bool`
Check if key exists.
```python
let m: {str: int} = {"a": 1}
let has: bool = map_has_key(m, "a")  # true
```

#### `map_delete(m: {K: V}, key: K) -> void`
Remove entry by key.
```python
let m: {str: int} = {"a": 1, "b": 2}
map_delete(m, "a")  # m is now {"b": 2}
```

---

## File I/O Functions (7)

#### `file_read(path: str) -> str`
Read entire file as string.
```python
let content: str = file_read("/tmp/test.txt")
```

#### `file_write(path: str, data: str) -> bool`
Write string to file (overwrites). Returns success.
```python
let ok: bool = file_write("/tmp/test.txt", "Hello")
```

#### `file_append(path: str, data: str) -> bool`
Append string to file. Returns success.
```python
let ok: bool = file_append("/tmp/test.txt", "\nLine 2")
```

#### `file_exists(path: str) -> bool`
Check if file exists.
```python
let exists: bool = file_exists("/tmp/test.txt")
```

#### `file_delete(path: str) -> bool`
Delete file. Returns success.
```python
let ok: bool = file_delete("/tmp/test.txt")
```

#### `file_size(path: str) -> int`
Get file size in bytes.
```python
let size: int = file_size("/tmp/test.txt")
```

#### `file_copy(src: str, dst: str) -> bool`
Copy file from source to destination.
```python
let ok: bool = file_copy("/tmp/a.txt", "/tmp/b.txt")
```

---

## Security Functions (4)

#### `hash_sha256(data: str) -> str`
Compute SHA-256 hash.
```python
let hash: str = hash_sha256("password")
```

#### `hash_md5(data: str) -> str`
Compute MD5 hash.
```python
let hash: str = hash_md5("data")
```

#### `secure_compare(a: str, b: str) -> bool`
Constant-time string comparison (prevents timing attacks).
```python
let equal: bool = secure_compare(hash1, hash2)
```

#### `random_bytes(length: int) -> str`
Generate cryptographically secure random bytes (hex encoded).
```python
let bytes: str = random_bytes(16)  # 32 hex characters
```

---

## Encoding Functions (4)

#### `base64_encode(data: str) -> str`
Encode string to base64.
```python
let encoded: str = base64_encode("Hello")  # "SGVsbG8="
```

#### `base64_decode(data: str) -> str`
Decode base64 to string.
```python
let decoded: str = base64_decode("SGVsbG8=")  # "Hello"
```

#### `bytes_to_str(bytes: [int]) -> str`
Convert byte array to string.

#### `str_to_bytes(s: str) -> [int]`
Convert string to byte array.

---

## Validation Functions (6)

#### `is_digit(s: str) -> bool`
Check if string contains only digits.
```python
let d: bool = is_digit("12345")  # true
```

#### `is_alpha(s: str) -> bool`
Check if string contains only letters.
```python
let a: bool = is_alpha("Hello")  # true
```

#### `is_alnum(s: str) -> bool`
Check if string is alphanumeric.
```python
let an: bool = is_alnum("Hello123")  # true
```

#### `is_space(s: str) -> bool`
Check if string contains only whitespace.
```python
let sp: bool = is_space("   ")  # true
```

#### `is_nan(f: float) -> bool`
Check if float is NaN (Not a Number).
```python
let nan: bool = is_nan(0.0 / 0.0)  # true
```

#### `is_inf(f: float) -> bool`
Check if float is infinity.
```python
let inf: bool = is_inf(1.0 / 0.0)  # true
```

---

## System Functions (7)

#### `clock_ms() -> int`
Get current time in milliseconds since epoch.
```python
let now: int = clock_ms()
```

#### `sleep(ms: int) -> void`
Sleep for specified milliseconds.
```python
sleep(1000)  # Sleep 1 second
```

#### `getenv(name: str) -> str`
Get environment variable value (empty string if not found).
```python
let user: str = getenv("USER")
```

#### `exit(code: int) -> void`
Exit program with status code.
```python
exit(0)   # Success
exit(1)   # Error
```

#### `rand() -> int`
Generate pseudo-random integer (0-32767).
```python
let r: int = rand()
```

#### `rand_range(min: int, max: int) -> int`
Generate random integer in range [min, max).
```python
let dice: int = rand_range(1, 7)  # 1-6
```

#### `rand_seed(seed: int) -> void`
Seed the random number generator.
```python
rand_seed(42)  # Reproducible random numbers
```

---

## Function Count Summary

| Category | Count |
|----------|-------|
| I/O | 2 |
| String Operations | 21 |
| Integer Math | 10 |
| Float Math | 17 |
| Type Conversion | 7 |
| Array Operations | 3 |
| Map Operations | 5 |
| File I/O | 7 |
| Security | 4 |
| Encoding | 4 |
| Validation | 6 |
| System/Random | 7 |
| **Total** | **93+** |

---

## Usage Examples

### Text Processing
```python
let text: str = "  HELLO WORLD  "
let clean: str = str_trim(text)
let lower: str = str_lower(clean)
let replaced: str = str_replace(lower, "world", "universe")
print(replaced)  # "hello universe"
```

### Random Password Generator
```python
rand_seed(clock_ms())
let password: str = ""
for i in range(0, 12):
    let code: int = rand_range(65, 91)
    password = password + chr(code)
print(password)
```

### File Processing with Error Handling
```python
try:
    if file_exists("input.txt"):
        let data: str = file_read("input.txt")
        let upper: str = str_upper(data)
        file_write("output.txt", upper)
        print("Processed!")
    else:
        throw "File not found"
catch e:
    print("Error: ")
    print(e)
```

### Mathematical Calculations
```python
let pi: float = 3.14159
let radius: float = 5.0
let area: float = pi * fpow(radius, 2.0)
let circumference: float = 2.0 * pi * radius
print(float_to_str(area))
print(float_to_str(circumference))
```

---

**BetterPython v1.0.0 - Standard Library**

*The AI-Native Programming Language*
