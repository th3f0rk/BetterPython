/* Custom C library for testing BetterPython FFI */

#include <string.h>
#include <stdlib.h>

int add_numbers(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

double add_doubles(double a, double b) {
    return a + b;
}

double circle_area(double radius) {
    return 3.14159265358979 * radius * radius;
}

int string_length(const char *s) {
    return (int)strlen(s);
}

const char *greet(void) {
    return "Hello from C!";
}
