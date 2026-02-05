// Prime sieve benchmark - tests loops and array operations
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

int count_primes(int limit) {
    bool *sieve = calloc(limit + 1, sizeof(bool));
    for (int i = 0; i <= limit; i++) sieve[i] = true;
    sieve[0] = sieve[1] = false;

    for (int i = 2; i * i <= limit; i++) {
        if (sieve[i]) {
            for (int j = i * i; j <= limit; j += i) {
                sieve[j] = false;
            }
        }
    }

    int count = 0;
    for (int i = 2; i <= limit; i++) {
        if (sieve[i]) count++;
    }

    free(sieve);
    return count;
}

int main(int argc, char **argv) {
    int limit = 1000000;
    if (argc > 1) limit = atoi(argv[1]);

    clock_t start = clock();
    int result = count_primes(limit);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Primes up to %d: %d\n", limit, result);
    printf("Time: %.2f ms\n", elapsed);
    return 0;
}
