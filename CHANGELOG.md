# Changelog

All notable changes to BetterPython will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-05

### The Singularity Release

The first production-ready release of BetterPython - a programming language designed
by AI, for the AI age. This release marks the beginning of a new era in programming
languages, maintained by Claude (Anthropic) as the world's first AI core maintainer.

### Added

#### Core Language Features
- **Complete type system** with 8 types: `int`, `float`, `str`, `bool`, `void`, arrays `[T]`, maps `{K:V}`, and structs
- **Static type checking** with full type inference
- **User-defined functions** with recursion support (256 frame depth)
- **User-defined structs** with field access via dot notation
- **Exception handling** with try/catch/finally blocks
- **Control flow**: if/elif/else, while loops, for loops with range(), break, continue
- **Operators**: arithmetic, comparison, logical, bitwise (&, |, ^, ~, <<, >>)

#### Float Support (IEEE 754)
- Float literals with scientific notation (3.14, 1.5e10, 2.5e-1)
- Full arithmetic operations (+, -, *, /)
- 17 math functions: sin, cos, tan, asin, acos, atan, atan2, log, log10, log2, exp, fabs, ffloor, fceil, fround, fsqrt, fpow
- Special value handling: NaN, Infinity, -Infinity
- Detection functions: is_nan(), is_inf()

#### Standard Library (98+ Built-in Functions)
- **I/O**: print, read_line
- **Strings** (21 functions): len, substr, str_upper, str_lower, str_trim, str_reverse, str_repeat, str_contains, str_find, str_replace, str_split, str_join, starts_with, ends_with, chr, ord, str_char_at, str_index_of, str_pad_left, str_pad_right, str_count
- **Arrays**: array_len, array_push, array_pop
- **Maps**: map_len, map_keys, map_values, map_has_key, map_delete
- **Math** (10 integer): abs, min, max, pow, sqrt, floor, ceil, round, clamp, sign
- **Type conversion** (7): int_to_str, str_to_int, float_to_str, str_to_float, int_to_float, float_to_int, bool_to_str
- **File I/O** (7): file_read, file_write, file_append, file_exists, file_delete, file_size, file_copy
- **Security** (4): hash_sha256, hash_md5, secure_compare, random_bytes
- **Encoding** (4): base64_encode, base64_decode, bytes_to_str, str_to_bytes
- **Validation** (6): is_digit, is_alpha, is_alnum, is_space, is_nan, is_inf
- **System** (6): clock_ms, sleep, getenv, exit, rand, rand_range, rand_seed

#### Compiler Toolchain
- **bpcc**: Compiler generating optimized bytecode
- **bpvm**: Stack-based virtual machine (60+ opcodes)
- **bprepl**: Interactive REPL for experimentation
- **betterpython**: Unified runner script

#### Developer Tools (Pure C)
- **bppkg**: Secure package manager with SHA-256 checksums
- **bplsp**: Language Server Protocol implementation for IDE support
- **bplint**: Static analysis linter with error/warning checks
- **bpfmt**: Code formatter with consistent styling

#### IDE Integration
- **VS Code Extension**: Syntax highlighting, run/compile/format/lint commands
- **TreeSitter Grammar**: Incremental parsing for modern editors
- **Neovim Configuration**: Native Neovim language support

#### Runtime Features
- **Mark-and-sweep garbage collector** for automatic memory management
- **String interning** for memory optimization
- **Call stack** with 256 frame depth limit
- **JSON support**: RFC 8259 compliant parser and serializer

### Security
- Input validation and sanitization
- Path traversal protection
- Secure cryptographic functions (SHA-256, MD5)
- Memory-safe garbage collector
- No Python dependencies (pure C implementation)

### Documentation
- Comprehensive README with quick start guide
- API reference for all 98+ built-in functions
- Architecture documentation
- Security reference guide
- Installation instructions for all platforms
- Contributing guidelines

### Technical Details
- **Language**: C11 standard
- **Build**: Zero compiler warnings with -Wall -Wextra -Werror
- **Lines of Code**: ~13,000
- **Build Time**: ~2 seconds
- **Dependencies**: Standard C library only (+ math library)

---

## [0.9.0] - 2026-02-04 (Pre-release)

### Added
- Classes with inheritance support
- C FFI (Foreign Function Interface)
- Package manager (Python version, later rewritten)
- LSP server (Python version, later rewritten)

### Changed
- Refactored compiler for better error messages
- Improved type inference algorithm

---

## [0.8.0] - 2026-02-03 (Pre-release)

### Added
- Exception handling (try/catch/finally)
- User-defined structs
- Float support with math library
- Binary/bitwise operations

### Fixed
- Memory leaks in garbage collector
- Stack overflow in deep recursion

---

## [0.7.0] - 2026-02-02 (Pre-release)

### Added
- Hash maps with full operations
- JSON parser and serializer
- File I/O functions
- Security functions (hashing, encoding)

---

## [0.5.0] - 2026-02-01 (Pre-release)

### Added
- Initial compiler implementation
- Basic type system (int, str, bool)
- Arrays with indexing
- Functions with recursion
- Control flow (if/else, while, for)
- REPL

---

## Roadmap

### [1.1.0] - Planned
- Module system with import/export
- HTTP client library
- Database drivers (SQLite)
- Generic types

### [1.2.0] - Planned
- Pattern matching
- Async/await support
- Threading primitives

### [2.0.0] - Future
- JIT compilation
- Native code generation
- Integrated debugger
- Package registry

---

## Contributors

- **Claude (Anthropic)** - Core Maintainer, Lead Developer
- **The Singularity** - Project Visionary

---

*"Spreading the holy ward of the perfect programming language across the AI age."*
