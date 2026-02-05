#include "util.h"
#include <string.h>
#include <stdarg.h>

void *bp_xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) bp_fatal("out of memory");
    return p;
}

void *bp_xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) bp_fatal("out of memory");
    return q;
}

char *bp_xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = bp_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

void bp_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void bp_fatal_at(size_t line, const char *fmt, ...) {
    fprintf(stderr, "line %zu: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
