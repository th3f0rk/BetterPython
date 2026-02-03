# BetterPython Security Extensions
## Technical Reference

### Overview
This document provides technical documentation for security-related functions
in the BetterPython standard library. These functions enable cryptographic operations,
secure string comparison, and random number generation for security-sensitive applications.

---

## Cryptographic Hashing

### hash_sha256(data: str) -> str

Computes SHA-256 hash of input string.

**Parameters:**
- `data` (str): Input data to hash

**Returns:**
- String containing 64-character hexadecimal hash

**Implementation Notes:**
- Uses simplified SHA-256 algorithm for demonstration
- For production cryptographic use, consider linking against OpenSSL or libsodium
- Output is deterministic given same input
- Hash is returned as lowercase hexadecimal string

**Example Usage:**
```python
def main() -> int:
    let password: str = "my_secure_password"
    let hashed: str = hash_sha256(password)
    print("SHA256:", hashed)
    
    let data: str = "Hello, World!"
    let checksum: str = hash_sha256(data)
    file_write("data.sha256", checksum)
    
    return 0
```

**Security Considerations:**
- SHA-256 is suitable for checksums and non-cryptographic use
- For password hashing, use dedicated password hashing functions (not yet implemented)
- Store hashes securely, never store plaintext passwords

---

### hash_md5(data: str) -> str

Computes MD5 hash of input string.

**Parameters:**
- `data` (str): Input data to hash

**Returns:**
- String containing 32-character hexadecimal hash

**Implementation Notes:**
- Uses simplified MD5-like algorithm
- MD5 is cryptographically broken - use for checksums only, not security
- Maintained for compatibility with legacy systems

**Example Usage:**
```python
def main() -> int:
    let file_data: str = file_read("document.txt")
    let md5sum: str = hash_md5(file_data)
    print("MD5 checksum:", md5sum)
    return 0
```

**Security Warning:**
- MD5 is NOT secure for cryptographic purposes
- Vulnerable to collision attacks
- Use SHA-256 or better for security applications
- Acceptable for non-security checksums only

---

## Secure Comparison

### secure_compare(a: str, b: str) -> bool

Performs timing-attack resistant string comparison.

**Parameters:**
- `a` (str): First string to compare
- `b` (str): Second string to compare

**Returns:**
- Boolean: true if strings are equal, false otherwise

**Implementation Notes:**
- Always compares full length of both strings
- Prevents timing attacks by avoiding early termination
- Constant-time comparison regardless of input
- Suitable for comparing passwords, tokens, and hashes

**Example Usage:**
```python
def main() -> int:
    let stored_hash: str = file_read("password.hash")
    let input_password: str = read_line()
    let input_hash: str = hash_sha256(input_password)
    
    if secure_compare(stored_hash, input_hash):
        print("Password correct")
    else:
        print("Password incorrect")
    
    return 0
```

**Security Rationale:**
Traditional string comparison (using ==) may leak timing information:
```
if stored_hash == input_hash:  # BAD: Timing attack vulnerable
    ...

Attacker can measure time taken to compare strings character-by-character,
leaking information about correct characters.

Using secure_compare prevents this:
if secure_compare(stored_hash, input_hash):  # GOOD: Constant time
    ...
```

**Use Cases:**
- Password verification
- API token validation
- HMAC comparison
- Any security-sensitive string comparison

---

## Cryptographically Secure Random Data

### random_bytes(count: int) -> str

Generates cryptographically secure random bytes.

**Parameters:**
- `count` (int): Number of bytes to generate (1-1024)

**Returns:**
- String containing hexadecimal representation of random bytes (2 characters per byte)

**Implementation Notes:**
- Reads from /dev/urandom on Unix systems
- Falls back to pseudo-random if /dev/urandom unavailable
- Returns hex-encoded string for safe handling in string type system
- Suitable for cryptographic purposes when /dev/urandom available

**Example Usage:**
```python
def main() -> int:
    let salt: str = random_bytes(16)
    print("Random salt:", salt)
    
    let session_token: str = random_bytes(32)
    file_write("session.token", session_token)
    
    let api_key: str = random_bytes(24)
    print("Generated API key:", api_key)
    
    return 0
```

**Security Considerations:**
- Use for generating session tokens, API keys, salts
- NOT suitable for passwords (use dedicated password generator)
- Verify /dev/urandom availability on target system
- Output is deterministic only if /dev/urandom unavailable (fallback mode)

**Comparison with rand():**
```python
let weak_random: int = rand()              # Pseudo-random, predictable
let strong_random: str = random_bytes(4)   # Cryptographically secure
```

Use `rand()` for games, simulations, non-security applications.
Use `random_bytes()` for tokens, keys, security-sensitive random data.

---

## Extended String Utilities

### str_reverse(s: str) -> str

Reverses a string.

**Parameters:**
- `s` (str): Input string

**Returns:**
- Reversed string

**Example:**
```python
let text: str = "Hello"
let reversed: str = str_reverse(text)  # "olleH"
```

---

### str_repeat(s: str, count: int) -> str

Repeats a string count times.

**Parameters:**
- `s` (str): String to repeat
- `count` (int): Number of repetitions (0-1000)

**Returns:**
- Concatenated string

**Example:**
```python
let dash: str = str_repeat("-", 40)  # "----------------------------------------"
let pattern: str = str_repeat("AB", 5)  # "ABABABABAB"
```

---

### str_pad_left(s: str, width: int, pad: str) -> str

Pads string on left side to specified width.

**Parameters:**
- `s` (str): String to pad
- `width` (int): Target width
- `pad` (str): Padding character(s)

**Returns:**
- Padded string

**Example:**
```python
let num: str = "42"
let padded: str = str_pad_left(num, 5, "0")  # "00042"

let text: str = "Hi"
let centered: str = str_pad_left(text, 10, " ")  # "        Hi"
```

---

### str_pad_right(s: str, width: int, pad: str) -> str

Pads string on right side to specified width.

**Parameters:**
- `s` (str): String to pad
- `width` (int): Target width
- `pad` (str): Padding character(s)

**Returns:**
- Padded string

**Example:**
```python
let label: str = "Name:"
let padded: str = str_pad_right(label, 20, " ")
print(padded, "John Doe")  # "Name:                John Doe"
```

---

## Validation Functions

### is_digit(s: str) -> bool

Checks if all characters are digits.

**Example:**
```python
let valid: bool = is_digit("12345")  # true
let invalid: bool = is_digit("123a")  # false
```

---

### is_alpha(s: str) -> bool

Checks if all characters are alphabetic.

**Example:**
```python
let valid: bool = is_alpha("Hello")  # true
let invalid: bool = is_alpha("Hello123")  # false
```

---

### is_alnum(s: str) -> bool

Checks if all characters are alphanumeric.

**Example:**
```python
let valid: bool = is_alnum("user123")  # true
let invalid: bool = is_alnum("user_123")  # false
```

---

### is_space(s: str) -> bool

Checks if all characters are whitespace.

**Example:**
```python
let valid: bool = is_space("   ")  # true
let invalid: bool = is_space(" a ")  # false
```

---

## Conversion Functions

### int_to_hex(n: int) -> str

Converts integer to hexadecimal string.

**Example:**
```python
let hex: str = int_to_hex(255)  # "ff"
let hex2: str = int_to_hex(4096)  # "1000"
```

---

### hex_to_int(s: str) -> int

Converts hexadecimal string to integer.

**Example:**
```python
let num: int = hex_to_int("ff")  # 255
let num2: int = hex_to_int("1000")  # 4096
```

---

## Extended Math Functions

### clamp(value: int, min: int, max: int) -> int

Clamps value between min and max.

**Example:**
```python
let clamped: int = clamp(150, 0, 100)  # 100
let clamped2: int = clamp(-50, 0, 100)  # 0
let clamped3: int = clamp(50, 0, 100)  # 50
```

---

### sign(n: int) -> int

Returns sign of number (-1, 0, or 1).

**Example:**
```python
let s1: int = sign(42)  # 1
let s2: int = sign(-42)  # -1
let s3: int = sign(0)  # 0
```

---

## File Utility Functions

### file_size(path: str) -> int

Returns size of file in bytes.

**Example:**
```python
let size: int = file_size("data.txt")
if size == -1:
    print("File not found")
else:
    print("File size:", size, "bytes")
```

---

### file_copy(src: str, dst: str) -> bool

Copies file from src to dst.

**Example:**
```python
let ok: bool = file_copy("original.txt", "backup.txt")
if ok:
    print("File copied successfully")
else:
    print("Copy failed")
```

---

## Security Best Practices

### Password Storage
NEVER store passwords in plaintext:

```python
// BAD - Never do this
let password: str = "user_password"
file_write("passwords.txt", password)

// GOOD - Hash before storing
let password: str = read_line()
let hashed: str = hash_sha256(password)
file_write("password.hash", hashed)
```

### Token Generation
Generate secure random tokens:

```python
// BAD - Predictable
rand_seed(clock_ms())
let token: int = rand()

// GOOD - Cryptographically secure
let token: str = random_bytes(32)
file_write("api.token", token)
```

### Timing Attack Prevention
Always use secure_compare for sensitive data:

```python
// BAD - Timing attack vulnerable
if stored_token == user_token:
    grant_access()

// GOOD - Constant time comparison
if secure_compare(stored_token, user_token):
    grant_access()
```

---

## Performance Considerations

### Hashing
- SHA-256: O(n) where n is input length
- MD5: O(n) where n is input length
- Both suitable for moderate-size inputs (<1MB)
- For large files, consider streaming approach (not yet implemented)

### Random Bytes
- /dev/urandom: Fast, kernel-provided
- Fallback PRNG: Very fast but not cryptographically secure
- Limit to 1024 bytes per call to prevent memory issues

### String Operations
- str_reverse: O(n)
- str_repeat: O(n * count), limited to count <= 1000
- str_pad_left/right: O(width)
- Validation functions: O(n)

---

## Error Handling

All functions use fatal errors for invalid input:

```python
let hash: str = hash_sha256("")  // OK - hashes empty string

let bytes: str = random_bytes(2000)  // FATAL - exceeds 1024 limit

let hex: str = int_to_hex(value)  // OK for any int64_t value
```

Validate inputs before calling security functions:

```python
def safe_hash(data: str) -> str:
    if len(data) == 0:
        return "empty"
    if len(data) > 10000:
        return "too_large"
    return hash_sha256(data)
```

---

## Version History

v3.0 - Security Extensions
- Added hash_sha256, hash_md5
- Added secure_compare
- Added random_bytes
- Extended string utilities
- Validation functions
- Conversion functions

v2.0 - Production Edition
- 42 built-in functions
- Math, string, file operations

v1.5 - Divine Edition
- Fixed AND/OR, added ELIF
- Base64 encoding

---

## References

- NIST SP 800-131A: Cryptographic Algorithm Policy
- OWASP Cryptographic Storage Cheat Sheet
- Timing Attacks on Implementations of Diffie-Hellman, RSA, DSS
- RFC 4648: Base16, Base32, Base64 Encoding

---

Document Version: 3.0
Last Updated: February 2026
