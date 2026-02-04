# BetterPython String Utilities API Reference

Comprehensive documentation for all string manipulation functions.

## Table of Contents

1. String Transformation
2. String Analysis
3. String Search
4. String Validation
5. Character Operations
6. Performance Notes

---

## String Transformation

### str_reverse

```
Signature: str_reverse(s: str) -> str
```

Returns string with characters in reverse order.

**Example:**
```python
let text: str = "Hello"
let reversed: str = str_reverse(text)
print(reversed)
```

**Output:** "olleH"

**Use Cases:**
- Palindrome checking
- String reversals for algorithms
- Text effects

**Performance:** O(n) time, O(n) space

---

### str_repeat

```
Signature: str_repeat(s: str, count: int) -> str
```

Returns string repeated count times. Maximum count is 10000.

**Example:**
```python
let pattern: str = "=-"
let line: str = str_repeat(pattern, 20)
print(line)
```

**Output:** "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-"

**Use Cases:**
- Creating separator lines
- Padding generation
- Pattern repetition

**Performance:** O(n * count) time and space

---

### str_pad_left

```
Signature: str_pad_left(s: str, width: int, pad: str) -> str
```

Pads string on left to reach specified width using pad character(s).

**Example:**
```python
let number: str = "42"
let padded: str = str_pad_left(number, 5, "0")
print(padded)
```

**Output:** "00042"

**Multiple Character Padding:**
```python
let text: str = "Hi"
let padded: str = str_pad_left(text, 10, "=-")
print(padded)
```

**Output:** "=-=-=-=-Hi"

**Use Cases:**
- Number formatting
- Text alignment
- Fixed-width fields

**Performance:** O(width) time and space

---

### str_pad_right

```
Signature: str_pad_right(s: str, width: int, pad: str) -> str
```

Pads string on right to reach specified width.

**Example:**
```python
let text: str = "Name"
let padded: str = str_pad_right(text, 10, " ")
print(padded + "|")
```

**Output:** "Name      |"

---

### str_upper

```
Signature: str_upper(s: str) -> str
```

Converts all characters to uppercase.

**Example:**
```python
let text: str = "Hello World"
let upper: str = str_upper(text)
```

**Output:** "HELLO WORLD"

---

### str_lower

```
Signature: str_lower(s: str) -> str
```

Converts all characters to lowercase.

**Example:**
```python
let text: str = "Hello World"
let lower: str = str_lower(text)
```

**Output:** "hello world"

---

### str_trim

```
Signature: str_trim(s: str) -> str
```

Removes leading and trailing whitespace (spaces, tabs, newlines).

**Example:**
```python
let text: str = "  hello world  "
let trimmed: str = str_trim(text)
```

**Output:** "hello world"

---

## String Analysis

### str_contains

```
Signature: str_contains(haystack: str, needle: str) -> bool
```

Returns true if haystack contains needle.

**Example:**
```python
let text: str = "The quick brown fox"
if str_contains(text, "fox"):
    print("Found fox")
```

**Performance:** O(n * m) where n = haystack length, m = needle length

---

### str_count

```
Signature: str_count(haystack: str, needle: str) -> int
```

Returns number of non-overlapping occurrences of needle in haystack.

**Example:**
```python
let text: str = "the cat and the dog"
let count: int = str_count(text, "the")
print(count)
```

**Output:** 2

**Note:** Non-overlapping means "aaa" contains "aa" only once, not twice.

---

### len

```
Signature: len(s: str) -> int
```

Returns number of characters in string.

**Example:**
```python
let text: str = "Hello"
let length: int = len(text)
```

**Output:** 5

---

## String Search

### str_find

```
Signature: str_find(haystack: str, needle: str) -> int
```

Returns index of first occurrence of needle in haystack, or -1 if not found.

**Example:**
```python
let text: str = "Hello World"
let pos: int = str_find(text, "World")
```

**Output:** 6

**Zero-indexed:** First character is at index 0.

---

### str_index_of

```
Signature: str_index_of(haystack: str, needle: str) -> int
```

Alias for str_find. Provided for API compatibility.

---

### starts_with

```
Signature: starts_with(s: str, prefix: str) -> bool
```

Returns true if string starts with prefix.

**Example:**
```python
let url: str = "https://example.com"
if starts_with(url, "https://"):
    print("Secure URL")
```

**Performance:** O(prefix length)

---

### ends_with

```
Signature: ends_with(s: str, suffix: str) -> bool
```

Returns true if string ends with suffix.

**Example:**
```python
let filename: str = "document.txt"
if ends_with(filename, ".txt"):
    print("Text file")
```

**Performance:** O(suffix length)

---

## String Validation

### is_digit

```
Signature: is_digit(s: str) -> bool
```

Returns true if all characters are digits (0-9). Returns false for empty string.

**Example:**
```python
let code: str = "12345"
if is_digit(code):
    print("Valid numeric code")
```

---

### is_alpha

```
Signature: is_alpha(s: str) -> bool
```

Returns true if all characters are alphabetic (a-z, A-Z). Returns false for empty string.

**Example:**
```python
let name: str = "Alice"
if is_alpha(name):
    print("Valid name")
```

---

### is_alnum

```
Signature: is_alnum(s: str) -> bool
```

Returns true if all characters are alphanumeric (a-z, A-Z, 0-9). Returns false for empty string.

**Example:**
```python
let username: str = "user123"
if is_alnum(username):
    print("Valid username")
```

---

### is_space

```
Signature: is_space(s: str) -> bool
```

Returns true if all characters are whitespace (space, tab, newline, etc). Returns false for empty string.

**Example:**
```python
let text: str = "   "
if is_space(text):
    print("Only whitespace")
```

---

## Character Operations

### str_char_at

```
Signature: str_char_at(s: str, index: int) -> str
```

Returns character at specified index. Returns empty string if index out of bounds.

**Example:**
```python
let word: str = "Hello"
let char: str = str_char_at(word, 0)
print(char)
```

**Output:** "H"

**Note:** Zero-indexed. Index 0 is first character.

---

### ord

```
Signature: ord(c: str) -> int
```

Returns ASCII value of first character in string.

**Example:**
```python
let char: str = "A"
let code: int = ord(char)
print(code)
```

**Output:** 65

---

### chr

```
Signature: chr(code: int) -> str
```

Returns character for given ASCII code (0-127).

**Example:**
```python
let char: str = chr(65)
print(char)
```

**Output:** "A"

---

### substr

```
Signature: substr(s: str, start: int, length: int) -> str
```

Extracts substring starting at index start with specified length.

**Example:**
```python
let text: str = "Hello World"
let sub: str = substr(text, 0, 5)
print(sub)
```

**Output:** "Hello"

---

### str_replace

```
Signature: str_replace(s: str, old: str, new: str) -> str
```

Replaces first occurrence of old with new.

**Example:**
```python
let text: str = "Hello World"
let replaced: str = str_replace(text, "World", "Universe")
```

**Output:** "Hello Universe"

**Note:** Only replaces first occurrence. For multiple replacements, call repeatedly.

---

## Practical Examples

### Text Processing Pipeline

```python
def process_input(raw: str) -> str:
    let trimmed: str = str_trim(raw)
    let lower: str = str_lower(trimmed)
    return lower
```

### Input Validation

```python
def validate_username(username: str) -> bool:
    if len(username) < 3:
        return false
    
    if not is_alnum(username):
        return false
    
    return true
```

### String Formatting

```python
def format_table_row(name: str, value: str) -> str:
    let padded_name: str = str_pad_right(name, 20, " ")
    let padded_value: str = str_pad_left(value, 10, " ")
    return padded_name + " | " + padded_value
```

### Search and Extract

```python
def extract_domain(url: str) -> str:
    if starts_with(url, "http://"):
        let domain: str = substr(url, 7, len(url) - 7)
        return domain
    elif starts_with(url, "https://"):
        let domain: str = substr(url, 8, len(url) - 8)
        return domain
    else:
        return url
```

---

## Performance Characteristics

| Function | Time | Space | Notes |
|----------|------|-------|-------|
| str_reverse | O(n) | O(n) | Linear scan and copy |
| str_repeat | O(n*c) | O(n*c) | c = count |
| str_pad_left/right | O(w) | O(w) | w = width |
| str_upper/lower | O(n) | O(n) | Character conversion |
| str_trim | O(n) | O(n) | Scan both ends |
| str_contains | O(n*m) | O(1) | Naive search |
| str_count | O(n*m) | O(1) | Non-overlapping |
| str_find | O(n*m) | O(1) | First match |
| starts_with | O(m) | O(1) | m = prefix length |
| ends_with | O(m) | O(1) | m = suffix length |
| is_digit/alpha/alnum | O(n) | O(1) | Character checks |
| str_char_at | O(1) | O(1) | Direct indexing |
| substr | O(m) | O(m) | m = length |
| str_replace | O(n) | O(n) | Single replacement |

---

## Common Patterns

### Case-Insensitive Comparison

```python
def compare_ignore_case(a: str, b: str) -> bool:
    let lower_a: str = str_lower(a)
    let lower_b: str = str_lower(b)
    return lower_a == lower_b
```

### Word Count

```python
def count_words(text: str) -> int:
    let count: int = 0
    let in_word: bool = false
    let i: int = 0
    
    while i < len(text):
        let char: str = str_char_at(text, i)
        if is_space(char):
            in_word = false
        else:
            if not in_word:
                count = count + 1
                in_word = true
        i = i + 1
    
    return count
```

### URL Parsing

```python
def is_url(text: str) -> bool:
    if starts_with(text, "http://"):
        return true
    if starts_with(text, "https://"):
        return true
    if starts_with(text, "ftp://"):
        return true
    return false
```

---

End of String API Reference
