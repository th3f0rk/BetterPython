#pragma once
#include <stdio.h>
#include <stdlib.h>

void *bp_xmalloc(size_t n);
void *bp_xrealloc(void *p, size_t n);
char *bp_xstrdup(const char *s);

void bp_fatal(const char *fmt, ...);
void bp_fatal_at(size_t line, const char *fmt, ...);
