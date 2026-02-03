BetterPython Programming Language
Version 2.0 Production Edition

Project Status: Production Ready
Standard Library: 42 Built-in Functions
Documentation: Comprehensive Technical Reference

QUICK START
-----------
Build: make
Run: ./bin/betterpython examples/hello.bp

FEATURES
--------
- Static typing with compile-time verification
- Python-inspired syntax with significant whitespace
- 42-function production standard library
- Fast bytecode compilation and execution
- Automatic garbage collection
- Comprehensive documentation

STRUCTURE
---------
bin/         Compiled binaries
docs/        Complete technical documentation
examples/    Sample programs (beginner/intermediate/advanced)
src/         Compiler and VM source code
tests/       Test suites
tools/       Development utilities

DOCUMENTATION
-------------
README.md                     This file
docs/TECHNICAL_REFERENCE.md   Complete language specification
docs/API_REFERENCE.md         Detailed API documentation
docs/TESTING_GUIDE.md         Testing methodology
STDLIB_API.md                 User-friendly stdlib reference

STANDARD LIBRARY
----------------
I/O: print, read_line
Strings: len, substr, to_str, chr, ord, str_upper, str_lower, str_trim,
         starts_with, ends_with, str_find, str_replace
Math: abs, min, max, pow, sqrt, floor, ceil, round
Random: rand, rand_range, rand_seed
Encoding: base64_encode, base64_decode
Files: file_read, file_write, file_append, file_exists, file_delete
Time: clock_ms, sleep
System: getenv, exit

LIMITATIONS
-----------
- No user-defined function calls (architectural)
- No arrays/lists
- No for loops (use while)
- Single-file compilation only
- ASCII only (no Unicode)
- Integer arithmetic only (no floats)

Despite limitations, all 42 built-in functions work perfectly, and the
language is suitable for educational use and small utility scripts.

See documentation in docs/ for complete details.
