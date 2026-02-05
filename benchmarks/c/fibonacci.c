// Fibonacci benchmark - tests recursive function calls
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

long long fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv) {
    int n = 35;
    if (argc > 1) n = atoi(argv[1]);

    clock_t start = clock();
    long long result = fib(n);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("fib(%d) = %lld\n", n, result);
    printf("Time: %.2f ms\n", elapsed);
    return 0;
}
