# Contributing to BetterPython

Thank you for your interest in contributing to BetterPython! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Issue Guidelines](#issue-guidelines)
- [Feature Requests](#feature-requests)

---

## Code of Conduct

BetterPython is committed to providing a welcoming and inclusive environment. Please:

- Be respectful and considerate in all interactions
- Focus on constructive feedback
- Welcome newcomers and help them get started
- Respect differing viewpoints and experiences

---

## Getting Started

### Prerequisites

- **C11 compiler** - GCC 4.9+ or Clang 3.5+
- **Make** - Build system
- **Git** - Version control
- **Node.js 18+** - For TreeSitter parser (optional)

### Clone and Build

```bash
git clone https://github.com/th3f0rk/BetterPython.git
cd BetterPython
make
```

### Verify Installation

```bash
# Run a simple program
echo 'def main() -> int:
    print("Hello, World!")
    return 0' > test.bp
./bin/betterpython test.bp

# Run the test suite
./tests/test_suite.sh
```

---

## Development Setup

### Project Structure

```
BetterPython/
├── src/                    # Core compiler and VM
│   ├── lexer.c/h          # Tokenization
│   ├── parser.c/h         # AST construction
│   ├── ast.c/h            # AST definitions
│   ├── types.c/h          # Type checking
│   ├── compiler.c/h       # Bytecode generation
│   ├── bytecode.c/h       # Opcodes
│   ├── vm.c/h             # Virtual machine
│   ├── gc.c/h             # Garbage collector
│   ├── stdlib.c/h         # Built-in functions
│   └── bp_main.c          # Entry point
├── tests/                  # Test suite
├── examples/               # Example programs
├── docs/                   # Documentation
└── tools/                  # Development utilities
```

### Build Commands

```bash
make              # Standard build
make clean        # Clean build artifacts
make clean && make # Full rebuild
```

---

## Making Changes

### Workflow

1. **Fork** the repository
2. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   # or
   git checkout -b fix/your-bug-fix
   ```
3. **Make your changes**
4. **Test thoroughly**
5. **Commit with clear messages**
6. **Push to your fork**
7. **Open a Pull Request**

### Branch Naming

- `feature/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation updates
- `refactor/` - Code refactoring
- `test/` - Test additions/improvements

---

## Coding Standards

### C Code Style

**Indentation:**
- Use 4 spaces (no tabs)

**Braces:**
```c
// Functions
static Value do_something(VM *vm, int arg) {
    if (arg > 0) {
        return v_int(arg);
    } else {
        return v_nil();
    }
}
```

**Naming:**
- Functions: `snake_case` (e.g., `emit_expr`, `check_type`)
- Types: `PascalCase` (e.g., `Value`, `Token`, `Expr`)
- Constants/Enums: `UPPER_SNAKE_CASE` (e.g., `OP_ADD`, `TY_INT`)
- Local variables: `snake_case`

**Comments:**
```c
// Single line comment for short explanations

/*
 * Multi-line comment for longer explanations
 * that span multiple lines.
 */

// TODO: Short description of what needs to be done
```

**File Headers:**
```c
/*
 * filename.c - Brief description
 *
 * Part of BetterPython - https://github.com/th3f0rk/BetterPython
 */
```

### Memory Management

- Always check return values of memory allocations
- Use the garbage collector for heap-allocated values
- Free resources in reverse order of allocation
- Avoid memory leaks - run valgrind if available

### Error Handling

```c
// Return error codes or NULL on failure
if (!ptr) {
    error("Failed to allocate memory");
    return NULL;
}

// Use error() function for user-facing errors
if (type_mismatch) {
    error("Type error at line %d: expected %s, got %s",
          line, expected_type, actual_type);
}
```

---

## Testing

### Running Tests

```bash
# Run all tests
./tests/test_suite.sh

# Run specific test files
./bin/betterpython tests/test_arrays.bp
./bin/betterpython tests/test_floats.bp
./bin/betterpython tests/test_exceptions.bp
```

### Writing Tests

Create test files in `tests/` directory:

```python
# tests/test_new_feature.bp

def test_basic_case() -> int:
    # Setup
    let input: int = 42

    # Exercise
    let result: int = some_operation(input)

    # Verify
    if result == expected_value:
        print("PASS: basic_case")
        return 0
    else:
        print("FAIL: basic_case")
        return 1

def test_edge_case() -> int:
    # Test edge cases
    ...

def main() -> int:
    let failures: int = 0
    failures = failures + test_basic_case()
    failures = failures + test_edge_case()

    if failures == 0:
        print("All tests passed!")
    else:
        print("Tests failed:")
        print(failures)

    return failures
```

### Test Requirements

Before submitting a PR:
- [ ] All existing tests pass
- [ ] New features have corresponding tests
- [ ] Edge cases are covered
- [ ] Build completes with zero warnings

---

## Submitting Changes

### Commit Messages

Follow conventional commit format:

```
type: short description

Longer description if needed. Explain the why, not just the what.

Fixes #123
```

**Types:**
- `feat:` - New feature
- `fix:` - Bug fix
- `docs:` - Documentation
- `refactor:` - Code refactoring
- `test:` - Test additions
- `chore:` - Maintenance tasks

**Examples:**
```
feat: add string interpolation support

Implement f-string style interpolation using {expr} syntax.
Parser and compiler updated to handle embedded expressions.

fix: prevent stack overflow in recursive functions

Add depth limit checking in VM call stack. Throws exception
when exceeding 256 nested calls.

Fixes #42
```

### Pull Request Guidelines

**Title:** Clear, concise description of the change

**Description should include:**
- What changed and why
- How to test the changes
- Any breaking changes
- Related issues

**PR Checklist:**
- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] New tests added for new features
- [ ] Documentation updated if needed
- [ ] No compiler warnings
- [ ] Rebased on latest main

---

## Issue Guidelines

### Bug Reports

Include:
1. **Description** - What happened?
2. **Expected behavior** - What should happen?
3. **Reproduction steps** - How to reproduce?
4. **Environment** - OS, compiler version, etc.
5. **Minimal example** - Code that demonstrates the bug

**Template:**
```markdown
## Bug Description
Brief description of the bug.

## Expected Behavior
What should happen.

## Steps to Reproduce
1. Create file with this code:
   ```python
   def main() -> int:
       # problematic code
       return 0
   ```
2. Run: `./bin/betterpython file.bp`
3. See error

## Environment
- OS: Ubuntu 22.04
- Compiler: GCC 11.4
- BetterPython version: 1.0.0

## Additional Context
Any other relevant information.
```

### Feature Requests

Include:
1. **Problem** - What problem does this solve?
2. **Proposed solution** - How should it work?
3. **Alternatives** - Other approaches considered
4. **Use cases** - Examples of how it would be used

---

## Feature Requests

When proposing new features, consider:

### Language Features
- Does it fit BetterPython's design philosophy?
- Is it consistent with existing syntax?
- What's the implementation complexity?
- Are there performance implications?

### Standard Library
- Is this commonly needed?
- Does it duplicate existing functionality?
- What are the type signatures?
- Are there security considerations?

---

## Development Areas

### Current Priorities

We welcome contributions in these areas:

1. **Bug fixes** - Always welcome!
2. **Test coverage** - More tests = better stability
3. **Documentation** - Examples, tutorials, guides
4. **Performance** - VM optimizations
5. **Error messages** - More helpful diagnostics

### Planned Features (v1.1+)

See the [Roadmap](README.md#roadmap) for planned features.
Contributors interested in major features should open an issue first to discuss approach.

---

## Questions?

- **Issues:** https://github.com/th3f0rk/BetterPython/issues
- **Discussions:** https://github.com/th3f0rk/BetterPython/discussions

---

## Recognition

Contributors will be recognized in:
- Release notes for the version containing their contribution
- CONTRIBUTORS.md file (for significant contributions)

Thank you for helping make BetterPython better!

---

*BetterPython - The AI-Native Programming Language*
