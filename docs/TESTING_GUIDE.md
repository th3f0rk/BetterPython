BetterPython Testing and Validation Guide
Version 2.0
================================================================================

CONTENTS
1. Testing Philosophy
2. Test Organization
3. Unit Test Reference
4. Integration Testing
5. Performance Benchmarks
6. Security Testing
7. Continuous Integration
8. Test Coverage Analysis

================================================================================
1. TESTING PHILOSOPHY
================================================================================

Principles:
- Every language feature must have test coverage
- Tests must be deterministic and repeatable
- Tests should be self-documenting
- Failure messages must be actionable

Test Pyramid:
    Unit Tests (60%): Test individual functions and operations
    Integration Tests (30%): Test compilation pipeline and execution
    System Tests (10%): Test complete programs and use cases

================================================================================
2. TEST ORGANIZATION
================================================================================

Directory Structure:
    tests/
        unit/           # Unit tests for individual features
        integration/    # End-to-end compilation tests
        regression/     # Tests for fixed bugs
        performance/    # Performance benchmarks
        security/       # Security validation tests

Test Naming Convention:
    test_<feature>_<scenario>.bp
    
    Examples:
        test_arithmetic_basic.bp
        test_strings_concatenation.bp
        test_control_flow_nested_if.bp

Test File Structure:
    def main() -> int:
        # Arrange: Setup test data
        # Act: Execute operation
        # Assert: Verify results
        # Report: Print success/failure
        return 0  # 0 = success, 1 = failure

================================================================================
3. UNIT TEST REFERENCE
================================================================================

3.1 Arithmetic Operations
--------------------------

Test: test_arithmetic_addition.bp
def main() -> int:
    let result: int = 10 + 20
    if result == 30:
        print("PASS: Addition")
        return 0
    else:
        print("FAIL: Addition expected 30, got", result)
        return 1

Test: test_arithmetic_division_by_zero.bp
Verify: Program should terminate with fatal error
Expected output: "division by zero"

Test: test_arithmetic_modulo.bp
def main() -> int:
    let tests_passed: int = 0
    if 10 % 3 == 1:
        tests_passed = tests_passed + 1
    if 15 % 5 == 0:
        tests_passed = tests_passed + 1
    if tests_passed == 2:
        print("PASS: Modulo")
        return 0
    else:
        print("FAIL: Modulo")
        return 1

3.2 String Operations
----------------------

Test: test_string_concatenation.bp
def main() -> int:
    let s1: str = "Hello"
    let s2: str = "World"
    let combined: str = s1 + " " + s2
    if combined == "Hello World":
        print("PASS: String concatenation")
        return 0
    else:
        print("FAIL: String concatenation")
        return 1

Test: test_string_length.bp
def main() -> int:
    if len("") == 0 and len("test") == 4:
        print("PASS: String length")
        return 0
    else:
        print("FAIL: String length")
        return 1

Test: test_string_substring.bp
def main() -> int:
    let s: str = "Hello World"
    let sub1: str = substr(s, 0, 5)
    let sub2: str = substr(s, 6, 5)
    if sub1 == "Hello" and sub2 == "World":
        print("PASS: Substring")
        return 0
    else:
        print("FAIL: Substring")
        return 1

3.3 Boolean Logic
-----------------

Test: test_boolean_and.bp
def main() -> int:
    let passed: bool = true
    if not (true and true):
        passed = false
    if false and false:
        passed = false
    if true and false:
        passed = false
    if passed:
        print("PASS: AND operator")
        return 0
    else:
        print("FAIL: AND operator")
        return 1

Test: test_boolean_or.bp
def main() -> int:
    let passed: bool = true
    if not (true or false):
        passed = false
    if not (false or true):
        passed = false
    if false or false:
        passed = false
    if passed:
        print("PASS: OR operator")
        return 0
    else:
        print("FAIL: OR operator")
        return 1

Test: test_boolean_not.bp
def main() -> int:
    let t: bool = true
    let f: bool = false
    if not t or f:
        print("FAIL: NOT operator")
        return 1
    if not f and t:
        print("PASS: NOT operator")
        return 0
    print("FAIL: NOT operator")
    return 1

3.4 Control Flow
----------------

Test: test_if_basic.bp
def main() -> int:
    let x: int = 10
    let passed: bool = false
    if x == 10:
        passed = true
    if passed:
        print("PASS: Basic if")
        return 0
    else:
        print("FAIL: Basic if")
        return 1

Test: test_elif_chain.bp
def main() -> int:
    let x: int = 15
    let result: str = ""
    if x < 10:
        result = "small"
    elif x < 20:
        result = "medium"
    else:
        result = "large"
    if result == "medium":
        print("PASS: Elif chain")
        return 0
    else:
        print("FAIL: Elif chain, got", result)
        return 1

Test: test_while_loop.bp
def main() -> int:
    let sum: int = 0
    let i: int = 1
    while i <= 10:
        sum = sum + i
        i = i + 1
    if sum == 55:
        print("PASS: While loop")
        return 0
    else:
        print("FAIL: While loop, expected 55, got", sum)
        return 1

3.5 Type System
---------------

Test: test_type_checking_mismatch.bp
Expected: Compilation error "type error: assigning int to str"
def main() -> int:
    let x: str = "test"
    x = 42  # Should fail type checking
    return 0

Test: test_type_inference.bp
Verify: Compiler correctly infers expression types
def main() -> int:
    let x: int = 10 + 20
    let s: str = "Hello" + "World"
    let b: bool = true and false
    print("PASS: Type inference")
    return 0

================================================================================
4. INTEGRATION TESTING
================================================================================

4.1 Compilation Pipeline
-------------------------

Test: Compile valid program
Command: ./bin/bpcc test.bp -o test.bpc
Expected: Exit code 0, test.bpc created

Test: Compile invalid syntax
Command: ./bin/bpcc invalid.bp -o out.bpc
Expected: Exit code 1, error message printed

Test: Compile type error
Command: ./bin/bpcc typeerror.bp -o out.bpc
Expected: Exit code 1, type error message

4.2 Execution Pipeline
-----------------------

Test: Execute compiled program
Command: ./bin/bpvm program.bpc
Expected: Program output, exit code 0

Test: Execute non-existent file
Command: ./bin/bpvm nonexistent.bpc
Expected: Error message, non-zero exit

4.3 End-to-End Workflow
------------------------

Test: Complete workflow
#!/bin/bash
echo 'def main() -> int:' > test.bp
echo '    print("Hello")' >> test.bp
echo '    return 0' >> test.bp
./bin/bpcc test.bp -o test.bpc
./bin/bpvm test.bpc
RESULT=$?
rm test.bp test.bpc
exit $RESULT

Expected: "Hello" printed, exit code 0

================================================================================
5. PERFORMANCE BENCHMARKS
================================================================================

5.1 Arithmetic Benchmark
-------------------------

test_perf_arithmetic.bp:
def main() -> int:
    let start: int = clock_ms()
    let sum: int = 0
    let i: int = 0
    while i < 1000000:
        sum = sum + i
        i = i + 1
    let end: int = clock_ms()
    print("Arithmetic (1M ops):", end - start, "ms")
    return 0

Baseline: ~50-100ms on modern hardware

5.2 String Operations Benchmark
--------------------------------

test_perf_strings.bp:
def main() -> int:
    let start: int = clock_ms()
    let s: str = ""
    let i: int = 0
    while i < 1000:
        s = s + "x"
        i = i + 1
    let end: int = clock_ms()
    print("String concat (1K ops):", end - start, "ms")
    print("Final length:", len(s))
    return 0

Baseline: ~10-50ms (quadratic complexity)

5.3 Function Call Overhead
---------------------------

test_perf_builtins.bp:
def main() -> int:
    let start: int = clock_ms()
    let i: int = 0
    while i < 10000:
        let x: int = abs(-42)
        i = i + 1
    let end: int = clock_ms()
    print("Builtin calls (10K):", end - start, "ms")
    return 0

Baseline: ~5-20ms

5.4 I/O Performance
-------------------

test_perf_file_io.bp:
def main() -> int:
    let data: str = "x"
    let i: int = 0
    while i < 100:
        data = data + data
        i = i + 1
    let start: int = clock_ms()
    let ok: bool = file_write("/tmp/perf_test.txt", data)
    let read_data: str = file_read("/tmp/perf_test.txt")
    let end: int = clock_ms()
    file_delete("/tmp/perf_test.txt")
    print("File I/O (large file):", end - start, "ms")
    print("Data size:", len(data))
    return 0

Baseline: Depends on disk speed, typically 10-100ms for MB-sized files

================================================================================
6. SECURITY TESTING
================================================================================

6.1 Input Validation
--------------------

Test: Buffer overflow in string operations
Strategy: Attempt to create extremely large strings
Result: Should hit memory limit and terminate gracefully

Test: Integer overflow
def main() -> int:
    let max: int = 9223372036854775807
    let overflow: int = max + 1
    print("Overflow result:", overflow)
    return 0

Expected: Silent wraparound (implementation-defined behavior)

6.2 File System Security
-------------------------

Test: Path traversal attempt
def main() -> int:
    let ok: bool = file_read("../../../../etc/passwd")
    return 0

Expected: Either succeeds (security issue) or fails with error
Note: BetterPython does not sanitize paths

Test: Writing to restricted locations
Result: Depends on process permissions

6.3 Resource Exhaustion
------------------------

Test: Memory exhaustion
def main() -> int:
    let s: str = "x"
    while true:
        s = s + s
    return 0

Expected: Eventually hits memory limit and terminates

Test: Infinite loop
def main() -> int:
    while true:
        let x: int = 42
    return 0

Expected: Runs forever (user must kill process)

================================================================================
7. CONTINUOUS INTEGRATION
================================================================================

7.1 Build Verification
-----------------------

Build Script (build_and_test.sh):
#!/bin/bash
set -e

# Clean build
make clean
make

# Run unit tests
for test in tests/unit/*.bp; do
    echo "Testing: $test"
    ./bin/betterpython "$test" || exit 1
done

# Run integration tests
for test in tests/integration/*.bp; do
    echo "Testing: $test"
    ./bin/betterpython "$test" || exit 1
done

echo "All tests passed!"

7.2 Regression Testing
-----------------------

After every bug fix:
1. Add test case reproducing the bug
2. Verify test fails on old code
3. Verify test passes on fixed code
4. Add to regression suite

Regression test naming:
    test_regression_issue_<number>.bp
    
Example: test_regression_issue_001_dedent_bug.bp

7.3 Code Quality Checks
------------------------

Compile with warnings:
    make CFLAGS="-O2 -Wall -Wextra -Werror -std=c11"

Static analysis:
    cppcheck --enable=all src/

Memory leak detection:
    valgrind --leak-check=full ./bin/bpvm program.bpc

================================================================================
8. TEST COVERAGE ANALYSIS
================================================================================

8.1 Feature Coverage Matrix
----------------------------

Language Feature | Test Coverage | Status
-----------------|---------------|--------
Basic arithmetic | Yes           | Pass
String operations| Yes           | Pass
Boolean logic    | Yes           | Pass
If statements    | Yes           | Pass
Elif statements  | Yes           | Pass
While loops      | Yes           | Pass
Variable binding | Yes           | Pass
Type checking    | Yes           | Pass
Builtins         | Partial       | Pass
File I/O         | Yes           | Pass
Error handling   | Partial       | Pass

8.2 Code Coverage
-----------------

Target: 80% line coverage in core modules

Tools:
    gcc -fprofile-arcs -ftest-coverage
    gcov src/*.c
    lcov for HTML reports

8.3 Edge Cases
--------------

Tested:
- Empty strings
- Zero values
- Maximum integers
- Null returns
- File not found
- Permission denied

Not tested:
- Unicode edge cases (limited support)
- Platform-specific behavior
- Concurrent access (single-threaded)

================================================================================
9. TEST AUTOMATION
================================================================================

9.1 Test Runner Script
-----------------------

run_tests.sh:
#!/bin/bash
PASS=0
FAIL=0

for test in tests/**/*.bp; do
    if ./bin/betterpython "$test" > /dev/null 2>&1; then
        echo "PASS: $test"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $test"
        FAIL=$((FAIL + 1))
    fi
done

echo "Results: $PASS passed, $FAIL failed"
exit $FAIL

9.2 Performance Tracking
-------------------------

Track performance over time:
1. Run benchmark suite
2. Log results with timestamp
3. Compare against baseline
4. Alert on >10% regression

9.3 Nightly Builds
------------------

Schedule:
- Clean checkout
- Full rebuild
- Run all tests
- Run benchmarks
- Generate coverage report
- Email results

================================================================================
10. DEBUGGING TESTS
================================================================================

10.1 Verbose Mode
-----------------

Debug failing test:
    ./bin/betterpython -v test.bp

View generated bytecode:
    ./bin/bpcc test.bp -o test.bpc --dump-bytecode

10.2 Common Failures
--------------------

Symptom: Test hangs
Cause: Infinite loop in test code
Solution: Add timeout to test runner

Symptom: Segmentation fault
Cause: Stack overflow or memory corruption
Solution: Run under valgrind

Symptom: Incorrect output
Cause: Logic error in test or implementation
Solution: Add print statements for debugging

================================================================================
END OF TESTING GUIDE
================================================================================
