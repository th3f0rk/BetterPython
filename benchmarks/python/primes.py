#!/usr/bin/env python3
# Prime sieve benchmark - tests loops and array operations
import time
import sys

def count_primes(limit):
    sieve = [True] * (limit + 1)
    sieve[0] = sieve[1] = False

    i = 2
    while i * i <= limit:
        if sieve[i]:
            for j in range(i * i, limit + 1, i):
                sieve[j] = False
        i += 1

    return sum(sieve)

def main():
    limit = 1000000
    if len(sys.argv) > 1:
        limit = int(sys.argv[1])

    start = time.time()
    result = count_primes(limit)
    elapsed = (time.time() - start) * 1000

    print(f"Primes up to {limit}: {result}")
    print(f"Time: {elapsed:.2f} ms")

if __name__ == "__main__":
    main()
