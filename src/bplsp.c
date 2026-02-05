/*
 * BetterPython Language Server (bplsp)
 * A minimal LSP server implementation in C.
 *
 * Supports:
 * - textDocument/didOpen
 * - textDocument/didChange
 * - textDocument/completion
 * - textDocument/hover
 * - textDocument/definition
 * - textDocument/publishDiagnostics
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

#define VERSION "1.0.0"
#define MAX_LINE 65536
#define MAX_CONTENT 1048576
#define MAX_SYMBOLS 1024
#define MAX_URI_LEN 4096

// ============================================================================
// JSON Utilities (Simple)
// ============================================================================

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

// Skip whitespace
static const char *json_skip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

// Parse string (returns malloc'd string)
static char *json_parse_string(const char **p) {
    const char *s = *p;
    if (*s != '"') return NULL;
    s++;

    const char *start = s;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1]) s += 2;
        else s++;
    }

    size_t len = (size_t)(s - start);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    // Copy with escape handling
    char *d = result;
    s = start;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1]) {
            s++;
            switch (*s) {
                case 'n': *d++ = '\n'; break;
                case 't': *d++ = '\t'; break;
                case 'r': *d++ = '\r'; break;
                case '"': *d++ = '"'; break;
                case '\\': *d++ = '\\'; break;
                default: *d++ = *s; break;
            }
            s++;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0';

    if (*s == '"') s++;
    *p = s;
    return result;
}

// Get value of a key in JSON object (returns pointer to value start)
static const char *json_get(const char *json, const char *key) {
    if (!json || !key) return NULL;

    size_t key_len = strlen(key);
    const char *p = json;

    while (*p) {
        p = json_skip(p);
        if (*p == '"') {
            const char *ks = p + 1;
            const char *ke = strchr(ks, '"');
            if (ke && (size_t)(ke - ks) == key_len && strncmp(ks, key, key_len) == 0) {
                p = json_skip(ke + 1);
                if (*p == ':') {
                    return json_skip(p + 1);
                }
            }
        }
        p++;
    }
    return NULL;
}

// Get string value
static char *json_get_string(const char *json, const char *key) {
    const char *v = json_get(json, key);
    if (!v) return NULL;
    return json_parse_string(&v);
}

// Get integer value
static int json_get_int(const char *json, const char *key) {
    const char *v = json_get(json, key);
    if (!v) return 0;
    return atoi(v);
}

// ============================================================================
// Built-in Functions Database
// ============================================================================

typedef struct {
    const char *name;
    const char *signature;
    const char *doc;
} BuiltinFunc;

static const BuiltinFunc BUILTINS[] = {
    {"print", "print(...) -> void", "Print values to stdout"},
    {"read_line", "read_line() -> str", "Read line from stdin"},
    {"len", "len(s: str) -> int", "Get length of string"},
    {"substr", "substr(s: str, start: int, len: int) -> str", "Extract substring"},
    {"str_upper", "str_upper(s: str) -> str", "Convert to uppercase"},
    {"str_lower", "str_lower(s: str) -> str", "Convert to lowercase"},
    {"str_trim", "str_trim(s: str) -> str", "Remove whitespace"},
    {"str_reverse", "str_reverse(s: str) -> str", "Reverse string"},
    {"str_contains", "str_contains(s: str, needle: str) -> bool", "Check if contains"},
    {"str_find", "str_find(s: str, needle: str) -> int", "Find position"},
    {"str_replace", "str_replace(s: str, old: str, new: str) -> str", "Replace first"},
    {"str_split", "str_split(s: str, delim: str) -> [str]", "Split string"},
    {"abs", "abs(n: int) -> int", "Absolute value"},
    {"min", "min(a: int, b: int) -> int", "Minimum"},
    {"max", "max(a: int, b: int) -> int", "Maximum"},
    {"pow", "pow(base: int, exp: int) -> int", "Power"},
    {"sqrt", "sqrt(n: int) -> int", "Integer square root"},
    {"rand", "rand() -> int", "Random 0-32767"},
    {"rand_range", "rand_range(min: int, max: int) -> int", "Random in range"},
    {"array_len", "array_len(arr: [T]) -> int", "Get array length"},
    {"array_push", "array_push(arr: [T], val: T) -> void", "Push to array"},
    {"array_pop", "array_pop(arr: [T]) -> T", "Pop from array"},
    {"map_keys", "map_keys(m: {K: V}) -> [K]", "Get map keys"},
    {"map_values", "map_values(m: {K: V}) -> [V]", "Get map values"},
    {"map_has_key", "map_has_key(m: {K: V}, key: K) -> bool", "Check key exists"},
    {"file_read", "file_read(path: str) -> str", "Read file"},
    {"file_write", "file_write(path: str, data: str) -> bool", "Write file"},
    {"file_exists", "file_exists(path: str) -> bool", "Check file exists"},
    {"hash_sha256", "hash_sha256(data: str) -> str", "SHA-256 hash"},
    {"base64_encode", "base64_encode(data: str) -> str", "Encode base64"},
    {"base64_decode", "base64_decode(data: str) -> str", "Decode base64"},
    {"clock_ms", "clock_ms() -> int", "Current time in ms"},
    {"sleep", "sleep(ms: int) -> void", "Sleep milliseconds"},
    {"exit", "exit(code: int) -> void", "Exit program"},
    {NULL, NULL, NULL}
};

static const char *KEYWORDS[] = {
    "def", "let", "if", "elif", "else", "while", "for", "in",
    "return", "and", "or", "not", "true", "false", "try", "catch",
    "finally", "throw", "struct", "class", "import", "export",
    NULL
};

static const char *TYPES[] = {
    "int", "str", "bool", "void", "float",
    NULL
};

// ============================================================================
// Symbol Table
// ============================================================================

typedef enum {
    SYM_FUNCTION,
    SYM_VARIABLE,
    SYM_PARAMETER
} SymbolKind;

typedef struct {
    char name[128];
    SymbolKind kind;
    char type[64];
    char signature[256];
    int line;
    int column;
} Symbol;

typedef struct {
    Symbol symbols[MAX_SYMBOLS];
    int count;
} SymbolTable;

// ============================================================================
// Document Store
// ============================================================================

typedef struct {
    char uri[MAX_URI_LEN];
    char *content;
    size_t content_len;
    SymbolTable symbols;
} Document;

#define MAX_DOCUMENTS 64
static Document documents[MAX_DOCUMENTS];
static int doc_count = 0;

static Document *find_document(const char *uri) {
    for (int i = 0; i < doc_count; i++) {
        if (strcmp(documents[i].uri, uri) == 0) {
            return &documents[i];
        }
    }
    return NULL;
}

static Document *add_document(const char *uri) {
    if (doc_count >= MAX_DOCUMENTS) return NULL;

    Document *doc = &documents[doc_count++];
    memset(doc, 0, sizeof(Document));
    strncpy(doc->uri, uri, MAX_URI_LEN - 1);
    return doc;
}

static void update_document(Document *doc, const char *content) {
    free(doc->content);
    doc->content_len = strlen(content);
    doc->content = malloc(doc->content_len + 1);
    if (doc->content) {
        strcpy(doc->content, content);
    }

    // Parse symbols
    doc->symbols.count = 0;

    const char *p = content;
    int line = 0;

    while (*p) {
        // Skip whitespace at start of line
        const char *line_start = p;
        while (*p && *p != '\n') {
            // Look for function definitions
            if (strncmp(p, "def ", 4) == 0 && doc->symbols.count < MAX_SYMBOLS) {
                p += 4;
                while (isspace((unsigned char)*p)) p++;

                Symbol *sym = &doc->symbols.symbols[doc->symbols.count];
                sym->kind = SYM_FUNCTION;
                sym->line = line;
                sym->column = (int)(p - line_start);

                // Extract name
                char *n = sym->name;
                while (isalnum((unsigned char)*p) || *p == '_') {
                    if (n - sym->name < 127) *n++ = *p;
                    p++;
                }
                *n = '\0';

                // Extract signature until ':'
                char *s = sym->signature;
                *s++ = '(';
                while (*p && *p != ':' && *p != '\n') {
                    if (s - sym->signature < 255) *s++ = *p;
                    p++;
                }
                *s = '\0';

                doc->symbols.count++;
            }
            // Look for variable declarations
            else if (strncmp(p, "let ", 4) == 0 && doc->symbols.count < MAX_SYMBOLS) {
                p += 4;
                while (isspace((unsigned char)*p)) p++;

                Symbol *sym = &doc->symbols.symbols[doc->symbols.count];
                sym->kind = SYM_VARIABLE;
                sym->line = line;
                sym->column = (int)(p - line_start);

                char *n = sym->name;
                while (isalnum((unsigned char)*p) || *p == '_') {
                    if (n - sym->name < 127) *n++ = *p;
                    p++;
                }
                *n = '\0';

                // Get type
                if (*p == ':') {
                    p++;
                    while (isspace((unsigned char)*p)) p++;
                    char *t = sym->type;
                    while (isalnum((unsigned char)*p) || *p == '[' || *p == ']' || *p == '{' || *p == '}' || *p == ':') {
                        if (t - sym->type < 63) *t++ = *p;
                        p++;
                    }
                    *t = '\0';
                }

                doc->symbols.count++;
            }
            else {
                p++;
            }
        }

        if (*p == '\n') {
            p++;
            line++;
        }
    }
}

// ============================================================================
// LSP Response Builders
// ============================================================================

static char response_buf[MAX_CONTENT];

static void send_response(const char *content) {
    size_t len = strlen(content);
    printf("Content-Length: %zu\r\n\r\n%s", len, content);
    fflush(stdout);
}

static void send_result(int id, const char *result) {
    snprintf(response_buf, sizeof(response_buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
        id, result);
    send_response(response_buf);
}

// send_notification can be used for diagnostics publishing
static void send_notification(const char *method, const char *params) {
    snprintf(response_buf, sizeof(response_buf),
        "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s}",
        method, params);
    send_response(response_buf);
}

// Publish empty diagnostics to clear any previous errors
static void publish_diagnostics(const char *uri) {
    char params[1024];
    snprintf(params, sizeof(params), "{\"uri\":\"%s\",\"diagnostics\":[]}", uri);
    send_notification("textDocument/publishDiagnostics", params);
}

// ============================================================================
// LSP Handlers
// ============================================================================

static void handle_initialize(int id) {
    const char *result =
        "{"
        "\"capabilities\":{"
            "\"textDocumentSync\":{\"openClose\":true,\"change\":1},"
            "\"completionProvider\":{\"triggerCharacters\":[\".\",\"(\"]},"
            "\"hoverProvider\":true,"
            "\"definitionProvider\":true"
        "},"
        "\"serverInfo\":{\"name\":\"bplsp\",\"version\":\"" VERSION "\"}"
        "}";
    send_result(id, result);
}

static void handle_shutdown(int id) {
    send_result(id, "null");
}

static void handle_did_open(const char *params) {
    const char *td = json_get(params, "textDocument");
    if (!td) return;

    char *uri = json_get_string(td, "uri");
    char *text = json_get_string(td, "text");

    if (uri && text) {
        Document *doc = find_document(uri);
        if (!doc) doc = add_document(uri);
        if (doc) {
            update_document(doc, text);
            publish_diagnostics(uri);
        }
    }

    free(uri);
    free(text);
}

static void handle_did_change(const char *params) {
    const char *td = json_get(params, "textDocument");
    if (!td) return;

    char *uri = json_get_string(td, "uri");
    if (!uri) return;

    Document *doc = find_document(uri);
    if (!doc) {
        free(uri);
        return;
    }

    // Get content changes
    const char *changes = json_get(params, "contentChanges");
    if (changes) {
        // Find first { after [
        const char *p = strchr(changes, '{');
        if (p) {
            char *text = json_get_string(p, "text");
            if (text) {
                update_document(doc, text);
                free(text);
            }
        }
    }

    free(uri);
}

static void handle_completion(int id, const char *params) {
    (void)params;

    // Build completion items
    char items[MAX_CONTENT] = "[";
    size_t pos = 1;

    // Add built-in functions
    for (int i = 0; BUILTINS[i].name; i++) {
        if (pos > 1) items[pos++] = ',';
        int n = snprintf(items + pos, sizeof(items) - pos,
            "{\"label\":\"%s\",\"kind\":3,\"detail\":\"%s\",\"documentation\":\"%s\"}",
            BUILTINS[i].name, BUILTINS[i].signature, BUILTINS[i].doc);
        if (n > 0) pos += (size_t)n;
    }

    // Add keywords
    for (int i = 0; KEYWORDS[i]; i++) {
        if (pos > 1) items[pos++] = ',';
        int n = snprintf(items + pos, sizeof(items) - pos,
            "{\"label\":\"%s\",\"kind\":14}",
            KEYWORDS[i]);
        if (n > 0) pos += (size_t)n;
    }

    // Add types
    for (int i = 0; TYPES[i]; i++) {
        if (pos > 1) items[pos++] = ',';
        int n = snprintf(items + pos, sizeof(items) - pos,
            "{\"label\":\"%s\",\"kind\":7}",
            TYPES[i]);
        if (n > 0) pos += (size_t)n;
    }

    items[pos++] = ']';
    items[pos] = '\0';

    send_result(id, items);
}

static void handle_hover(int id, const char *params) {
    const char *td = json_get(params, "textDocument");
    const char *pos_json = json_get(params, "position");

    if (!td || !pos_json) {
        send_result(id, "null");
        return;
    }

    char *uri = json_get_string(td, "uri");
    int line = json_get_int(pos_json, "line");
    int character = json_get_int(pos_json, "character");

    if (!uri) {
        send_result(id, "null");
        return;
    }

    Document *doc = find_document(uri);
    free(uri);

    if (!doc || !doc->content) {
        send_result(id, "null");
        return;
    }

    // Find word at position
    const char *p = doc->content;
    int cur_line = 0;
    while (*p && cur_line < line) {
        if (*p == '\n') cur_line++;
        p++;
    }

    // Move to character position
    for (int i = 0; i < character && *p && *p != '\n'; i++) p++;

    // Find word boundaries
    while (p > doc->content && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) p--;
    const char *word_start = p;
    while (isalnum((unsigned char)*p) || *p == '_') p++;
    size_t word_len = (size_t)(p - word_start);

    if (word_len == 0) {
        send_result(id, "null");
        return;
    }

    char word[128];
    if (word_len >= sizeof(word)) word_len = sizeof(word) - 1;
    memcpy(word, word_start, word_len);
    word[word_len] = '\0';

    // Check built-ins
    for (int i = 0; BUILTINS[i].name; i++) {
        if (strcmp(word, BUILTINS[i].name) == 0) {
            snprintf(response_buf, sizeof(response_buf),
                "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```betterpython\\n%s\\n```\\n\\n%s\"}}",
                BUILTINS[i].signature, BUILTINS[i].doc);
            send_result(id, response_buf);
            return;
        }
    }

    // Check document symbols
    for (int i = 0; i < doc->symbols.count; i++) {
        if (strcmp(word, doc->symbols.symbols[i].name) == 0) {
            Symbol *sym = &doc->symbols.symbols[i];
            if (sym->kind == SYM_FUNCTION) {
                snprintf(response_buf, sizeof(response_buf),
                    "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```betterpython\\ndef %s%s\\n```\"}}",
                    sym->name, sym->signature);
            } else {
                snprintf(response_buf, sizeof(response_buf),
                    "{\"contents\":{\"kind\":\"markdown\",\"value\":\"```betterpython\\n%s: %s\\n```\"}}",
                    sym->name, sym->type[0] ? sym->type : "unknown");
            }
            send_result(id, response_buf);
            return;
        }
    }

    send_result(id, "null");
}

static void handle_definition(int id, const char *params) {
    const char *td = json_get(params, "textDocument");
    const char *pos_json = json_get(params, "position");

    if (!td || !pos_json) {
        send_result(id, "null");
        return;
    }

    char *uri = json_get_string(td, "uri");
    int line = json_get_int(pos_json, "line");
    int character = json_get_int(pos_json, "character");

    if (!uri) {
        send_result(id, "null");
        return;
    }

    Document *doc = find_document(uri);

    if (!doc || !doc->content) {
        free(uri);
        send_result(id, "null");
        return;
    }

    // Find word at position
    const char *p = doc->content;
    int cur_line = 0;
    while (*p && cur_line < line) {
        if (*p == '\n') cur_line++;
        p++;
    }

    for (int i = 0; i < character && *p && *p != '\n'; i++) p++;

    while (p > doc->content && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) p--;
    const char *word_start = p;
    while (isalnum((unsigned char)*p) || *p == '_') p++;
    size_t word_len = (size_t)(p - word_start);

    if (word_len == 0) {
        free(uri);
        send_result(id, "null");
        return;
    }

    char word[128];
    if (word_len >= sizeof(word)) word_len = sizeof(word) - 1;
    memcpy(word, word_start, word_len);
    word[word_len] = '\0';

    // Find symbol definition
    for (int i = 0; i < doc->symbols.count; i++) {
        if (strcmp(word, doc->symbols.symbols[i].name) == 0) {
            Symbol *sym = &doc->symbols.symbols[i];
            snprintf(response_buf, sizeof(response_buf),
                "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}",
                uri, sym->line, sym->column, sym->line, sym->column + (int)strlen(sym->name));
            free(uri);
            send_result(id, response_buf);
            return;
        }
    }

    free(uri);
    send_result(id, "null");
}

// ============================================================================
// Main Loop
// ============================================================================

static void process_message(const char *content) {
    const char *method_val = json_get(content, "method");
    int id = json_get_int(content, "id");

    char method[128] = "";
    if (method_val && *method_val == '"') {
        const char *p = method_val;
        char *m = json_parse_string(&p);
        if (m) {
            strncpy(method, m, sizeof(method) - 1);
            free(m);
        }
    }

    const char *params = json_get(content, "params");

    if (strcmp(method, "initialize") == 0) {
        handle_initialize(id);
    } else if (strcmp(method, "initialized") == 0) {
        // No response needed
    } else if (strcmp(method, "shutdown") == 0) {
        handle_shutdown(id);
    } else if (strcmp(method, "exit") == 0) {
        exit(0);
    } else if (strcmp(method, "textDocument/didOpen") == 0) {
        handle_did_open(params);
    } else if (strcmp(method, "textDocument/didChange") == 0) {
        handle_did_change(params);
    } else if (strcmp(method, "textDocument/completion") == 0) {
        handle_completion(id, params);
    } else if (strcmp(method, "textDocument/hover") == 0) {
        handle_hover(id, params);
    } else if (strcmp(method, "textDocument/definition") == 0) {
        handle_definition(id, params);
    }
}

int main(void) {
    char header[MAX_LINE];
    char *content = malloc(MAX_CONTENT);
    if (!content) return 1;

    while (fgets(header, sizeof(header), stdin)) {
        // Parse Content-Length
        int content_length = 0;
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }

        // Read until empty line
        while (fgets(header, sizeof(header), stdin)) {
            if (header[0] == '\r' || header[0] == '\n') break;
        }

        if (content_length > 0 && content_length < MAX_CONTENT) {
            size_t read = fread(content, 1, (size_t)content_length, stdin);
            content[read] = '\0';
            process_message(content);
        }
    }

    free(content);
    return 0;
}
