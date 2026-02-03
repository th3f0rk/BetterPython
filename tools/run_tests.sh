#!/bin/bash

# BetterPython Test Runner
# Executes all test files and reports results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BETTERPYTHON="$PROJECT_ROOT/bin/betterpython"
TEST_DIR="$PROJECT_ROOT/tests"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "BetterPython Test Suite"
echo "======================="
echo ""

# Check if betterpython exists
if [ ! -f "$BETTERPYTHON" ]; then
    echo -e "${RED}Error: betterpython not found at $BETTERPYTHON${NC}"
    echo "Run 'make' first to build the project"
    exit 1
fi

# Count tests
total_tests=0
passed_tests=0
failed_tests=0

# Function to run a test
run_test() {
    local test_file=$1
    local test_name=$(basename "$test_file" .bp)
    
    echo -n "Running $test_name... "
    
    if output=$("$BETTERPYTHON" "$test_file" 2>&1); then
        if echo "$output" | grep -q "FAIL"; then
            echo -e "${RED}FAIL${NC}"
            echo "$output"
            ((failed_tests++))
        else
            echo -e "${GREEN}PASS${NC}"
            ((passed_tests++))
        fi
    else
        echo -e "${RED}ERROR${NC}"
        echo "$output"
        ((failed_tests++))
    fi
    
    ((total_tests++))
}

# Run all tests
for test_file in "$TEST_DIR"/*.bp; do
    if [ -f "$test_file" ]; then
        run_test "$test_file"
    fi
done

echo ""
echo "======================="
echo "Results: $passed_tests/$total_tests tests passed"

if [ $failed_tests -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}$failed_tests tests failed${NC}"
    exit 1
fi
