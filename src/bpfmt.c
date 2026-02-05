/*
 * BetterPython Formatter (bpfmt)
 * Code formatter for BetterPython.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define VERSION "1.0.0"
#define MAX_LINE 8192
#define INDENT_SIZE 4

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static void sb_init(StringBuilder *sb) {
    sb->cap = 4096;
    sb->len = 0;
    sb->data = malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_append(StringBuilder *sb, const char *s) {
    size_t slen = strlen(s);
    if (sb->len + slen + 1 > sb->cap) {
        sb->cap = (sb->len + slen + 1) * 2;
        char *new_data = realloc(sb->data, sb->cap);
        if (!new_data) return;
        sb->data = new_data;
    }
    memcpy(sb->data + sb->len, s, slen + 1);
    sb->len += slen;
}

static void sb_append_char(StringBuilder *sb, char c) {
    char buf[2] = {c, '\0'};
    sb_append(sb, buf);
}

static void trim_trailing(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int count_leading_spaces(const char *s) {
    int count = 0;
    while (*s == ' ' || *s == '\t') {
        count += (*s == '\t') ? INDENT_SIZE : 1;
        s++;
    }
    return count;
}

static const char *skip_whitespace(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static bool is_block_start(const char *line) {
    const char *trimmed = skip_whitespace(line);

    // Lines ending with : start a block
    size_t len = strlen(trimmed);
    if (len > 0 && trimmed[len - 1] == ':') {
        // Check it's a real block starter
        if (strncmp(trimmed, "def ", 4) == 0) return true;
        if (strncmp(trimmed, "if ", 3) == 0) return true;
        if (strncmp(trimmed, "elif ", 5) == 0) return true;
        if (strncmp(trimmed, "else:", 5) == 0) return true;
        if (strncmp(trimmed, "while ", 6) == 0) return true;
        if (strncmp(trimmed, "for ", 4) == 0) return true;
        if (strncmp(trimmed, "try:", 4) == 0) return true;
        if (strncmp(trimmed, "catch ", 6) == 0) return true;
        if (strncmp(trimmed, "catch:", 6) == 0) return true;
        if (strncmp(trimmed, "finally:", 8) == 0) return true;
        if (strncmp(trimmed, "struct ", 7) == 0) return true;
        if (strncmp(trimmed, "class ", 6) == 0) return true;
    }
    return false;
}

static void format_line(const char *input, int indent, StringBuilder *output) {
    const char *trimmed = skip_whitespace(input);

    // Skip empty lines
    if (*trimmed == '\0') {
        sb_append_char(output, '\n');
        return;
    }

    // Add proper indentation
    for (int i = 0; i < indent; i++) {
        sb_append_char(output, ' ');
    }

    // Process the line content
    const char *p = trimmed;
    bool in_string = false;
    char string_char = 0;

    while (*p) {
        // Handle strings
        if (!in_string && (*p == '"' || *p == '\'')) {
            in_string = true;
            string_char = *p;
            sb_append_char(output, *p++);
            continue;
        }
        if (in_string) {
            if (*p == '\\' && p[1]) {
                sb_append_char(output, *p++);
                sb_append_char(output, *p++);
                continue;
            }
            if (*p == string_char) {
                in_string = false;
            }
            sb_append_char(output, *p++);
            continue;
        }

        // Normalize operators with spacing
        if (*p == '=' && p[1] == '=') {
            sb_append(output, " == ");
            p += 2;
            while (*p == ' ') p++;
            continue;
        }
        if (*p == '!' && p[1] == '=') {
            sb_append(output, " != ");
            p += 2;
            while (*p == ' ') p++;
            continue;
        }
        if (*p == '<' && p[1] == '=') {
            sb_append(output, " <= ");
            p += 2;
            while (*p == ' ') p++;
            continue;
        }
        if (*p == '>' && p[1] == '=') {
            sb_append(output, " >= ");
            p += 2;
            while (*p == ' ') p++;
            continue;
        }
        if (*p == '-' && p[1] == '>') {
            sb_append(output, " -> ");
            p += 2;
            while (*p == ' ') p++;
            continue;
        }

        // Single operators
        if (*p == '=' && p[-1] != '=' && p[-1] != '!' && p[-1] != '<' && p[-1] != '>') {
            // Assignment
            if (output->len > 0 && output->data[output->len - 1] != ' ') {
                sb_append_char(output, ' ');
            }
            sb_append_char(output, '=');
            sb_append_char(output, ' ');
            p++;
            while (*p == ' ') p++;
            continue;
        }

        // Comma spacing
        if (*p == ',') {
            sb_append(output, ", ");
            p++;
            while (*p == ' ') p++;
            continue;
        }

        // Colon in type annotation
        if (*p == ':' && p[1] != ':') {
            // Check if it's end of function def or type annotation
            const char *next = p + 1;
            while (*next == ' ') next++;
            if (*next == '\0' || *next == '\n') {
                // End of block header
                sb_append_char(output, ':');
            } else if (isalpha((unsigned char)*next) || *next == '[' || *next == '{') {
                // Type annotation
                sb_append(output, ": ");
            } else {
                sb_append_char(output, ':');
            }
            p++;
            while (*p == ' ') p++;
            continue;
        }

        sb_append_char(output, *p++);
    }

    sb_append_char(output, '\n');
}

static char *format_content(const char *content) {
    StringBuilder output;
    sb_init(&output);

    char line[MAX_LINE];
    const char *p = content;
    int indent = 0;
    int prev_indent = 0;

    while (*p) {
        // Read a line
        char *l = line;
        while (*p && *p != '\n' && l - line < MAX_LINE - 1) {
            *l++ = *p++;
        }
        *l = '\0';
        if (*p == '\n') p++;

        trim_trailing(line);
        const char *trimmed = skip_whitespace(line);

        // Determine indent level
        if (*trimmed == '\0') {
            // Empty line - preserve
            sb_append_char(&output, '\n');
            continue;
        }

        // Handle dedent keywords
        if (strncmp(trimmed, "elif ", 5) == 0 ||
            strncmp(trimmed, "else:", 5) == 0 ||
            strncmp(trimmed, "catch ", 6) == 0 ||
            strncmp(trimmed, "catch:", 6) == 0 ||
            strncmp(trimmed, "finally:", 8) == 0) {
            if (indent >= INDENT_SIZE) {
                indent -= INDENT_SIZE;
            }
        }

        // Use source indentation as hint
        int source_indent = count_leading_spaces(line);

        // Decrease indent if source has less
        if (source_indent < prev_indent && indent > 0) {
            int diff = (prev_indent - source_indent) / INDENT_SIZE;
            indent -= diff * INDENT_SIZE;
            if (indent < 0) indent = 0;
        }

        format_line(line, indent, &output);

        // Increase indent after block start
        if (is_block_start(line)) {
            indent += INDENT_SIZE;
        }

        prev_indent = source_indent;
    }

    char *result = output.data;
    output.data = NULL;  // Transfer ownership
    return result;
}

static int format_file(const char *path, bool in_place) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return 1;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return 1;
    }

    size_t read = fread(content, 1, (size_t)size, f);
    content[read] = '\0';
    fclose(f);

    // Format
    char *formatted = format_content(content);
    free(content);

    if (!formatted) {
        return 1;
    }

    if (in_place) {
        f = fopen(path, "w");
        if (!f) {
            fprintf(stderr, "Error: Cannot write file: %s\n", path);
            free(formatted);
            return 1;
        }
        fputs(formatted, f);
        fclose(f);
        printf("Formatted: %s\n", path);
    } else {
        printf("%s", formatted);
    }

    free(formatted);
    return 0;
}

static void print_usage(void) {
    printf("bpfmt - BetterPython Formatter v%s\n\n", VERSION);
    printf("Usage: bpfmt [options] <file.bp> [files...]\n\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help\n");
    printf("  -v, --version  Show version\n");
    printf("  -w, --write    Write result to file (in-place)\n\n");
    printf("Without -w, prints formatted code to stdout.\n\n");
    printf("Formatting rules:\n");
    printf("  - 4-space indentation\n");
    printf("  - Spaces around operators (=, ==, !=, etc.)\n");
    printf("  - Space after colons in type annotations\n");
    printf("  - Space after commas\n");
    printf("  - No trailing whitespace\n");
    printf("  - Consistent block indentation\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    bool in_place = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("bpfmt %s\n", VERSION);
            return 0;
        }
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--write") == 0) {
            in_place = true;
        }
    }

    int result = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (format_file(argv[i], in_place) != 0) {
                result = 1;
            }
        }
    }

    return result;
}
