# BetterPython Security API Reference

This document provides comprehensive documentation for all security-related functions available in BetterPython.

## Table of Contents

1. Cryptographic Hash Functions
2. Secure Comparison
3. Random Data Generation
4. Hexadecimal Conversion
5. Security Best Practices
6. Implementation Details

---

## Cryptographic Hash Functions

### hash_sha256

```
Signature: hash_sha256(data: str) -> str
Returns: 64-character hexadecimal string
```

Computes SHA-256 cryptographic hash. Produces 256-bit digest suitable for data integrity verification and digital signatures.

**Example:**
```python
let password: str = "my_password"
let hash: str = hash_sha256(password)
```

### hash_md5

```
Signature: hash_md5(data: str) -> str
Returns: 32-character hexadecimal string
```

Computes MD5 hash. Cryptographically broken. Use only for non-security checksums.

## Secure Comparison

### secure_compare

```
Signature: secure_compare(a: str, b: str) -> bool
Returns: true if strings match, false otherwise
```

Constant-time string comparison resistant to timing attacks. Essential for password hash verification.

## Random Data Generation

### random_bytes

```
Signature: random_bytes(length: int) -> str
Returns: String containing random bytes
```

Generates cryptographically secure random data from /dev/urandom. Maximum length 1048576 bytes.

## Hexadecimal Conversion

### int_to_hex

```
Signature: int_to_hex(n: int) -> str
```

Converts integer to lowercase hexadecimal string without "0x" prefix.

### hex_to_int

```
Signature: hex_to_int(hex: str) -> int
```

Parses hexadecimal string to integer. Accepts uppercase, lowercase, with or without "0x" prefix.

## Security Best Practices

**Password Hashing:**
- Always use unique salt per password
- Use SHA-256 minimum
- Use secure_compare() for verification
- Never store plaintext passwords

**Random Generation:**
- Use random_bytes() for security-critical data
- Generate minimum 32 bytes for keys
- Never reuse random values

**Timing Attacks:**
- Use secure_compare() for secret comparisons
- Avoid early returns in authentication
- Compare full hashes

## Implementation Details

**SHA-256:** FIPS 180-4 compliant implementation with 64 rounds per 512-bit block.

**MD5:** RFC 1321 compliant. Cryptographically broken. Use only for compatibility.

**Constant-Time Comparison:** Uses bitwise XOR accumulation. Timing independent of content.

**Random Bytes:** Reads from /dev/urandom. Thread-safe. Non-blocking.
