#!/usr/bin/env python3
# Fibonacci benchmark - tests recursive function calls
import time
import sys

def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

def main():
    n = 35
    if len(sys.argv) > 1:
        n = int(sys.argv[1])

    start = time.time()
    result = fib(n)
    elapsed = (time.time() - start) * 1000

    print(f"fib({n}) = {result}")
    print(f"Time: {elapsed:.2f} ms")

if __name__ == "__main__":
    main()
