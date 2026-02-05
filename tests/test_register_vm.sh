#!/bin/bash
# Register VM test suite
# Tests the register-based VM implementation

set -e

BPCC="./bin/bpcc"
BPVM="./bin/bpvm"

echo "===== Register VM Test Suite ====="
echo

PASS=0
FAIL=0

run_test() {
    name="$1"
    expected="$2"
    src="$3"

    echo -n "Testing $name... "

    # Write test file
    echo "$src" > /tmp/test_reg.bp

    # Compile with register VM
    if ! $BPCC /tmp/test_reg.bp -o /tmp/test_reg.bpc --register 2>/dev/null; then
        echo "FAIL (compile error)"
        FAIL=$((FAIL + 1))
        return
    fi

    # Run
    actual=$($BPVM /tmp/test_reg.bpc 2>&1) || true

    if [ "$actual" = "$expected" ]; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL (expected: '$expected', got: '$actual')"
        FAIL=$((FAIL + 1))
    fi
}

# Basic arithmetic
run_test "addition" "30" '
def main() -> int:
    let x: int = 10
    let y: int = 20
    print(x + y)
    return 0
'

run_test "subtraction" "5" '
def main() -> int:
    print(15 - 10)
    return 0
'

run_test "multiplication" "56" '
def main() -> int:
    print(7 * 8)
    return 0
'

run_test "division" "4" '
def main() -> int:
    print(20 / 5)
    return 0
'

run_test "modulo" "1" '
def main() -> int:
    print(10 % 3)
    return 0
'

# Control flow
run_test "if-true" "yes" '
def main() -> int:
    if 1 == 1:
        print("yes")
    else:
        print("no")
    return 0
'

run_test "if-false" "no" '
def main() -> int:
    if 1 == 2:
        print("yes")
    else:
        print("no")
    return 0
'

run_test "while-loop" "5" '
def main() -> int:
    let i: int = 0
    while i < 5:
        i = i + 1
    print(i)
    return 0
'

run_test "for-loop" "10" '
def main() -> int:
    let sum: int = 0
    for i in range(0, 5):
        sum = sum + i
    print(sum)
    return 0
'

# Function calls
run_test "function-call" "25" '
def square(x: int) -> int:
    return x * x

def main() -> int:
    print(square(5))
    return 0
'

run_test "recursion" "120" '
def factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)

def main() -> int:
    print(factorial(5))
    return 0
'

# Strings
run_test "string-print" "hello" '
def main() -> int:
    print("hello")
    return 0
'

run_test "string-concat" "hello world" '
def main() -> int:
    let a: str = "hello"
    let b: str = " world"
    print(a + b)
    return 0
'

# Comparisons
run_test "less-than" "true" '
def main() -> int:
    if 5 < 10:
        print("true")
    else:
        print("false")
    return 0
'

run_test "greater-than" "false" '
def main() -> int:
    if 5 > 10:
        print("true")
    else:
        print("false")
    return 0
'

# Boolean operations
run_test "and-true" "yes" '
def main() -> int:
    if true and true:
        print("yes")
    else:
        print("no")
    return 0
'

run_test "or-true" "yes" '
def main() -> int:
    if false or true:
        print("yes")
    else:
        print("no")
    return 0
'

# Arrays
run_test "array-create-get" "2" '
def main() -> int:
    let arr: [int] = [1, 2, 3]
    print(arr[1])
    return 0
'

run_test "array-set" "42" '
def main() -> int:
    let arr: [int] = [1, 2, 3]
    arr[1] = 42
    print(arr[1])
    return 0
'

# Floats
run_test "float-add" "5.5" '
def main() -> int:
    let x: float = 2.5
    let y: float = 3.0
    print(float_to_str(x + y))
    return 0
'

echo
echo "===== Results ====="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo

if [ $FAIL -eq 0 ]; then
    echo "All register VM tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi
