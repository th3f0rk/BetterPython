#!/bin/bash
# BetterPython Benchmark Suite
# Compares compile time and runtime across multiple languages

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"
RESULTS_FILE="$SCRIPT_DIR/results.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Create build directory
mkdir -p "$BUILD_DIR"

# Initialize results file
cat > "$RESULTS_FILE" << 'EOF'
==================================================================
  BetterPython Benchmark Suite
==================================================================

EOF
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

print_header() {
    echo -e "\n${CYAN}${BOLD}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}${BOLD}  $1${NC}"
    echo -e "${CYAN}${BOLD}════════════════════════════════════════════════════════════════${NC}\n"
}

print_section() {
    echo -e "${BLUE}>>> $1${NC}"
}

print_result() {
    echo -e "  ${GREEN}$1${NC}"
}

print_error() {
    echo -e "  ${RED}$1${NC}"
}

# Check which tools are available
check_tools() {
    print_header "Checking Available Tools"

    echo "Checking compilers and interpreters..."

    HAS_GCC=$(command -v gcc &> /dev/null && echo 1 || echo 0)
    HAS_PYTHON3=$(command -v python3 &> /dev/null && echo 1 || echo 0)
    HAS_PYPY=$(command -v pypy3 &> /dev/null && echo 1 || echo 0)
    HAS_JAVA=$(command -v javac &> /dev/null && echo 1 || echo 0)
    HAS_ZIG=$(command -v zig &> /dev/null && echo 1 || echo 0)
    HAS_BP=1  # BetterPython is always available

    [ "$HAS_GCC" = "1" ] && print_result "GCC: $(gcc --version | head -1)" || print_error "GCC: not found"
    [ "$HAS_PYTHON3" = "1" ] && print_result "Python3: $(python3 --version)" || print_error "Python3: not found"
    [ "$HAS_PYPY" = "1" ] && print_result "PyPy: $(pypy3 --version 2>&1 | head -1)" || print_error "PyPy: not found"
    [ "$HAS_JAVA" = "1" ] && print_result "Java: $(java -version 2>&1 | head -1)" || print_error "Java: not found"
    [ "$HAS_ZIG" = "1" ] && print_result "Zig: $(zig version)" || print_error "Zig: not found"
    print_result "BetterPython: available"

    echo ""
}

# Extract time from output (looks for number before "ms")
extract_time() {
    echo "$1" | grep -oE '[0-9]+\.?[0-9]* ms' | head -1 | grep -oE '[0-9]+\.?[0-9]*' || echo "N/A"
}

# Run Fibonacci benchmark
run_fibonacci() {
    print_header "Fibonacci Benchmark (fib(35) - recursive)"

    echo "=== FIBONACCI BENCHMARK ===" >> "$RESULTS_FILE"
    printf "%-20s %15s %15s\n" "Language" "Compile(ms)" "Runtime(ms)" >> "$RESULTS_FILE"
    printf "%-20s %15s %15s\n" "--------" "-----------" "-----------" >> "$RESULTS_FILE"

    # C
    if [ "$HAS_GCC" = "1" ]; then
        print_section "C (gcc -O2)"
        local compile_start=$(date +%s%N)
        gcc -O2 -o "$BUILD_DIR/fib_c" "$SCRIPT_DIR/c/fibonacci.c"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        local output=$("$BUILD_DIR/fib_c" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s\n" "C (gcc -O2)" "$compile_time" "$run_time" >> "$RESULTS_FILE"
    fi

    # BetterPython
    print_section "BetterPython"
    local compile_start=$(date +%s%N)
    "$PROJECT_DIR/bin/bpcc" "$SCRIPT_DIR/betterpython/fibonacci.bp" -o "$BUILD_DIR/fib_bp.bpc"
    local compile_end=$(date +%s%N)
    local compile_time=$(( (compile_end - compile_start) / 1000000 ))

    local output=$("$PROJECT_DIR/bin/bpvm" "$BUILD_DIR/fib_bp.bpc" 2>&1)
    local run_time=$(echo "$output" | tail -1)
    print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
    printf "%-20s %15s %15s\n" "BetterPython" "$compile_time" "$run_time" >> "$RESULTS_FILE"

    # CPython
    if [ "$HAS_PYTHON3" = "1" ]; then
        print_section "CPython"
        local output=$(python3 "$SCRIPT_DIR/python/fibonacci.py" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: N/A (interpreted), Runtime: ${run_time}ms"
        printf "%-20s %15s %15s\n" "CPython" "N/A" "$run_time" >> "$RESULTS_FILE"
    fi

    # PyPy
    if [ "$HAS_PYPY" = "1" ]; then
        print_section "PyPy"
        # Warm up JIT
        pypy3 "$SCRIPT_DIR/python/fibonacci.py" 30 > /dev/null 2>&1 || true
        local output=$(pypy3 "$SCRIPT_DIR/python/fibonacci.py" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: N/A (JIT), Runtime: ${run_time}ms"
        printf "%-20s %15s %15s\n" "PyPy" "N/A" "$run_time" >> "$RESULTS_FILE"
    fi

    # Java
    if [ "$HAS_JAVA" = "1" ]; then
        print_section "Java"
        local compile_start=$(date +%s%N)
        javac -d "$BUILD_DIR" "$SCRIPT_DIR/java/Fibonacci.java"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        local output=$(java -cp "$BUILD_DIR" Fibonacci 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s\n" "Java" "$compile_time" "$run_time" >> "$RESULTS_FILE"
    fi

    # Zig
    if [ "$HAS_ZIG" = "1" ]; then
        print_section "Zig (ReleaseFast)"
        local compile_start=$(date +%s%N)
        cd "$BUILD_DIR"
        zig build-exe -O ReleaseFast "$SCRIPT_DIR/zig/fibonacci.zig" 2>/dev/null || true
        cd "$SCRIPT_DIR"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        if [ -f "$BUILD_DIR/fibonacci" ]; then
            local output=$("$BUILD_DIR/fibonacci" 2>&1)
            local run_time=$(extract_time "$output")
            print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
            printf "%-20s %15s %15s\n" "Zig" "$compile_time" "$run_time" >> "$RESULTS_FILE"
        else
            print_error "Compilation failed"
        fi
    fi

    echo "" >> "$RESULTS_FILE"
}

# Run Prime Sieve benchmark
run_primes() {
    print_header "Prime Sieve Benchmark"

    echo "=== PRIME SIEVE BENCHMARK ===" >> "$RESULTS_FILE"
    printf "%-20s %15s %15s %15s\n" "Language" "Limit" "Compile(ms)" "Runtime(ms)" >> "$RESULTS_FILE"
    printf "%-20s %15s %15s %15s\n" "--------" "-----" "-----------" "-----------" >> "$RESULTS_FILE"

    # C (1 million)
    if [ "$HAS_GCC" = "1" ]; then
        print_section "C (gcc -O2) - 1,000,000 primes"
        local compile_start=$(date +%s%N)
        gcc -O2 -o "$BUILD_DIR/primes_c" "$SCRIPT_DIR/c/primes.c"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        local output=$("$BUILD_DIR/primes_c" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s %15s\n" "C (gcc -O2)" "1000000" "$compile_time" "$run_time" >> "$RESULTS_FILE"
    fi

    # BetterPython (100k - smaller due to interpreted nature)
    print_section "BetterPython - 100,000 primes"
    local compile_start=$(date +%s%N)
    "$PROJECT_DIR/bin/bpcc" "$SCRIPT_DIR/betterpython/primes.bp" -o "$BUILD_DIR/primes_bp.bpc"
    local compile_end=$(date +%s%N)
    local compile_time=$(( (compile_end - compile_start) / 1000000 ))

    local output=$("$PROJECT_DIR/bin/bpvm" "$BUILD_DIR/primes_bp.bpc" 2>&1)
    local run_time=$(echo "$output" | tail -1)
    print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
    printf "%-20s %15s %15s %15s\n" "BetterPython" "100000" "$compile_time" "$run_time" >> "$RESULTS_FILE"

    # CPython (1 million)
    if [ "$HAS_PYTHON3" = "1" ]; then
        print_section "CPython - 1,000,000 primes"
        local output=$(python3 "$SCRIPT_DIR/python/primes.py" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: N/A, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s %15s\n" "CPython" "1000000" "N/A" "$run_time" >> "$RESULTS_FILE"
    fi

    # PyPy (1 million)
    if [ "$HAS_PYPY" = "1" ]; then
        print_section "PyPy - 1,000,000 primes"
        local output=$(pypy3 "$SCRIPT_DIR/python/primes.py" 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: N/A, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s %15s\n" "PyPy" "1000000" "N/A" "$run_time" >> "$RESULTS_FILE"
    fi

    # Java (1 million)
    if [ "$HAS_JAVA" = "1" ]; then
        print_section "Java - 1,000,000 primes"
        local compile_start=$(date +%s%N)
        javac -d "$BUILD_DIR" "$SCRIPT_DIR/java/Primes.java"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        local output=$(java -cp "$BUILD_DIR" Primes 2>&1)
        local run_time=$(extract_time "$output")
        print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
        printf "%-20s %15s %15s %15s\n" "Java" "1000000" "$compile_time" "$run_time" >> "$RESULTS_FILE"
    fi

    # Zig (1 million)
    if [ "$HAS_ZIG" = "1" ]; then
        print_section "Zig (ReleaseFast) - 1,000,000 primes"
        local compile_start=$(date +%s%N)
        cd "$BUILD_DIR"
        zig build-exe -O ReleaseFast "$SCRIPT_DIR/zig/primes.zig" 2>/dev/null || true
        cd "$SCRIPT_DIR"
        local compile_end=$(date +%s%N)
        local compile_time=$(( (compile_end - compile_start) / 1000000 ))

        if [ -f "$BUILD_DIR/primes" ]; then
            local output=$("$BUILD_DIR/primes" 2>&1)
            local run_time=$(extract_time "$output")
            print_result "Compile: ${compile_time}ms, Runtime: ${run_time}ms"
            printf "%-20s %15s %15s %15s\n" "Zig" "1000000" "$compile_time" "$run_time" >> "$RESULTS_FILE"
        else
            print_error "Compilation failed"
        fi
    fi

    echo "" >> "$RESULTS_FILE"
}

# Print summary
print_summary() {
    print_header "Benchmark Results"

    echo ""
    cat "$RESULTS_FILE"
    echo ""
    echo -e "${GREEN}Results saved to: $RESULTS_FILE${NC}"
}

# Main
main() {
    print_header "BetterPython Benchmark Suite"
    echo "Comparing compile time and runtime across languages"
    echo "Date: $(date)"

    # Make sure BetterPython is built
    if [ ! -f "$PROJECT_DIR/bin/bpcc" ]; then
        echo "Building BetterPython..."
        (cd "$PROJECT_DIR" && make)
    fi

    check_tools
    run_fibonacci
    run_primes
    print_summary
}

main "$@"
