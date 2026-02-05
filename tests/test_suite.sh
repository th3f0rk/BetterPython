#!/bin/bash
# BetterPython Comprehensive Test Suite
# The Singularity - Release v1.0.0
#
# Usage: ./tests/test_suite.sh [options]
#   -v, --verbose    Show detailed output
#   -q, --quiet      Only show summary
#   -f, --fail-fast  Stop on first failure

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BIN_DIR="$PROJECT_ROOT/bin"
BPCC="$BIN_DIR/bpcc"
BPVM="$BIN_DIR/bpvm"
BPLINT="$BIN_DIR/bplint"
BPFMT="$BIN_DIR/bpfmt"

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Options
VERBOSE=false
QUIET=false
FAIL_FAST=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -q|--quiet)
            QUIET=true
            shift
            ;;
        -f|--fail-fast)
            FAIL_FAST=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Print functions
print_header() {
    if [ "$QUIET" = false ]; then
        echo ""
        echo -e "${CYAN}${BOLD}═══════════════════════════════════════════════════════════════${NC}"
        echo -e "${CYAN}${BOLD}  BetterPython Test Suite - The Singularity${NC}"
        echo -e "${CYAN}${BOLD}  Release v1.0.0${NC}"
        echo -e "${CYAN}${BOLD}═══════════════════════════════════════════════════════════════${NC}"
        echo ""
    fi
}

print_section() {
    if [ "$QUIET" = false ]; then
        echo ""
        echo -e "${BLUE}${BOLD}>>> $1${NC}"
        echo -e "${BLUE}───────────────────────────────────────────────────────────────${NC}"
    fi
}

print_test() {
    local name=$1
    local status=$2

    if [ "$QUIET" = false ]; then
        if [ "$status" = "PASS" ]; then
            echo -e "  ${GREEN}[PASS]${NC} $name"
        elif [ "$status" = "FAIL" ]; then
            echo -e "  ${RED}[FAIL]${NC} $name"
        elif [ "$status" = "SKIP" ]; then
            echo -e "  ${YELLOW}[SKIP]${NC} $name"
        fi
    fi
}

print_verbose() {
    if [ "$VERBOSE" = true ]; then
        echo -e "         ${YELLOW}$1${NC}"
    fi
}

# Test functions
check_prerequisites() {
    print_section "Checking Prerequisites"

    local prereq_ok=true

    # Check compiler
    if [ -x "$BPCC" ]; then
        print_test "Compiler (bpcc)" "PASS"
    else
        print_test "Compiler (bpcc)" "FAIL"
        prereq_ok=false
    fi

    # Check VM
    if [ -x "$BPVM" ]; then
        print_test "Virtual Machine (bpvm)" "PASS"
    else
        print_test "Virtual Machine (bpvm)" "FAIL"
        prereq_ok=false
    fi

    # Check linter
    if [ -x "$BPLINT" ]; then
        print_test "Linter (bplint)" "PASS"
    else
        print_test "Linter (bplint) - optional" "SKIP"
    fi

    # Check formatter
    if [ -x "$BPFMT" ]; then
        print_test "Formatter (bpfmt)" "PASS"
    else
        print_test "Formatter (bpfmt) - optional" "SKIP"
    fi

    if [ "$prereq_ok" = false ]; then
        echo ""
        echo -e "${RED}Prerequisites not met. Run 'make' first.${NC}"
        exit 1
    fi
}

# Run a single test file
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .bp)
    local tmp_bytecode="/tmp/${test_name}_$$.bpc"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    # Compile
    if ! "$BPCC" "$test_file" -o "$tmp_bytecode" 2>/dev/null; then
        print_test "$test_name" "FAIL"
        print_verbose "Compilation failed"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        rm -f "$tmp_bytecode"
        if [ "$FAIL_FAST" = true ]; then
            return 1
        fi
        return 0
    fi

    # Execute
    local output
    if output=$("$BPVM" "$tmp_bytecode" 2>&1); then
        # Check for FAIL in output
        if echo "$output" | grep -qi "FAIL"; then
            print_test "$test_name" "FAIL"
            print_verbose "Test reported failure"
            if [ "$VERBOSE" = true ]; then
                echo "$output" | grep -i "FAIL" | head -5 | while read line; do
                    echo -e "         ${RED}$line${NC}"
                done
            fi
            FAILED_TESTS=$((FAILED_TESTS + 1))
        else
            print_test "$test_name" "PASS"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        fi
    else
        print_test "$test_name" "FAIL"
        print_verbose "Runtime error"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi

    rm -f "$tmp_bytecode"

    if [ "$FAIL_FAST" = true ] && [ "$FAILED_TESTS" -gt 0 ]; then
        return 1
    fi
    return 0
}

# Run compiler validation tests
run_compiler_tests() {
    print_section "Compiler Validation"

    # Test: Valid minimal program compiles and runs
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    local minimal="/tmp/minimal_$$.bp"
    echo "def main() -> int:" > "$minimal"
    echo "    return 0" >> "$minimal"
    if "$BPCC" "$minimal" -o /tmp/minimal.bpc 2>/dev/null; then
        if "$BPVM" /tmp/minimal.bpc 2>/dev/null; then
            print_test "Minimal program compiles and runs" "PASS"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            print_test "Minimal program compiles and runs" "FAIL"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    else
        print_test "Minimal program compiles and runs" "FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    rm -f "$minimal" /tmp/minimal.bpc

    # Test: Program with syntax error fails
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    local syntax_err="/tmp/syntax_err_$$.bp"
    echo "def main( -> int:" > "$syntax_err"
    echo "    return 0" >> "$syntax_err"
    if ! "$BPCC" "$syntax_err" -o /tmp/syntax_err.bpc 2>/dev/null; then
        print_test "Reject syntax error" "PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        print_test "Reject syntax error" "FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    rm -f "$syntax_err" /tmp/syntax_err.bpc

    # Test: Program with type error fails
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    local type_err="/tmp/type_err_$$.bp"
    echo "def main() -> int:" > "$type_err"
    echo "    let x: int = \"hello\"" >> "$type_err"
    echo "    return 0" >> "$type_err"
    if ! "$BPCC" "$type_err" -o /tmp/type_err.bpc 2>/dev/null; then
        print_test "Reject type error" "PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        print_test "Reject type error" "FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    rm -f "$type_err" /tmp/type_err.bpc
}

# Run language feature tests
run_feature_tests() {
    print_section "Language Feature Tests"

    # Run all .bp files in tests directory
    for test_file in "$SCRIPT_DIR"/*.bp; do
        if [ -f "$test_file" ]; then
            run_test "$test_file" || true
        fi
    done
}

# Run tool tests
run_tool_tests() {
    print_section "Tool Tests"

    # Test bplint
    if [ -x "$BPLINT" ]; then
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        local lint_test="/tmp/lint_test_$$.bp"
        echo "def main() -> int:" > "$lint_test"
        echo "    return 0" >> "$lint_test"
        if "$BPLINT" "$lint_test" >/dev/null 2>&1; then
            print_test "Linter runs on valid code" "PASS"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            print_test "Linter runs on valid code" "FAIL"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
        rm -f "$lint_test"
    else
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        print_test "Linter" "SKIP"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
    fi

    # Test bpfmt
    if [ -x "$BPFMT" ]; then
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        local fmt_test="/tmp/fmt_test_$$.bp"
        echo "def main()->int:" > "$fmt_test"
        echo "    return 0" >> "$fmt_test"
        if "$BPFMT" "$fmt_test" >/dev/null 2>&1; then
            print_test "Formatter runs on valid code" "PASS"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            print_test "Formatter runs on valid code" "FAIL"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
        rm -f "$fmt_test"
    else
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        print_test "Formatter" "SKIP"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
    fi
}

# Run example tests (compile and run examples)
run_example_tests() {
    print_section "Example Programs"

    local examples_dir="$PROJECT_ROOT/examples"
    if [ -d "$examples_dir" ]; then
        # Test a few key examples
        for example in "hello.bp" "minimal.bp" "fizzbuzz.bp"; do
            local example_file="$examples_dir/$example"
            if [ -f "$example_file" ]; then
                run_test "$example_file" || true
            fi
        done
    fi
}

# Print summary
print_summary() {
    echo ""
    echo -e "${CYAN}${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}${BOLD}  Test Summary${NC}"
    echo -e "${CYAN}${BOLD}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "  Total:   ${BOLD}$TOTAL_TESTS${NC}"
    echo -e "  Passed:  ${GREEN}${BOLD}$PASSED_TESTS${NC}"
    echo -e "  Failed:  ${RED}${BOLD}$FAILED_TESTS${NC}"
    echo -e "  Skipped: ${YELLOW}${BOLD}$SKIPPED_TESTS${NC}"
    echo ""

    if [ "$FAILED_TESTS" -eq 0 ]; then
        echo -e "${GREEN}${BOLD}  ALL TESTS PASSED!${NC}"
        echo ""
        echo -e "  ${CYAN}The Singularity approves. BetterPython is ready.${NC}"
    else
        echo -e "${RED}${BOLD}  SOME TESTS FAILED${NC}"
        echo ""
        echo -e "  ${RED}Fix the failures before release.${NC}"
    fi
    echo ""
    echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
}

# Main
main() {
    print_header
    check_prerequisites
    run_compiler_tests
    run_feature_tests
    run_tool_tests
    run_example_tests
    print_summary

    # Exit with appropriate code
    if [ "$FAILED_TESTS" -gt 0 ]; then
        exit 1
    fi
    exit 0
}

main "$@"
