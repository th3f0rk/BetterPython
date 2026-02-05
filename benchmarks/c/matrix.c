// Matrix multiplication benchmark - tests nested loops and arithmetic
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void matrix_mult(int n, double **a, double **b, double **c) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            c[i][j] = 0;
            for (int k = 0; k < n; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

double **alloc_matrix(int n) {
    double **m = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        m[i] = malloc(n * sizeof(double));
    }
    return m;
}

void free_matrix(int n, double **m) {
    for (int i = 0; i < n; i++) free(m[i]);
    free(m);
}

int main(int argc, char **argv) {
    int n = 200;
    if (argc > 1) n = atoi(argv[1]);

    double **a = alloc_matrix(n);
    double **b = alloc_matrix(n);
    double **c = alloc_matrix(n);

    // Initialize matrices
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            a[i][j] = (double)(i + j);
            b[i][j] = (double)(i - j);
        }
    }

    clock_t start = clock();
    matrix_mult(n, a, b, c);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Matrix %dx%d multiplication\n", n, n);
    printf("c[0][0] = %.2f, c[n-1][n-1] = %.2f\n", c[0][0], c[n-1][n-1]);
    printf("Time: %.2f ms\n", elapsed);

    free_matrix(n, a);
    free_matrix(n, b);
    free_matrix(n, c);
    return 0;
}
