/*
 * BetterPython Linter (bplint)
 * Static analysis tool for BetterPython code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define VERSION "1.0.0"
#define MAX_LINE 4096
#define MAX_ISSUES 1000

typedef enum {
    ISSUE_ERROR,
    ISSUE_WARNING,
    ISSUE_INFO
} IssueSeverity;

typedef struct {
    int line;
    int column;
    IssueSeverity severity;
    char code[16];
    char message[256];
} Issue;

static Issue issues[MAX_ISSUES];
static int issue_count = 0;

static void add_issue(int line, int col, IssueSeverity sev, const char *code, const char *msg) {
    if (issue_count >= MAX_ISSUES) return;
    Issue *i = &issues[issue_count++];
    i->line = line;
    i->column = col;
    i->severity = sev;
    strncpy(i->code, code, sizeof(i->code) - 1);
    strncpy(i->message, msg, sizeof(i->message) - 1);
}

static const char *sev_str(IssueSeverity s) {
    switch (s) {
        case ISSUE_ERROR: return "error";
        case ISSUE_WARNING: return "warning";
        case ISSUE_INFO: return "info";
    }
    return "unknown";
}

static void check_line(const char *line, int line_num) {
    size_t len = strlen(line);

    // Check line length
    if (len > 100) {
        add_issue(line_num, 100, ISSUE_WARNING, "W001", "Line exceeds 100 characters");
    }

    // Check for trailing whitespace
    if (len > 0 && isspace((unsigned char)line[len - 1])) {
        add_issue(line_num, (int)len, ISSUE_WARNING, "W002", "Trailing whitespace");
    }

    // Check for tabs
    const char *tab = strchr(line, '\t');
    if (tab) {
        add_issue(line_num, (int)(tab - line + 1), ISSUE_WARNING, "W003", "Tab character found, use spaces");
    }

    // Check function definitions
    const char *def = strstr(line, "def ");
    if (def) {
        // Check for return type
        if (!strstr(def, "->")) {
            add_issue(line_num, (int)(def - line + 1), ISSUE_ERROR, "E001", "Function missing return type annotation");
        }
        // Check for colon
        if (!strchr(def, ':')) {
            add_issue(line_num, (int)(def - line + 1), ISSUE_ERROR, "E002", "Function definition missing colon");
        }
    }

    // Check variable declarations
    const char *let = strstr(line, "let ");
    if (let) {
        // Check for type annotation
        const char *eq = strchr(let, '=');
        const char *colon = strchr(let, ':');
        if (eq && (!colon || colon > eq)) {
            add_issue(line_num, (int)(let - line + 1), ISSUE_ERROR, "E003", "Variable missing type annotation");
        }
    }

    // Check for == with strings (common mistake)
    if (strstr(line, "== \"") || strstr(line, "==\"")) {
        // This is actually fine in BetterPython, but note it
    }

    // Check for print statements without parentheses (Python habit)
    const char *p = line;
    while ((p = strstr(p, "print ")) != NULL) {
        // Check if it's print( or print followed by something
        const char *after = p + 6;
        while (*after && isspace((unsigned char)*after)) after++;
        if (*after && *after != '(') {
            add_issue(line_num, (int)(p - line + 1), ISSUE_INFO, "I001", "Consider using print() with parentheses");
        }
        p++;
    }

    // Check for Python-style comments (# is fine, but // is not)
    if (strstr(line, "//")) {
        add_issue(line_num, (int)(strstr(line, "//") - line + 1), ISSUE_WARNING, "W004", "Use # for comments, not //");
    }

    // Check for consistent indentation
    int spaces = 0;
    for (const char *c = line; *c && *c == ' '; c++) spaces++;
    if (spaces > 0 && spaces % 4 != 0) {
        add_issue(line_num, 1, ISSUE_WARNING, "W005", "Indentation should be a multiple of 4 spaces");
    }
}

static int lint_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return 1;
    }

    issue_count = 0;
    char line[MAX_LINE];
    int line_num = 1;

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

        check_line(line, line_num);
        line_num++;
    }

    fclose(f);

    // Print results
    if (issue_count == 0) {
        printf("%s: No issues found\n", path);
        return 0;
    }

    int errors = 0, warnings = 0, infos = 0;
    for (int i = 0; i < issue_count; i++) {
        Issue *iss = &issues[i];
        printf("%s:%d:%d: %s [%s]: %s\n",
            path, iss->line, iss->column,
            sev_str(iss->severity), iss->code, iss->message);

        switch (iss->severity) {
            case ISSUE_ERROR: errors++; break;
            case ISSUE_WARNING: warnings++; break;
            case ISSUE_INFO: infos++; break;
        }
    }

    printf("\n%s: %d error(s), %d warning(s), %d info(s)\n",
        path, errors, warnings, infos);

    return errors > 0 ? 1 : 0;
}

static void print_usage(void) {
    printf("bplint - BetterPython Linter v%s\n\n", VERSION);
    printf("Usage: bplint [options] <file.bp> [files...]\n\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help\n");
    printf("  -v, --version  Show version\n\n");
    printf("Checks:\n");
    printf("  E001  Function missing return type annotation\n");
    printf("  E002  Function definition missing colon\n");
    printf("  E003  Variable missing type annotation\n");
    printf("  W001  Line exceeds 100 characters\n");
    printf("  W002  Trailing whitespace\n");
    printf("  W003  Tab character found\n");
    printf("  W004  Use # for comments, not //\n");
    printf("  W005  Inconsistent indentation\n");
    printf("  I001  Style suggestion\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        printf("bplint %s\n", VERSION);
        return 0;
    }

    int result = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (lint_file(argv[i]) != 0) {
                result = 1;
            }
        }
    }

    return result;
}
