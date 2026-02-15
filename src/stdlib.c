#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "stdlib.h"
#include "security.h"
#include "util.h"
#include "thread.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <fcntl.h>
#include <regex.h>

// Global storage for command line arguments
static int g_argc = 0;
static char **g_argv = NULL;

void stdlib_set_args(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

static BpStr *val_to_str(Value v, Gc *gc) {
    char buf[64];
    if (v.type == VAL_INT) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
        return gc_new_str(gc, buf, strlen(buf));
    }
    if (v.type == VAL_FLOAT) {
        // Use %g for clean output (removes trailing zeros)
        snprintf(buf, sizeof(buf), "%g", v.as.f);
        return gc_new_str(gc, buf, strlen(buf));
    }
    if (v.type == VAL_BOOL) {
        const char *s = v.as.b ? "true" : "false";
        return gc_new_str(gc, s, strlen(s));
    }
    if (v.type == VAL_NULL) {
        return gc_new_str(gc, "null", 4);
    }
    if (v.type == VAL_FLOAT) {
        snprintf(buf, sizeof(buf), "%g", v.as.f);
        return gc_new_str(gc, buf, strlen(buf));
    }
    if (v.type == VAL_STR) return v.as.s;
    return gc_new_str(gc, "<?>", 3);
}

static Value bi_print(Value *args, uint16_t argc, Gc *gc) {
    for (uint16_t i = 0; i < argc; i++) {
        BpStr *s = val_to_str(args[i], gc);
        fputs(s->data, stdout);
        if (i + 1 < argc) fputc(' ', stdout);
    }
    fputc('\n', stdout);
    return v_null();
}

static Value bi_len(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("len expects (str)");
    return v_int((int64_t)args[0].as.s->len);
}

static Value bi_substr(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_INT) bp_fatal("substr expects (str,int,int)");
    BpStr *s = args[0].as.s;
    int64_t start = args[1].as.i;
    int64_t length = args[2].as.i;
    if (start < 0) start = 0;
    if (length < 0) length = 0;
    if ((size_t)start > s->len) start = (int64_t)s->len;
    size_t max_len = s->len - (size_t)start;
    size_t n = (size_t)length;
    if (n > max_len) n = max_len;
    BpStr *out = gc_new_str(gc, s->data + start, n);
    return v_str(out);
}

static Value bi_read_line(Gc *gc) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return v_str(gc_new_str(gc, "", 0));
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) { line[n - 1] = '\0'; n--; }
    BpStr *s = gc_new_str(gc, line, (size_t)n);
    free(line);
    return v_str(s);
}

static Value bi_to_str(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1) bp_fatal("to_str expects (x)");
    return v_str(val_to_str(args[0], gc));
}

static Value bi_clock_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t ms = (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
    return v_int(ms);
}

static Value bi_exit(Value *args, uint16_t argc, int *exit_code, bool *exiting) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("exit expects (int)");
    *exit_code = (int)args[0].as.i;
    *exiting = true;
    return v_null();
}

static Value bi_file_read(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("file_read expects (str)");
    const char *path = args[0].as.s->data;
    FILE *f = fopen(path, "rb");
    if (!f) return v_str(gc_new_str(gc, "", 0));
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return v_str(gc_new_str(gc, "", 0)); }
    char *buf = bp_xmalloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    BpStr *s = gc_new_str(gc, buf, n);
    free(buf);
    return v_str(s);
}

static Value bi_file_write(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) bp_fatal("file_write expects (str,str)");
    const char *path = args[0].as.s->data;
    FILE *f = fopen(path, "wb");
    if (!f) return v_bool(false);
    size_t n = fwrite(args[1].as.s->data, 1, args[1].as.s->len, f);
    fclose(f);
    return v_bool(n == args[1].as.s->len);
}

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Value bi_base64_encode(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("base64_encode expects (str)");
    const unsigned char *data = (const unsigned char *)args[0].as.s->data;
    size_t len = args[0].as.s->len;
    
    size_t out_len = ((len + 2) / 3) * 4;
    char *out = bp_xmalloc(out_len + 1);
    size_t j = 0;
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (data[i] << 16);
        if (i + 1 < len) triple |= (data[i + 1] << 8);
        if (i + 2 < len) triple |= data[i + 2];
        
        out[j++] = base64_chars[(triple >> 18) & 0x3F];
        out[j++] = base64_chars[(triple >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? base64_chars[(triple >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? base64_chars[triple & 0x3F] : '=';
    }
    
    out[j] = '\0';
    BpStr *s = gc_new_str(gc, out, j);
    free(out);
    return v_str(s);
}

static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static Value bi_base64_decode(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("base64_decode expects (str)");
    const char *data = args[0].as.s->data;
    size_t len = args[0].as.s->len;
    
    size_t out_len = (len / 4) * 3;
    unsigned char *out = bp_xmalloc(out_len + 1);
    size_t j = 0;
    
    for (size_t i = 0; i < len; i += 4) {
        if (i + 3 >= len) break;
        
        int v1 = base64_decode_char(data[i]);
        int v2 = base64_decode_char(data[i + 1]);
        int v3 = base64_decode_char(data[i + 2]);
        int v4 = base64_decode_char(data[i + 3]);
        
        if (v1 == -1 || v2 == -1) continue;
        
        uint32_t triple = (v1 << 18) | (v2 << 12);
        if (v3 != -1) triple |= (v3 << 6);
        if (v4 != -1) triple |= v4;
        
        out[j++] = (triple >> 16) & 0xFF;
        if (data[i + 2] != '=') out[j++] = (triple >> 8) & 0xFF;
        if (data[i + 3] != '=') out[j++] = triple & 0xFF;
    }
    
    out[j] = '\0';
    BpStr *s = gc_new_str(gc, (char *)out, j);
    free(out);
    return v_str(s);
}

static Value bi_chr(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("chr expects (int)");
    int64_t code = args[0].as.i;
    if (code < 0 || code > 127) bp_fatal("chr expects 0-127");
    char buf[2] = {(char)code, '\0'};
    return v_str(gc_new_str(gc, buf, 1));
}

static Value bi_ord(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("ord expects (str)");
    if (args[0].as.s->len == 0) bp_fatal("ord expects non-empty string");
    return v_int((int64_t)(unsigned char)args[0].as.s->data[0]);
}

// Math functions
static Value bi_abs(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("abs expects (int)");
    int64_t n = args[0].as.i;
    return v_int(n < 0 ? -n : n);
}

static Value bi_min(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) 
        bp_fatal("min expects (int, int)");
    return v_int(args[0].as.i < args[1].as.i ? args[0].as.i : args[1].as.i);
}

static Value bi_max(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) 
        bp_fatal("max expects (int, int)");
    return v_int(args[0].as.i > args[1].as.i ? args[0].as.i : args[1].as.i);
}

static Value bi_pow(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT) 
        bp_fatal("pow expects (int, int)");
    double result = pow((double)args[0].as.i, (double)args[1].as.i);
    return v_int((int64_t)result);
}

static Value bi_sqrt(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("sqrt expects (int)");
    double result = sqrt((double)args[0].as.i);
    return v_int((int64_t)result);
}

static Value bi_floor(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("floor expects (int)");
    return v_int(args[0].as.i);
}

static Value bi_ceil(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("ceil expects (int)");
    return v_int(args[0].as.i);
}

static Value bi_round(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("round expects (int)");
    return v_int(args[0].as.i);
}

static Value bi_clamp(Value *args, uint16_t argc) {
    if (argc != 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT)
        bp_fatal("clamp expects (int, int, int)");
    int64_t val = args[0].as.i;
    int64_t min_val = args[1].as.i;
    int64_t max_val = args[2].as.i;
    
    if (val < min_val) return v_int(min_val);
    if (val > max_val) return v_int(max_val);
    return v_int(val);
}

static Value bi_sign(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("sign expects (int)");
    int64_t n = args[0].as.i;
    if (n > 0) return v_int(1);
    if (n < 0) return v_int(-1);
    return v_int(0);
}

// String operations
static Value bi_str_upper(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_upper expects (str)");
    BpStr *s = args[0].as.s;
    char *buf = bp_xmalloc(s->len + 1);
    for (size_t i = 0; i < s->len; i++) {
        buf[i] = toupper((unsigned char)s->data[i]);
    }
    buf[s->len] = '\0';
    BpStr *result = gc_new_str(gc, buf, s->len);
    free(buf);
    return v_str(result);
}

static Value bi_str_lower(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_lower expects (str)");
    BpStr *s = args[0].as.s;
    char *buf = bp_xmalloc(s->len + 1);
    for (size_t i = 0; i < s->len; i++) {
        buf[i] = tolower((unsigned char)s->data[i]);
    }
    buf[s->len] = '\0';
    BpStr *result = gc_new_str(gc, buf, s->len);
    free(buf);
    return v_str(result);
}

static Value bi_str_trim(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_trim expects (str)");
    BpStr *s = args[0].as.s;
    
    size_t start = 0;
    while (start < s->len && isspace((unsigned char)s->data[start])) start++;
    
    size_t end = s->len;
    while (end > start && isspace((unsigned char)s->data[end - 1])) end--;
    
    size_t new_len = end - start;
    BpStr *result = gc_new_str(gc, s->data + start, new_len);
    return v_str(result);
}

static Value bi_str_starts_with(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("starts_with expects (str, str)");
    BpStr *s = args[0].as.s;
    BpStr *prefix = args[1].as.s;
    
    if (prefix->len > s->len) return v_bool(false);
    return v_bool(memcmp(s->data, prefix->data, prefix->len) == 0);
}

static Value bi_str_ends_with(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("ends_with expects (str, str)");
    BpStr *s = args[0].as.s;
    BpStr *suffix = args[1].as.s;
    
    if (suffix->len > s->len) return v_bool(false);
    return v_bool(memcmp(s->data + s->len - suffix->len, suffix->data, suffix->len) == 0);
}

static Value bi_str_find(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_find expects (str, str)");
    BpStr *s = args[0].as.s;
    BpStr *needle = args[1].as.s;
    
    if (needle->len == 0) return v_int(0);
    if (needle->len > s->len) return v_int(-1);
    
    for (size_t i = 0; i <= s->len - needle->len; i++) {
        if (memcmp(s->data + i, needle->data, needle->len) == 0) {
            return v_int((int64_t)i);
        }
    }
    return v_int(-1);
}

static Value bi_str_replace(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_STR || args[2].type != VAL_STR)
        bp_fatal("str_replace expects (str, str, str)");
    BpStr *s = args[0].as.s;
    BpStr *old = args[1].as.s;
    BpStr *new = args[2].as.s;
    
    if (old->len == 0) return v_str(s);
    
    // Find first occurrence
    size_t pos = (size_t)-1;
    for (size_t i = 0; i <= s->len - old->len; i++) {
        if (memcmp(s->data + i, old->data, old->len) == 0) {
            pos = i;
            break;
        }
    }
    
    if (pos == (size_t)-1) return v_str(s);
    
    // Build result: before + new + after
    size_t result_len = s->len - old->len + new->len;
    char *buf = bp_xmalloc(result_len + 1);
    
    memcpy(buf, s->data, pos);
    memcpy(buf + pos, new->data, new->len);
    memcpy(buf + pos + new->len, s->data + pos + old->len, s->len - pos - old->len);
    buf[result_len] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, result_len);
    free(buf);
    return v_str(result);
}

// Random numbers
static unsigned long rand_state = 1;

static Value bi_rand(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return v_int((int64_t)((rand_state / 65536) % 32768));
}

static Value bi_rand_range(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT)
        bp_fatal("rand_range expects (int, int)");
    int64_t min = args[0].as.i;
    int64_t max = args[1].as.i;
    if (min >= max) return v_int(min);
    
    rand_state = rand_state * 1103515245 + 12345;
    int64_t range = max - min;
    int64_t val = (int64_t)((rand_state / 65536) % range);
    return v_int(min + val);
}

static Value bi_rand_seed(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("rand_seed expects (int)");
    rand_state = (unsigned long)args[0].as.i;
    return v_null();
}

// File operations
static Value bi_file_exists(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("file_exists expects (str)");
    const char *path = args[0].as.s->data;
    return v_bool(access(path, F_OK) == 0);
}

static Value bi_file_delete(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("file_delete expects (str)");
    const char *path = args[0].as.s->data;
    return v_bool(unlink(path) == 0);
}

static Value bi_file_append(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("file_append expects (str, str)");
    const char *path = args[0].as.s->data;
    FILE *f = fopen(path, "ab");
    if (!f) return v_bool(false);
    size_t n = fwrite(args[1].as.s->data, 1, args[1].as.s->len, f);
    fclose(f);
    return v_bool(n == args[1].as.s->len);
}

static Value bi_file_size(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("file_size expects (str)");
    struct stat st;
    if (stat(args[0].as.s->data, &st) != 0) return v_int(-1);
    return v_int((int64_t)st.st_size);
}

static Value bi_file_copy(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("file_copy expects (str, str)");
    
    FILE *src = fopen(args[0].as.s->data, "rb");
    if (!src) return v_bool(false);
    
    FILE *dst = fopen(args[1].as.s->data, "wb");
    if (!dst) {
        fclose(src);
        return v_bool(false);
    }
    
    char buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dst) != n) {
            fclose(src);
            fclose(dst);
            return v_bool(false);
        }
    }
    
    fclose(src);
    fclose(dst);
    return v_bool(true);
}

// System operations
static Value bi_sleep(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("sleep expects (int)");
    int64_t ms = args[0].as.i;
    if (ms > 0) {
        usleep((useconds_t)(ms * 1000));
    }
    return v_null();
}

static Value bi_getenv(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("getenv expects (str)");
    const char *name = args[0].as.s->data;
    const char *val = getenv(name);
    if (!val) return v_str(gc_new_str(gc, "", 0));
    return v_str(gc_new_str(gc, val, strlen(val)));
}

// Security: Simple SHA256 implementation (non-cryptographic quality, for demonstration)
static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };
    
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
    }
    
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25)) ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
        uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15)) ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ ((~e) & g);
        t1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static Value bi_hash_sha256(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hash_sha256 expects (str)");
    
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    const uint8_t *data = (const uint8_t *)args[0].as.s->data;
    size_t len = args[0].as.s->len;
    size_t total_len = len + 1 + 8;
    size_t padded_len = ((total_len + 63) / 64) * 64;
    
    uint8_t *padded = bp_xmalloc(padded_len);
    memset(padded, 0, padded_len);
    memcpy(padded, data, len);
    padded[len] = 0x80;
    
    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; i++) {
        padded[padded_len - 1 - i] = (bit_len >> (i * 8)) & 0xff;
    }
    
    for (size_t i = 0; i < padded_len; i += 64) {
        sha256_transform(state, padded + i);
    }
    
    char hash_str[65];
    for (int i = 0; i < 8; i++) {
        sprintf(hash_str + i * 8, "%08x", state[i]);
    }
    hash_str[64] = '\0';
    
    free(padded);
    return v_str(gc_new_str(gc, hash_str, 64));
}

// Security: Simple MD5 implementation
static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t T[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };
    
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];
    
    for (int i = 0; i < 16; i++) {
        x[i] = ((uint32_t)block[i*4]) | ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
    }
    
    #define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
    #define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
    #define H(x, y, z) ((x) ^ (y) ^ (z))
    #define I(x, y, z) ((y) ^ ((x) | (~z)))
    #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))
    
    #define FF(a, b, c, d, x, s, ac) { \
        (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define GG(a, b, c, d, x, s, ac) { \
        (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define HH(a, b, c, d, x, s, ac) { \
        (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    #define II(a, b, c, d, x, s, ac) { \
        (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    
    FF(a, b, c, d, x[ 0],  7, T[ 0]); FF(d, a, b, c, x[ 1], 12, T[ 1]);
    FF(c, d, a, b, x[ 2], 17, T[ 2]); FF(b, c, d, a, x[ 3], 22, T[ 3]);
    FF(a, b, c, d, x[ 4],  7, T[ 4]); FF(d, a, b, c, x[ 5], 12, T[ 5]);
    FF(c, d, a, b, x[ 6], 17, T[ 6]); FF(b, c, d, a, x[ 7], 22, T[ 7]);
    FF(a, b, c, d, x[ 8],  7, T[ 8]); FF(d, a, b, c, x[ 9], 12, T[ 9]);
    FF(c, d, a, b, x[10], 17, T[10]); FF(b, c, d, a, x[11], 22, T[11]);
    FF(a, b, c, d, x[12],  7, T[12]); FF(d, a, b, c, x[13], 12, T[13]);
    FF(c, d, a, b, x[14], 17, T[14]); FF(b, c, d, a, x[15], 22, T[15]);
    
    GG(a, b, c, d, x[ 1],  5, T[16]); GG(d, a, b, c, x[ 6],  9, T[17]);
    GG(c, d, a, b, x[11], 14, T[18]); GG(b, c, d, a, x[ 0], 20, T[19]);
    GG(a, b, c, d, x[ 5],  5, T[20]); GG(d, a, b, c, x[10],  9, T[21]);
    GG(c, d, a, b, x[15], 14, T[22]); GG(b, c, d, a, x[ 4], 20, T[23]);
    GG(a, b, c, d, x[ 9],  5, T[24]); GG(d, a, b, c, x[14],  9, T[25]);
    GG(c, d, a, b, x[ 3], 14, T[26]); GG(b, c, d, a, x[ 8], 20, T[27]);
    GG(a, b, c, d, x[13],  5, T[28]); GG(d, a, b, c, x[ 2],  9, T[29]);
    GG(c, d, a, b, x[ 7], 14, T[30]); GG(b, c, d, a, x[12], 20, T[31]);
    
    HH(a, b, c, d, x[ 5],  4, T[32]); HH(d, a, b, c, x[ 8], 11, T[33]);
    HH(c, d, a, b, x[11], 16, T[34]); HH(b, c, d, a, x[14], 23, T[35]);
    HH(a, b, c, d, x[ 1],  4, T[36]); HH(d, a, b, c, x[ 4], 11, T[37]);
    HH(c, d, a, b, x[ 7], 16, T[38]); HH(b, c, d, a, x[10], 23, T[39]);
    HH(a, b, c, d, x[13],  4, T[40]); HH(d, a, b, c, x[ 0], 11, T[41]);
    HH(c, d, a, b, x[ 3], 16, T[42]); HH(b, c, d, a, x[ 6], 23, T[43]);
    HH(a, b, c, d, x[ 9],  4, T[44]); HH(d, a, b, c, x[12], 11, T[45]);
    HH(c, d, a, b, x[15], 16, T[46]); HH(b, c, d, a, x[ 2], 23, T[47]);
    
    II(a, b, c, d, x[ 0],  6, T[48]); II(d, a, b, c, x[ 7], 10, T[49]);
    II(c, d, a, b, x[14], 15, T[50]); II(b, c, d, a, x[ 5], 21, T[51]);
    II(a, b, c, d, x[12],  6, T[52]); II(d, a, b, c, x[ 3], 10, T[53]);
    II(c, d, a, b, x[10], 15, T[54]); II(b, c, d, a, x[ 1], 21, T[55]);
    II(a, b, c, d, x[ 8],  6, T[56]); II(d, a, b, c, x[15], 10, T[57]);
    II(c, d, a, b, x[ 6], 15, T[58]); II(b, c, d, a, x[13], 21, T[59]);
    II(a, b, c, d, x[ 4],  6, T[60]); II(d, a, b, c, x[11], 10, T[61]);
    II(c, d, a, b, x[ 2], 15, T[62]); II(b, c, d, a, x[ 9], 21, T[63]);
    
    #undef F
    #undef G
    #undef H
    #undef I
    #undef ROTATE_LEFT
    #undef FF
    #undef GG
    #undef HH
    #undef II
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static Value bi_hash_md5(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hash_md5 expects (str)");
    
    uint32_t state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    
    const uint8_t *data = (const uint8_t *)args[0].as.s->data;
    size_t len = args[0].as.s->len;
    size_t total_len = len + 1 + 8;
    size_t padded_len = ((total_len + 63) / 64) * 64;
    
    uint8_t *padded = bp_xmalloc(padded_len);
    memset(padded, 0, padded_len);
    memcpy(padded, data, len);
    padded[len] = 0x80;
    
    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; i++) {
        padded[padded_len - 8 + i] = (bit_len >> (i * 8)) & 0xff;
    }
    
    for (size_t i = 0; i < padded_len; i += 64) {
        md5_transform(state, padded + i);
    }
    
    char hash_str[33];
    for (int i = 0; i < 4; i++) {
        sprintf(hash_str + i * 8, "%02x%02x%02x%02x",
                state[i] & 0xff, (state[i] >> 8) & 0xff,
                (state[i] >> 16) & 0xff, (state[i] >> 24) & 0xff);
    }
    hash_str[32] = '\0';
    
    free(padded);
    return v_str(gc_new_str(gc, hash_str, 32));
}

// Security: Constant-time string comparison
static Value bi_secure_compare(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("secure_compare expects (str, str)");
    
    BpStr *s1 = args[0].as.s;
    BpStr *s2 = args[1].as.s;
    
    if (s1->len != s2->len) return v_bool(false);
    
    int result = 0;
    for (size_t i = 0; i < s1->len; i++) {
        result |= s1->data[i] ^ s2->data[i];
    }
    
    return v_bool(result == 0);
}

// Security: Generate random bytes (using /dev/urandom)
static Value bi_random_bytes(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("random_bytes expects (int)");
    
    int64_t count = args[0].as.i;
    if (count < 0 || count > 1024) bp_fatal("random_bytes: count must be 0-1024");
    
    unsigned char *bytes = bp_xmalloc((size_t)count);
    
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        free(bytes);
        bp_fatal("random_bytes: cannot open /dev/urandom");
    }
    
    ssize_t n = read(fd, bytes, (size_t)count);
    close(fd);
    
    if (n != count) {
        free(bytes);
        bp_fatal("random_bytes: failed to read random data");
    }
    
    char *hex = bp_xmalloc((size_t)count * 2 + 1);
    for (int64_t i = 0; i < count; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[count * 2] = '\0';
    
    free(bytes);
    BpStr *result = gc_new_str(gc, hex, (size_t)count * 2);
    free(hex);
    return v_str(result);
}

// String utilities
static Value bi_str_reverse(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_reverse expects (str)");
    
    BpStr *s = args[0].as.s;
    char *buf = bp_xmalloc(s->len + 1);
    
    for (size_t i = 0; i < s->len; i++) {
        buf[i] = s->data[s->len - 1 - i];
    }
    buf[s->len] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, s->len);
    free(buf);
    return v_str(result);
}

static Value bi_str_repeat(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_INT)
        bp_fatal("str_repeat expects (str, int)");
    
    BpStr *s = args[0].as.s;
    int64_t count = args[1].as.i;
    
    if (count < 0) count = 0;
    if (count > 1000) bp_fatal("str_repeat: count too large (max 1000)");
    
    size_t new_len = s->len * (size_t)count;
    char *buf = bp_xmalloc(new_len + 1);
    
    for (int64_t i = 0; i < count; i++) {
        memcpy(buf + i * s->len, s->data, s->len);
    }
    buf[new_len] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, new_len);
    free(buf);
    return v_str(result);
}

static Value bi_str_pad_left(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_STR)
        bp_fatal("str_pad_left expects (str, int, str)");
    
    BpStr *s = args[0].as.s;
    int64_t width = args[1].as.i;
    BpStr *pad = args[2].as.s;
    
    if (width <= (int64_t)s->len || pad->len == 0) return v_str(s);
    
    size_t pad_count = (size_t)(width - s->len);
    char *buf = bp_xmalloc((size_t)width + 1);
    
    for (size_t i = 0; i < pad_count; i++) {
        buf[i] = pad->data[i % pad->len];
    }
    memcpy(buf + pad_count, s->data, s->len);
    buf[width] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, (size_t)width);
    free(buf);
    return v_str(result);
}

static Value bi_str_pad_right(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_STR)
        bp_fatal("str_pad_right expects (str, int, str)");
    
    BpStr *s = args[0].as.s;
    int64_t width = args[1].as.i;
    BpStr *pad = args[2].as.s;
    
    if (width <= (int64_t)s->len || pad->len == 0) return v_str(s);
    
    size_t pad_count = (size_t)(width - s->len);
    char *buf = bp_xmalloc((size_t)width + 1);
    
    memcpy(buf, s->data, s->len);
    for (size_t i = 0; i < pad_count; i++) {
        buf[s->len + i] = pad->data[i % pad->len];
    }
    buf[width] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, (size_t)width);
    free(buf);
    return v_str(result);
}

static Value bi_str_contains(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_contains expects (str, str)");
    
    BpStr *haystack = args[0].as.s;
    BpStr *needle = args[1].as.s;
    
    if (needle->len == 0) return v_bool(true);
    if (needle->len > haystack->len) return v_bool(false);
    
    for (size_t i = 0; i <= haystack->len - needle->len; i++) {
        if (memcmp(haystack->data + i, needle->data, needle->len) == 0) {
            return v_bool(true);
        }
    }
    
    return v_bool(false);
}

static Value bi_str_count(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_count expects (str, str)");
    
    BpStr *haystack = args[0].as.s;
    BpStr *needle = args[1].as.s;
    
    if (needle->len == 0) return v_int(0);
    if (needle->len > haystack->len) return v_int(0);
    
    int64_t count = 0;
    for (size_t i = 0; i <= haystack->len - needle->len; i++) {
        if (memcmp(haystack->data + i, needle->data, needle->len) == 0) {
            count++;
            i += needle->len - 1;
        }
    }
    
    return v_int(count);
}

static Value bi_int_to_hex(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("int_to_hex expects (int)");

    int64_t val = args[0].as.i;
    char buf[32];
    snprintf(buf, sizeof(buf), "%llx", (long long)val);
    return v_str(gc_new_str(gc, buf, strlen(buf)));
}

static Value bi_str_char_at(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_INT)
        bp_fatal("str_char_at expects (str, int)");
    BpStr *s = args[0].as.s;
    int64_t index = args[1].as.i;

    if (index < 0 || (size_t)index >= s->len) {
        return v_str(gc_new_str(gc, "", 0));
    }

    char buf[2] = {s->data[index], '\0'};
    return v_str(gc_new_str(gc, buf, 1));
}

static Value bi_hex_to_int(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hex_to_int expects (str)");

    BpStr *s = args[0].as.s;
    int64_t result = 0;

    for (size_t i = 0; i < s->len; i++) {
        char c = s->data[i];
        int digit = 0;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else bp_fatal("hex_to_int: invalid hex character");
        result = result * 16 + digit;
    }

    return v_int(result);
}

static Value bi_str_index_of(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_index_of expects (str, str)");
    return bi_str_find(args, argc);
}

/* Validation functions */
static Value bi_is_digit(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_digit expects (str)");

    BpStr *s = args[0].as.s;
    if (s->len == 0) return v_bool(false);

    for (size_t i = 0; i < s->len; i++) {
        if (!isdigit((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

static Value bi_is_alpha(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_alpha expects (str)");

    BpStr *s = args[0].as.s;
    if (s->len == 0) return v_bool(false);

    for (size_t i = 0; i < s->len; i++) {
        if (!isalpha((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

static Value bi_is_alnum(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_alnum expects (str)");

    BpStr *s = args[0].as.s;
    if (s->len == 0) return v_bool(false);

    for (size_t i = 0; i < s->len; i++) {
        if (!isalnum((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

static Value bi_is_space(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_space expects (str)");

    BpStr *s = args[0].as.s;
    if (s->len == 0) return v_bool(false);

    for (size_t i = 0; i < s->len; i++) {
        if (!isspace((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

/* Float math functions */
static Value bi_sin(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("sin expects (float)");
    return v_float(sin(args[0].as.f));
}

static Value bi_cos(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("cos expects (float)");
    return v_float(cos(args[0].as.f));
}

static Value bi_tan(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("tan expects (float)");
    return v_float(tan(args[0].as.f));
}

static Value bi_asin(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("asin expects (float)");
    return v_float(asin(args[0].as.f));
}

static Value bi_acos(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("acos expects (float)");
    return v_float(acos(args[0].as.f));
}

static Value bi_atan(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("atan expects (float)");
    return v_float(atan(args[0].as.f));
}

static Value bi_atan2(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_FLOAT || args[1].type != VAL_FLOAT)
        bp_fatal("atan2 expects (float, float)");
    return v_float(atan2(args[0].as.f, args[1].as.f));
}

static Value bi_log_f(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("log expects (float)");
    return v_float(log(args[0].as.f));
}

static Value bi_log10_f(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("log10 expects (float)");
    return v_float(log10(args[0].as.f));
}

static Value bi_log2_f(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("log2 expects (float)");
    return v_float(log2(args[0].as.f));
}

static Value bi_exp_f(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("exp expects (float)");
    return v_float(exp(args[0].as.f));
}

static Value bi_fabs(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("fabs expects (float)");
    return v_float(fabs(args[0].as.f));
}

static Value bi_ffloor(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("ffloor expects (float)");
    return v_float(floor(args[0].as.f));
}

static Value bi_fceil(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("fceil expects (float)");
    return v_float(ceil(args[0].as.f));
}

static Value bi_fround(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("fround expects (float)");
    return v_float(round(args[0].as.f));
}

static Value bi_fsqrt(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("fsqrt expects (float)");
    return v_float(sqrt(args[0].as.f));
}

static Value bi_fpow(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_FLOAT || args[1].type != VAL_FLOAT)
        bp_fatal("fpow expects (float, float)");
    return v_float(pow(args[0].as.f, args[1].as.f));
}

/* Float conversion functions */
static Value bi_int_to_float(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("int_to_float expects (int)");
    return v_float((double)args[0].as.i);
}

static Value bi_float_to_int(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("float_to_int expects (float)");
    return v_int((int64_t)args[0].as.f);
}

static Value bi_float_to_str(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("float_to_str expects (float)");
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", args[0].as.f);
    return v_str(gc_new_str(gc, buf, strlen(buf)));
}

static Value bi_str_to_float(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_to_float expects (str)");
    return v_float(strtod(args[0].as.s->data, NULL));
}

/* Float utilities */
static Value bi_is_nan(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("is_nan expects (float)");
    return v_bool(isnan(args[0].as.f));
}

static Value bi_is_inf(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_FLOAT) bp_fatal("is_inf expects (float)");
    return v_bool(isinf(args[0].as.f));
}

/* Array operations */
static Value bi_array_len(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) bp_fatal("array_len expects (array)");
    return v_int((int64_t)args[0].as.arr->len);
}

static Value bi_array_push(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_ARRAY) bp_fatal("array_push expects (array, value)");
    gc_array_push(gc, args[0].as.arr, args[1]);
    return v_null();
}

static Value bi_array_pop(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_ARRAY) bp_fatal("array_pop expects (array)");
    BpArray *arr = args[0].as.arr;
    if (arr->len == 0) bp_fatal("array_pop on empty array");
    Value v = arr->data[arr->len - 1];
    arr->len--;
    return v;
}

/* Map operations */
static Value bi_map_len(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_MAP) bp_fatal("map_len expects (map)");
    return v_int((int64_t)args[0].as.map->count);
}

static Value bi_map_keys(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_MAP) bp_fatal("map_keys expects (map)");
    BpArray *arr = gc_map_keys(gc, args[0].as.map);
    return v_array(arr);
}

static Value bi_map_values(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_MAP) bp_fatal("map_values expects (map)");
    BpArray *arr = gc_map_values(gc, args[0].as.map);
    return v_array(arr);
}

static Value bi_map_has_key(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_MAP) bp_fatal("map_has_key expects (map, key)");
    return v_bool(gc_map_has_key(args[0].as.map, args[1]));
}

static Value bi_map_delete(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_MAP) bp_fatal("map_delete expects (map, key)");
    return v_bool(gc_map_delete(args[0].as.map, args[1]));
}

static Value bi_argv(Gc *gc) {
    // Create an array of strings from command line arguments
    BpArray *arr = gc_new_array(gc, (size_t)g_argc);
    for (int i = 0; i < g_argc; i++) {
        const char *arg = g_argv[i];
        size_t len = strlen(arg);
        BpStr *s = gc_new_str(gc, arg, len);
        arr->data[i] = v_str(s);
    }
    arr->len = (size_t)g_argc;
    return v_array(arr);
}

static Value bi_argc(void) {
    return v_int((int64_t)g_argc);
}

// ============================================================================
// Threading Built-in Functions
// ============================================================================

static Value bi_thread_current(void) {
    return v_int((int64_t)bp_thread_current_id());
}

static Value bi_thread_yield(void) {
    bp_thread_yield();
    return v_null();
}

static Value bi_thread_sleep(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("thread_sleep expects (int)");
    bp_thread_sleep((uint64_t)args[0].as.i);
    return v_null();
}

static Value bi_mutex_new(void) {
    BpMutex *mutex = bp_mutex_new();
    if (!mutex) return v_null();
    return v_ptr(mutex);
}

static Value bi_mutex_lock(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("mutex_lock expects (mutex)");
    BpMutex *mutex = (BpMutex *)args[0].as.ptr;
    bp_mutex_lock(mutex);
    return v_null();
}

static Value bi_mutex_trylock(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("mutex_trylock expects (mutex)");
    BpMutex *mutex = (BpMutex *)args[0].as.ptr;
    return v_bool(bp_mutex_trylock(mutex));
}

static Value bi_mutex_unlock(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("mutex_unlock expects (mutex)");
    BpMutex *mutex = (BpMutex *)args[0].as.ptr;
    bp_mutex_unlock(mutex);
    return v_null();
}

static Value bi_cond_new(void) {
    BpCondition *cond = bp_cond_new();
    if (!cond) return v_null();
    return v_ptr(cond);
}

static Value bi_cond_wait(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_PTR || args[1].type != VAL_PTR)
        bp_fatal("cond_wait expects (cond, mutex)");
    BpCondition *cond = (BpCondition *)args[0].as.ptr;
    BpMutex *mutex = (BpMutex *)args[1].as.ptr;
    bp_cond_wait(cond, mutex);
    return v_null();
}

static Value bi_cond_signal(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("cond_signal expects (cond)");
    BpCondition *cond = (BpCondition *)args[0].as.ptr;
    bp_cond_signal(cond);
    return v_null();
}

static Value bi_cond_broadcast(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("cond_broadcast expects (cond)");
    BpCondition *cond = (BpCondition *)args[0].as.ptr;
    bp_cond_broadcast(cond);
    return v_null();
}

// Thread spawn and join need VM context - handled by special opcodes
// These placeholders return errors if called without proper VM integration
static Value bi_thread_spawn(Value *args, uint16_t argc) {
    BP_UNUSED(args);
    BP_UNUSED(argc);
    bp_fatal("thread_spawn requires VM context - use OP_THREAD_SPAWN opcode");
    return v_null();
}

static Value bi_thread_join(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("thread_join expects (thread)");
    BpThread *thread = (BpThread *)args[0].as.ptr;
    return bp_thread_join(thread);
}

static Value bi_thread_detach(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_PTR) bp_fatal("thread_detach expects (thread)");
    BpThread *thread = (BpThread *)args[0].as.ptr;
    return v_bool(bp_thread_detach(thread));
}

// ===== Regex operations =====

// regex_match(pattern, text) -> bool
// Returns true if pattern matches the entire text
static Value bi_regex_match(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("regex_match expects (str, str)");

    const char *pattern = args[0].as.s->data;
    const char *text = args[1].as.s->data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        return v_bool(false);  // Invalid regex pattern
    }

    ret = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);

    return v_bool(ret == 0);
}

// regex_search(pattern, text) -> int
// Returns the starting index of the first match, or -1 if no match
static Value bi_regex_search(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("regex_search expects (str, str)");

    const char *pattern = args[0].as.s->data;
    const char *text = args[1].as.s->data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        return v_int(-1);  // Invalid regex pattern
    }

    regmatch_t match;
    ret = regexec(&regex, text, 1, &match, 0);
    regfree(&regex);

    if (ret == 0) {
        return v_int(match.rm_so);  // Return start of match
    }
    return v_int(-1);  // No match
}

// regex_replace(pattern, replacement, text) -> str
// Replaces all matches of pattern with replacement
static Value bi_regex_replace(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_STR || args[2].type != VAL_STR)
        bp_fatal("regex_replace expects (str, str, str)");

    const char *pattern = args[0].as.s->data;
    const char *replacement = args[1].as.s->data;
    const char *text = args[2].as.s->data;
    size_t text_len = args[2].as.s->len;
    size_t repl_len = strlen(replacement);

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        // Invalid regex, return original text
        return v_str(gc_new_str(gc, text, text_len));
    }

    // Build result string
    size_t result_cap = text_len * 2 + 64;
    char *result = bp_xmalloc(result_cap);
    size_t result_len = 0;

    const char *cursor = text;
    regmatch_t match;

    while (regexec(&regex, cursor, 1, &match, 0) == 0) {
        // Copy text before match
        size_t before_len = (size_t)match.rm_so;
        if (result_len + before_len + repl_len + 1 > result_cap) {
            result_cap = (result_len + before_len + repl_len) * 2;
            result = bp_xrealloc(result, result_cap);
        }
        memcpy(result + result_len, cursor, before_len);
        result_len += before_len;

        // Copy replacement
        memcpy(result + result_len, replacement, repl_len);
        result_len += repl_len;

        // Move cursor past match
        cursor += match.rm_eo;
        if (match.rm_eo == 0) cursor++;  // Avoid infinite loop on empty match
    }

    // Copy remaining text
    size_t remaining = strlen(cursor);
    if (result_len + remaining + 1 > result_cap) {
        result_cap = result_len + remaining + 1;
        result = bp_xrealloc(result, result_cap);
    }
    memcpy(result + result_len, cursor, remaining);
    result_len += remaining;
    result[result_len] = '\0';

    regfree(&regex);

    BpStr *s = gc_new_str(gc, result, result_len);
    free(result);
    return v_str(s);
}

// regex_split(pattern, text) -> [str]
// Splits text by pattern, returns array of strings
static Value bi_regex_split(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("regex_split expects (str, str)");

    const char *pattern = args[0].as.s->data;
    const char *text = args[1].as.s->data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        // Invalid regex, return array with original string
        BpArray *arr = gc_new_array(gc, 1);
        gc_array_push(gc, arr, v_str(gc_new_str(gc, text, strlen(text))));
        return v_array(arr);
    }

    BpArray *arr = gc_new_array(gc, 8);
    const char *cursor = text;
    regmatch_t match;

    while (regexec(&regex, cursor, 1, &match, 0) == 0) {
        // Add substring before match
        size_t before_len = (size_t)match.rm_so;
        BpStr *part = gc_new_str(gc, cursor, before_len);
        gc_array_push(gc, arr, v_str(part));

        // Move cursor past match
        cursor += match.rm_eo;
        if (match.rm_eo == 0) cursor++;  // Avoid infinite loop on empty match
    }

    // Add remaining text
    BpStr *remaining = gc_new_str(gc, cursor, strlen(cursor));
    gc_array_push(gc, arr, v_str(remaining));

    regfree(&regex);
    return v_array(arr);
}

// regex_find_all(pattern, text) -> [str]
// Returns array of all matches
static Value bi_regex_find_all(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("regex_find_all expects (str, str)");

    const char *pattern = args[0].as.s->data;
    const char *text = args[1].as.s->data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        // Invalid regex, return empty array
        return v_array(gc_new_array(gc, 0));
    }

    BpArray *arr = gc_new_array(gc, 8);
    const char *cursor = text;
    regmatch_t match;

    while (regexec(&regex, cursor, 1, &match, 0) == 0) {
        // Add matched substring
        size_t match_len = (size_t)(match.rm_eo - match.rm_so);
        BpStr *m = gc_new_str(gc, cursor + match.rm_so, match_len);
        gc_array_push(gc, arr, v_str(m));

        // Move cursor past match
        cursor += match.rm_eo;
        if (match.rm_eo == 0) cursor++;  // Avoid infinite loop on empty match
    }

    regfree(&regex);
    return v_array(arr);
}

// ===== StringBuilder-like operations =====

// str_split_str(text, separator) -> [str]
// Splits text by a string separator, returns array of strings
static Value bi_str_split_str(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_split_str expects (str, str)");

    const char *text = args[0].as.s->data;
    const char *sep = args[1].as.s->data;
    size_t sep_len = args[1].as.s->len;

    BpArray *arr = gc_new_array(gc, 8);

    if (sep_len == 0) {
        // Empty separator - split into individual characters
        const char *p = text;
        while (*p) {
            BpStr *part = gc_new_str(gc, p, 1);
            gc_array_push(gc, arr, v_str(part));
            p++;
        }
        return v_array(arr);
    }

    const char *cursor = text;
    const char *found;

    while ((found = strstr(cursor, sep)) != NULL) {
        size_t part_len = (size_t)(found - cursor);
        BpStr *part = gc_new_str(gc, cursor, part_len);
        gc_array_push(gc, arr, v_str(part));
        cursor = found + sep_len;
    }

    // Add remaining text
    BpStr *remaining = gc_new_str(gc, cursor, strlen(cursor));
    gc_array_push(gc, arr, v_str(remaining));

    return v_array(arr);
}

// str_join_arr(arr, separator) -> str
// Joins array of strings with separator
static Value bi_str_join_arr(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STR)
        bp_fatal("str_join_arr expects (array, str)");

    BpArray *arr = args[0].as.arr;
    const char *sep = args[1].as.s->data;
    size_t sep_len = args[1].as.s->len;

    if (arr->len == 0) {
        return v_str(gc_new_str(gc, "", 0));
    }

    // Calculate total length
    size_t total_len = 0;
    for (size_t i = 0; i < arr->len; i++) {
        if (arr->data[i].type != VAL_STR) bp_fatal("str_join_arr: array elements must be strings");
        total_len += arr->data[i].as.s->len;
    }
    total_len += (arr->len - 1) * sep_len;

    // Allocate and build result
    char *result = bp_xmalloc(total_len + 1);
    size_t pos = 0;

    for (size_t i = 0; i < arr->len; i++) {
        BpStr *s = arr->data[i].as.s;
        memcpy(result + pos, s->data, s->len);
        pos += s->len;

        if (i + 1 < arr->len) {
            memcpy(result + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    result[pos] = '\0';

    BpStr *str = gc_new_str(gc, result, total_len);
    free(result);
    return v_str(str);
}

// str_concat_all(arr) -> str
// Concatenates all strings in array (like join with empty separator)
static Value bi_str_concat_all(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_ARRAY)
        bp_fatal("str_concat_all expects (array)");

    BpArray *arr = args[0].as.arr;

    if (arr->len == 0) {
        return v_str(gc_new_str(gc, "", 0));
    }

    // Calculate total length
    size_t total_len = 0;
    for (size_t i = 0; i < arr->len; i++) {
        if (arr->data[i].type != VAL_STR) bp_fatal("str_concat_all: array elements must be strings");
        total_len += arr->data[i].as.s->len;
    }

    // Allocate and build result
    char *result = bp_xmalloc(total_len + 1);
    size_t pos = 0;

    for (size_t i = 0; i < arr->len; i++) {
        BpStr *s = arr->data[i].as.s;
        memcpy(result + pos, s->data, s->len);
        pos += s->len;
    }
    result[pos] = '\0';

    BpStr *str = gc_new_str(gc, result, total_len);
    free(result);
    return v_str(str);
}

// ============================================================================
// JSON Parser (recursive descent)
// ============================================================================

static Value json_parse_value(const char *src, size_t slen, size_t *pos, Gc *gc);

static void jp_skip_ws(const char *src, size_t slen, size_t *pos) {
    while (*pos < slen && (src[*pos]==' '||src[*pos]=='\t'||src[*pos]=='\n'||src[*pos]=='\r'))
        (*pos)++;
}

static Value json_parse_string(const char *src, size_t slen, size_t *pos, Gc *gc) {
    if (*pos >= slen || src[*pos] != '"') bp_fatal("json_parse: expected '\"'");
    (*pos)++;  // skip opening quote
    // Build string with escape handling
    char *buf = bp_xmalloc(slen);
    size_t blen = 0;
    while (*pos < slen && src[*pos] != '"') {
        if (src[*pos] == '\\' && *pos + 1 < slen) {
            (*pos)++;
            switch (src[*pos]) {
                case '"': buf[blen++] = '"'; break;
                case '\\': buf[blen++] = '\\'; break;
                case '/': buf[blen++] = '/'; break;
                case 'b': buf[blen++] = '\b'; break;
                case 'f': buf[blen++] = '\f'; break;
                case 'n': buf[blen++] = '\n'; break;
                case 'r': buf[blen++] = '\r'; break;
                case 't': buf[blen++] = '\t'; break;
                case 'u': {
                    // Parse 4 hex digits (basic support)
                    if (*pos + 4 < slen) {
                        unsigned int cp = 0;
                        for (int i = 0; i < 4; i++) {
                            (*pos)++;
                            char c = src[*pos];
                            cp <<= 4;
                            if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                            else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
                        }
                        // Simple ASCII range support
                        if (cp < 128) buf[blen++] = (char)cp;
                        else { buf[blen++] = '?'; }  // Non-ASCII placeholder
                    }
                    break;
                }
                default: buf[blen++] = src[*pos]; break;
            }
        } else {
            buf[blen++] = src[*pos];
        }
        (*pos)++;
    }
    if (*pos < slen) (*pos)++;  // skip closing quote
    buf[blen] = '\0';
    BpStr *s = gc_new_str(gc, buf, blen);
    free(buf);
    return v_str(s);
}

static Value json_parse_number(const char *src, size_t slen, size_t *pos, Gc *gc) {
    (void)gc;
    size_t start = *pos;
    bool is_float = false;
    if (*pos < slen && src[*pos] == '-') (*pos)++;
    while (*pos < slen && src[*pos] >= '0' && src[*pos] <= '9') (*pos)++;
    if (*pos < slen && src[*pos] == '.') { is_float = true; (*pos)++; }
    while (*pos < slen && src[*pos] >= '0' && src[*pos] <= '9') (*pos)++;
    if (*pos < slen && (src[*pos] == 'e' || src[*pos] == 'E')) {
        is_float = true;
        (*pos)++;
        if (*pos < slen && (src[*pos] == '+' || src[*pos] == '-')) (*pos)++;
        while (*pos < slen && src[*pos] >= '0' && src[*pos] <= '9') (*pos)++;
    }
    char tmp[64];
    size_t nlen = *pos - start;
    if (nlen >= sizeof(tmp)) nlen = sizeof(tmp) - 1;
    memcpy(tmp, src + start, nlen);
    tmp[nlen] = '\0';
    if (is_float) return v_float(strtod(tmp, NULL));
    return v_int(strtoll(tmp, NULL, 10));
}

static Value json_parse_array(const char *src, size_t slen, size_t *pos, Gc *gc) {
    (*pos)++;  // skip '['
    jp_skip_ws(src, slen, pos);
    BpArray *arr = gc_new_array(gc, 8);
    if (*pos < slen && src[*pos] == ']') { (*pos)++; return v_array(arr); }
    while (*pos < slen) {
        Value elem = json_parse_value(src, slen, pos, gc);
        gc_array_push(gc, arr, elem);
        jp_skip_ws(src, slen, pos);
        if (*pos < slen && src[*pos] == ',') { (*pos)++; jp_skip_ws(src, slen, pos); continue; }
        break;
    }
    if (*pos < slen && src[*pos] == ']') (*pos)++;
    return v_array(arr);
}

static Value json_parse_object(const char *src, size_t slen, size_t *pos, Gc *gc) {
    (*pos)++;  // skip '{'
    jp_skip_ws(src, slen, pos);
    BpMap *map = gc_new_map(gc, 16);
    if (*pos < slen && src[*pos] == '}') { (*pos)++; return v_map(map); }
    while (*pos < slen) {
        jp_skip_ws(src, slen, pos);
        Value key = json_parse_string(src, slen, pos, gc);
        jp_skip_ws(src, slen, pos);
        if (*pos < slen && src[*pos] == ':') (*pos)++;
        jp_skip_ws(src, slen, pos);
        Value val = json_parse_value(src, slen, pos, gc);
        gc_map_set(gc, map, key, val);
        jp_skip_ws(src, slen, pos);
        if (*pos < slen && src[*pos] == ',') { (*pos)++; continue; }
        break;
    }
    if (*pos < slen && src[*pos] == '}') (*pos)++;
    return v_map(map);
}

static Value json_parse_value(const char *src, size_t slen, size_t *pos, Gc *gc) {
    jp_skip_ws(src, slen, pos);
    if (*pos >= slen) return v_null();
    char c = src[*pos];
    if (c == '"') return json_parse_string(src, slen, pos, gc);
    if (c == '[') return json_parse_array(src, slen, pos, gc);
    if (c == '{') return json_parse_object(src, slen, pos, gc);
    if (c == 't' && *pos + 3 < slen && memcmp(src + *pos, "true", 4) == 0) { *pos += 4; return v_bool(true); }
    if (c == 'f' && *pos + 4 < slen && memcmp(src + *pos, "false", 5) == 0) { *pos += 5; return v_bool(false); }
    if (c == 'n' && *pos + 3 < slen && memcmp(src + *pos, "null", 4) == 0) { *pos += 4; return v_null(); }
    if (c == '-' || (c >= '0' && c <= '9')) return json_parse_number(src, slen, pos, gc);
    bp_fatal("json_parse: unexpected character '%c' at position %zu", c, *pos);
    return v_null();
}

Value stdlib_call(BuiltinId id, Value *args, uint16_t argc, Gc *gc, int *exit_code, bool *exiting) {
    switch (id) {
        case BI_PRINT: return bi_print(args, argc, gc);
        case BI_LEN: return bi_len(args, argc);
        case BI_SUBSTR: return bi_substr(args, argc, gc);
        case BI_READ_LINE: return bi_read_line(gc);
        case BI_TO_STR: return bi_to_str(args, argc, gc);
        case BI_CLOCK_MS: return bi_clock_ms();
        case BI_EXIT: return bi_exit(args, argc, exit_code, exiting);
        case BI_FILE_READ: return bi_file_read(args, argc, gc);
        case BI_FILE_WRITE: return bi_file_write(args, argc);
        case BI_BASE64_ENCODE: return bi_base64_encode(args, argc, gc);
        case BI_BASE64_DECODE: return bi_base64_decode(args, argc, gc);
        case BI_CHR: return bi_chr(args, argc, gc);
        case BI_ORD: return bi_ord(args, argc);
        
        // Math
        case BI_ABS: return bi_abs(args, argc);
        case BI_MIN: return bi_min(args, argc);
        case BI_MAX: return bi_max(args, argc);
        case BI_POW: return bi_pow(args, argc);
        case BI_SQRT: return bi_sqrt(args, argc);
        case BI_FLOOR: return bi_floor(args, argc);
        case BI_CEIL: return bi_ceil(args, argc);
        case BI_ROUND: return bi_round(args, argc);
        
        // Strings
        case BI_STR_UPPER: return bi_str_upper(args, argc, gc);
        case BI_STR_LOWER: return bi_str_lower(args, argc, gc);
        case BI_STR_TRIM: return bi_str_trim(args, argc, gc);
        case BI_STR_STARTS_WITH: return bi_str_starts_with(args, argc);
        case BI_STR_ENDS_WITH: return bi_str_ends_with(args, argc);
        case BI_STR_FIND: return bi_str_find(args, argc);
        case BI_STR_REPLACE: return bi_str_replace(args, argc, gc);
        
        // Random
        case BI_RAND: return bi_rand();
        case BI_RAND_RANGE: return bi_rand_range(args, argc);
        case BI_RAND_SEED: return bi_rand_seed(args, argc);
        
        // File ops
        case BI_FILE_EXISTS: return bi_file_exists(args, argc);
        case BI_FILE_DELETE: return bi_file_delete(args, argc);
        case BI_FILE_APPEND: return bi_file_append(args, argc);
        
        // System
        case BI_SLEEP: return bi_sleep(args, argc);
        case BI_GETENV: return bi_getenv(args, argc, gc);
        
        // Security
        case BI_HASH_SHA256: return bi_hash_sha256(args, argc, gc);
        case BI_HASH_MD5: return bi_hash_md5(args, argc, gc);
        case BI_SECURE_COMPARE: return bi_secure_compare(args, argc);
        case BI_RANDOM_BYTES: return bi_random_bytes(args, argc, gc);
        
        // String utilities
        case BI_STR_REVERSE: return bi_str_reverse(args, argc, gc);
        case BI_STR_REPEAT: return bi_str_repeat(args, argc, gc);
        case BI_STR_PAD_LEFT: return bi_str_pad_left(args, argc, gc);
        case BI_STR_PAD_RIGHT: return bi_str_pad_right(args, argc, gc);
        case BI_STR_CONTAINS: return bi_str_contains(args, argc);
        case BI_STR_COUNT: return bi_str_count(args, argc);
        case BI_STR_CHAR_AT: return bi_str_char_at(args, argc, gc);
        case BI_STR_INDEX_OF: return bi_str_index_of(args, argc);
        case BI_INT_TO_HEX: return bi_int_to_hex(args, argc, gc);
        case BI_HEX_TO_INT: return bi_hex_to_int(args, argc);
        
        // Validation
        case BI_IS_DIGIT: return bi_is_digit(args, argc);
        case BI_IS_ALPHA: return bi_is_alpha(args, argc);
        case BI_IS_ALNUM: return bi_is_alnum(args, argc);
        case BI_IS_SPACE: return bi_is_space(args, argc);
        
        // Math extensions
        case BI_CLAMP: return bi_clamp(args, argc);
        case BI_SIGN: return bi_sign(args, argc);
        
        // File utilities
        case BI_FILE_SIZE: return bi_file_size(args, argc);
        case BI_FILE_COPY: return bi_file_copy(args, argc);

        // Float math functions
        case BI_SIN: return bi_sin(args, argc);
        case BI_COS: return bi_cos(args, argc);
        case BI_TAN: return bi_tan(args, argc);
        case BI_ASIN: return bi_asin(args, argc);
        case BI_ACOS: return bi_acos(args, argc);
        case BI_ATAN: return bi_atan(args, argc);
        case BI_ATAN2: return bi_atan2(args, argc);
        case BI_LOG: return bi_log_f(args, argc);
        case BI_LOG10: return bi_log10_f(args, argc);
        case BI_LOG2: return bi_log2_f(args, argc);
        case BI_EXP: return bi_exp_f(args, argc);
        case BI_FABS: return bi_fabs(args, argc);
        case BI_FFLOOR: return bi_ffloor(args, argc);
        case BI_FCEIL: return bi_fceil(args, argc);
        case BI_FROUND: return bi_fround(args, argc);
        case BI_FSQRT: return bi_fsqrt(args, argc);
        case BI_FPOW: return bi_fpow(args, argc);

        // Float conversion functions
        case BI_INT_TO_FLOAT: return bi_int_to_float(args, argc);
        case BI_FLOAT_TO_INT: return bi_float_to_int(args, argc);
        case BI_FLOAT_TO_STR: return bi_float_to_str(args, argc, gc);
        case BI_STR_TO_FLOAT: return bi_str_to_float(args, argc);

        // Float utilities
        case BI_IS_NAN: return bi_is_nan(args, argc);
        case BI_IS_INF: return bi_is_inf(args, argc);

        // Array operations
        case BI_ARRAY_LEN: return bi_array_len(args, argc);
        case BI_ARRAY_PUSH: return bi_array_push(args, argc, gc);
        case BI_ARRAY_POP: return bi_array_pop(args, argc);

        // Map operations
        case BI_MAP_LEN: return bi_map_len(args, argc);
        case BI_MAP_KEYS: return bi_map_keys(args, argc, gc);
        case BI_MAP_VALUES: return bi_map_values(args, argc, gc);
        case BI_MAP_HAS_KEY: return bi_map_has_key(args, argc);
        case BI_MAP_DELETE: return bi_map_delete(args, argc);

        // System/argv
        case BI_ARGV: return bi_argv(gc);
        case BI_ARGC: return bi_argc();

        // Threading operations
        case BI_THREAD_SPAWN: return bi_thread_spawn(args, argc);
        case BI_THREAD_JOIN: return bi_thread_join(args, argc);
        case BI_THREAD_DETACH: return bi_thread_detach(args, argc);
        case BI_THREAD_CURRENT: return bi_thread_current();
        case BI_THREAD_YIELD: return bi_thread_yield();
        case BI_THREAD_SLEEP: return bi_thread_sleep(args, argc);
        case BI_MUTEX_NEW: return bi_mutex_new();
        case BI_MUTEX_LOCK: return bi_mutex_lock(args, argc);
        case BI_MUTEX_TRYLOCK: return bi_mutex_trylock(args, argc);
        case BI_MUTEX_UNLOCK: return bi_mutex_unlock(args, argc);
        case BI_COND_NEW: return bi_cond_new();
        case BI_COND_WAIT: return bi_cond_wait(args, argc);
        case BI_COND_SIGNAL: return bi_cond_signal(args, argc);
        case BI_COND_BROADCAST: return bi_cond_broadcast(args, argc);

        // Regex operations
        case BI_REGEX_MATCH: return bi_regex_match(args, argc);
        case BI_REGEX_SEARCH: return bi_regex_search(args, argc);
        case BI_REGEX_REPLACE: return bi_regex_replace(args, argc, gc);
        case BI_REGEX_SPLIT: return bi_regex_split(args, argc, gc);
        case BI_REGEX_FIND_ALL: return bi_regex_find_all(args, argc, gc);

        // StringBuilder-like operations
        case BI_STR_SPLIT_STR: return bi_str_split_str(args, argc, gc);
        case BI_STR_JOIN_ARR: return bi_str_join_arr(args, argc, gc);
        case BI_STR_CONCAT_ALL: return bi_str_concat_all(args, argc, gc);

        // Bitwise operations
        case BI_BIT_AND: return v_int(args[0].as.i & args[1].as.i);
        case BI_BIT_OR:  return v_int(args[0].as.i | args[1].as.i);
        case BI_BIT_XOR: return v_int(args[0].as.i ^ args[1].as.i);
        case BI_BIT_NOT: return v_int(~args[0].as.i);
        case BI_BIT_SHL: return v_int(args[0].as.i << args[1].as.i);
        case BI_BIT_SHR: return v_int(args[0].as.i >> args[1].as.i);

        // Byte conversion
        case BI_BYTES_TO_STR: {
            if (argc != 1 || args[0].type != VAL_STR) bp_fatal("bytes_to_str expects (str)");
            return args[0]; // identity - strings are already byte arrays in this impl
        }
        case BI_STR_TO_BYTES: {
            if (argc != 1 || args[0].type != VAL_STR) bp_fatal("str_to_bytes expects (str)");
            BpStr *s = args[0].as.s;
            BpArray *arr = gc_new_array(gc, s->len);
            for (size_t i = 0; i < s->len; i++) {
                gc_array_push(gc, arr, v_int((int64_t)(unsigned char)s->data[i]));
            }
            return v_array(arr);
        }

        case BI_PARSE_INT: {
            if (argc != 1 || args[0].type != VAL_STR) bp_fatal("parse_int expects (str)");
            char *end = NULL;
            long long val = strtoll(args[0].as.s->data, &end, 10);
            if (end == args[0].as.s->data) return v_int(0); // parse failure
            return v_int((int64_t)val);
        }
        case BI_JSON_STRINGIFY: {
            if (argc != 1) bp_fatal("json_stringify expects 1 arg");
            Value v = args[0];
            if (v.type == VAL_NULL) return v_str(gc_new_str(gc, "null", 4));
            if (v.type == VAL_BOOL) {
                const char *s = v.as.b ? "true" : "false";
                return v_str(gc_new_str(gc, s, strlen(s)));
            }
            if (v.type == VAL_INT) {
                char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
                return v_str(gc_new_str(gc, buf, strlen(buf)));
            }
            if (v.type == VAL_FLOAT) {
                char buf[64]; snprintf(buf, sizeof(buf), "%g", v.as.f);
                return v_str(gc_new_str(gc, buf, strlen(buf)));
            }
            if (v.type == VAL_STR) {
                // Wrap string in quotes with escaping
                BpStr *src = v.as.s;
                size_t cap = src->len * 2 + 3;
                char *buf = bp_xmalloc(cap);
                size_t o = 0;
                buf[o++] = '"';
                for (size_t i = 0; i < src->len; i++) {
                    char c = src->data[i];
                    if (c == '"' || c == '\\') { buf[o++] = '\\'; buf[o++] = c; }
                    else if (c == '\n') { buf[o++] = '\\'; buf[o++] = 'n'; }
                    else if (c == '\t') { buf[o++] = '\\'; buf[o++] = 't'; }
                    else if (c == '\r') { buf[o++] = '\\'; buf[o++] = 'r'; }
                    else buf[o++] = c;
                }
                buf[o++] = '"'; buf[o] = '\0';
                BpStr *res = gc_new_str(gc, buf, o);
                free(buf);
                return v_str(res);
            }
            if (v.type == VAL_ARRAY) {
                // Recursive JSON array serialization
                BpArray *arr = v.as.arr;
                size_t cap = 256;
                char *buf = bp_xmalloc(cap);
                size_t o = 0;
                buf[o++] = '[';
                for (size_t i = 0; i < arr->len; i++) {
                    if (i > 0) { buf[o++] = ','; }
                    // Recursively stringify each element
                    Value elem_args[1] = { gc_array_get(arr, i) };
                    Value elem_json = stdlib_call(BI_JSON_STRINGIFY, elem_args, 1, gc, exit_code, exiting);
                    BpStr *es = elem_json.as.s;
                    while (o + es->len + 2 > cap) { cap *= 2; buf = bp_xrealloc(buf, cap); }
                    memcpy(buf + o, es->data, es->len);
                    o += es->len;
                }
                buf[o++] = ']'; buf[o] = '\0';
                BpStr *res = gc_new_str(gc, buf, o);
                free(buf);
                return v_str(res);
            }
            if (v.type == VAL_MAP) {
                // Recursive JSON object serialization
                BpMap *map = v.as.map;
                size_t cap = 256;
                char *buf = bp_xmalloc(cap);
                size_t o = 0;
                buf[o++] = '{';
                bool first = true;
                for (size_t i = 0; i < map->cap; i++) {
                    if (map->entries[i].used != 1) continue;  // skip empty/tombstone
                    if (!first) { buf[o++] = ','; }
                    first = false;
                    // Stringify key
                    Value key_args[1] = { map->entries[i].key };
                    Value key_json = stdlib_call(BI_JSON_STRINGIFY, key_args, 1, gc, exit_code, exiting);
                    BpStr *ks = key_json.as.s;
                    while (o + ks->len + 2 > cap) { cap *= 2; buf = bp_xrealloc(buf, cap); }
                    memcpy(buf + o, ks->data, ks->len);
                    o += ks->len;
                    buf[o++] = ':';
                    // Stringify value
                    Value val_args[1] = { map->entries[i].value };
                    Value val_json = stdlib_call(BI_JSON_STRINGIFY, val_args, 1, gc, exit_code, exiting);
                    BpStr *vs = val_json.as.s;
                    while (o + vs->len + 2 > cap) { cap *= 2; buf = bp_xrealloc(buf, cap); }
                    memcpy(buf + o, vs->data, vs->len);
                    o += vs->len;
                }
                buf[o++] = '}'; buf[o] = '\0';
                BpStr *res = gc_new_str(gc, buf, o);
                free(buf);
                return v_str(res);
            }
            // Fallback for unsupported types
            BpStr *s = val_to_str(v, gc);
            return v_str(s);
        }
        case BI_BYTES_NEW: {
            if (argc != 1 || args[0].type != VAL_INT) bp_fatal("bytes_new expects (int)");
            int64_t n = args[0].as.i;
            if (n < 0) n = 0;
            BpArray *arr = gc_new_array(gc, (size_t)n);
            for (int64_t i = 0; i < n; i++) {
                gc_array_push(gc, arr, v_int(0));
            }
            return v_array(arr);
        }
        case BI_BYTES_GET: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_get expects (array, int)");
            int64_t idx = args[1].as.i;
            BpArray *arr = args[0].as.arr;
            if (idx < 0 || (size_t)idx >= arr->len) bp_fatal("bytes_get: index out of bounds");
            return v_int(arr->data[idx].as.i & 0xFF);
        }
        case BI_BYTES_SET: {
            if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT || args[2].type != VAL_INT)
                bp_fatal("bytes_set expects (array, int, int)");
            int64_t idx = args[1].as.i;
            BpArray *arr = args[0].as.arr;
            if (idx < 0 || (size_t)idx >= arr->len) bp_fatal("bytes_set: index out of bounds");
            arr->data[idx] = v_int(args[2].as.i & 0xFF);
            return v_null();
        }

        case BI_ARRAY_SORT: {
            if (argc != 1 || args[0].type != VAL_ARRAY) bp_fatal("array_sort expects (array)");
            BpArray *arr = args[0].as.arr;
            // Quicksort in-place (compare by integer value, then float, then string)
            if (arr->len <= 1) return v_null();
            // Simple insertion sort for small arrays, qsort for larger
            for (size_t i = 1; i < arr->len; i++) {
                Value key = arr->data[i];
                size_t j = i;
                while (j > 0) {
                    Value *a = &arr->data[j - 1];
                    bool swap = false;
                    if (a->type == VAL_INT && key.type == VAL_INT) swap = a->as.i > key.as.i;
                    else if (a->type == VAL_FLOAT && key.type == VAL_FLOAT) swap = a->as.f > key.as.f;
                    else if (a->type == VAL_STR && key.type == VAL_STR) swap = strcmp(a->as.s->data, key.as.s->data) > 0;
                    else swap = false;
                    if (!swap) break;
                    arr->data[j] = arr->data[j - 1];
                    j--;
                }
                arr->data[j] = key;
            }
            return v_null();
        }
        case BI_ARRAY_SLICE: {
            if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT || args[2].type != VAL_INT)
                bp_fatal("array_slice expects (array, int, int)");
            BpArray *src = args[0].as.arr;
            int64_t start = args[1].as.i;
            int64_t end = args[2].as.i;
            if (start < 0) start = 0;
            if (end > (int64_t)src->len) end = (int64_t)src->len;
            if (start >= end) {
                BpArray *empty = gc_new_array(gc, 1);
                return v_array(empty);
            }
            size_t n = (size_t)(end - start);
            BpArray *out = gc_new_array(gc, n);
            for (size_t i = 0; i < n; i++) {
                gc_array_push(gc, out, src->data[start + (int64_t)i]);
            }
            return v_array(out);
        }
        case BI_INT_TO_BYTES: {
            if (argc != 2 || args[0].type != VAL_INT || args[1].type != VAL_INT)
                bp_fatal("int_to_bytes expects (int, int)");
            int64_t value = args[0].as.i;
            int64_t size = args[1].as.i;
            if (size < 1 || size > 8) bp_fatal("int_to_bytes: size must be 1-8");
            BpArray *arr = gc_new_array(gc, (size_t)size);
            // Big-endian: most significant byte first
            for (int64_t i = size - 1; i >= 0; i--) {
                gc_array_push(gc, arr, v_int(value & 0xFF));
                value >>= 8;
            }
            // Reverse to get big-endian order (we pushed LSB first)
            for (size_t i = 0; i < (size_t)size / 2; i++) {
                Value tmp = arr->data[i];
                arr->data[i] = arr->data[size - 1 - (int64_t)i];
                arr->data[size - 1 - (int64_t)i] = tmp;
            }
            return v_array(arr);
        }
        case BI_INT_FROM_BYTES: {
            if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT || args[2].type != VAL_INT)
                bp_fatal("int_from_bytes expects (array, int, int)");
            BpArray *arr = args[0].as.arr;
            int64_t offset = args[1].as.i;
            int64_t size = args[2].as.i;
            if (size < 1 || size > 8) bp_fatal("int_from_bytes: size must be 1-8");
            if (offset < 0 || offset + size > (int64_t)arr->len)
                bp_fatal("int_from_bytes: out of bounds");
            // Big-endian: read most significant byte first
            int64_t result = 0;
            for (int64_t i = 0; i < size; i++) {
                result = (result << 8) | (arr->data[offset + i].as.i & 0xFF);
            }
            return v_int(result);
        }

        // Byte buffer operations
        case BI_BYTES_APPEND: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_append expects (array, int)");
            gc_array_push(gc, args[0].as.arr, v_int(args[1].as.i & 0xFF));
            return v_null();
        }
        case BI_BYTES_LEN: {
            if (argc != 1 || args[0].type != VAL_ARRAY)
                bp_fatal("bytes_len expects (array)");
            return v_int((int64_t)args[0].as.arr->len);
        }
        case BI_BYTES_WRITE_U16: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_write_u16 expects (array, int)");
            uint16_t val = (uint16_t)args[1].as.i;
            gc_array_push(gc, args[0].as.arr, v_int(val & 0xFF));
            gc_array_push(gc, args[0].as.arr, v_int((val >> 8) & 0xFF));
            return v_null();
        }
        case BI_BYTES_WRITE_U32: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_write_u32 expects (array, int)");
            uint32_t val = (uint32_t)args[1].as.i;
            gc_array_push(gc, args[0].as.arr, v_int(val & 0xFF));
            gc_array_push(gc, args[0].as.arr, v_int((val >> 8) & 0xFF));
            gc_array_push(gc, args[0].as.arr, v_int((val >> 16) & 0xFF));
            gc_array_push(gc, args[0].as.arr, v_int((val >> 24) & 0xFF));
            return v_null();
        }
        case BI_BYTES_WRITE_I64: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_write_i64 expects (array, int)");
            int64_t val = args[1].as.i;
            for (int b = 0; b < 8; b++) {
                gc_array_push(gc, args[0].as.arr, v_int((val >> (b * 8)) & 0xFF));
            }
            return v_null();
        }
        case BI_BYTES_READ_U16: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_read_u16 expects (array, int)");
            BpArray *arr = args[0].as.arr;
            int64_t off = args[1].as.i;
            if (off < 0 || (size_t)(off + 2) > arr->len) bp_fatal("bytes_read_u16: out of bounds");
            uint16_t val = (uint16_t)(arr->data[off].as.i & 0xFF) |
                           ((uint16_t)(arr->data[off+1].as.i & 0xFF) << 8);
            return v_int((int64_t)val);
        }
        case BI_BYTES_READ_U32: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_read_u32 expects (array, int)");
            BpArray *arr = args[0].as.arr;
            int64_t off = args[1].as.i;
            if (off < 0 || (size_t)(off + 4) > arr->len) bp_fatal("bytes_read_u32: out of bounds");
            uint32_t val = (uint32_t)(arr->data[off].as.i & 0xFF) |
                           ((uint32_t)(arr->data[off+1].as.i & 0xFF) << 8) |
                           ((uint32_t)(arr->data[off+2].as.i & 0xFF) << 16) |
                           ((uint32_t)(arr->data[off+3].as.i & 0xFF) << 24);
            return v_int((int64_t)val);
        }
        case BI_BYTES_READ_I64: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("bytes_read_i64 expects (array, int)");
            BpArray *arr = args[0].as.arr;
            int64_t off = args[1].as.i;
            if (off < 0 || (size_t)(off + 8) > arr->len) bp_fatal("bytes_read_i64: out of bounds");
            int64_t val = 0;
            for (int b = 0; b < 8; b++) {
                val |= ((int64_t)(arr->data[off + b].as.i & 0xFF)) << (b * 8);
            }
            return v_int(val);
        }

        // Binary file I/O
        case BI_FILE_READ_BYTES: {
            if (argc != 1 || args[0].type != VAL_STR)
                bp_fatal("file_read_bytes expects (str)");
            FILE *fp = fopen(args[0].as.s->data, "rb");
            if (!fp) return v_null();
            fseek(fp, 0, SEEK_END);
            long sz = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            BpArray *arr = gc_new_array(gc, sz > 0 ? (size_t)sz : 4);
            if (sz > 0) {
                unsigned char *buf = malloc((size_t)sz);
                size_t nread = fread(buf, 1, (size_t)sz, fp);
                for (size_t i = 0; i < nread; i++) {
                    gc_array_push(gc, arr, v_int((int64_t)buf[i]));
                }
                free(buf);
            }
            fclose(fp);
            return v_array(arr);
        }
        case BI_FILE_WRITE_BYTES: {
            if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_ARRAY)
                bp_fatal("file_write_bytes expects (str, array)");
            FILE *fp = fopen(args[0].as.s->data, "wb");
            if (!fp) return v_bool(false);
            BpArray *arr = args[1].as.arr;
            for (size_t i = 0; i < arr->len; i++) {
                unsigned char byte = (unsigned char)(arr->data[i].as.i & 0xFF);
                fwrite(&byte, 1, 1, fp);
            }
            fclose(fp);
            return v_bool(true);
        }

        // Array insert
        case BI_ARRAY_INSERT: {
            if (argc != 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("array_insert expects (array, int, value)");
            BpArray *arr = args[0].as.arr;
            int64_t idx = args[1].as.i;
            if (idx < 0) idx = 0;
            if ((size_t)idx > arr->len) idx = (int64_t)arr->len;
            // Grow array
            gc_array_push(gc, arr, v_null()); // extend by 1
            // Shift elements right
            for (size_t i = arr->len - 1; i > (size_t)idx; i--) {
                arr->data[i] = arr->data[i - 1];
            }
            arr->data[idx] = args[2];
            return v_null();
        }
        // Array remove
        case BI_ARRAY_REMOVE: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
                bp_fatal("array_remove expects (array, int)");
            BpArray *arr = args[0].as.arr;
            int64_t idx = args[1].as.i;
            if (idx < 0 || (size_t)idx >= arr->len) bp_fatal("array_remove: index out of bounds");
            Value removed = arr->data[idx];
            // Shift elements left
            for (size_t i = (size_t)idx; i < arr->len - 1; i++) {
                arr->data[i] = arr->data[i + 1];
            }
            arr->len--;
            return removed;
        }

        // Type introspection
        case BI_TYPEOF: {
            if (argc != 1) bp_fatal("typeof expects 1 arg");
            const char *tname = "unknown";
            switch (args[0].type) {
                case VAL_INT:    tname = "int"; break;
                case VAL_FLOAT:  tname = "float"; break;
                case VAL_BOOL:   tname = "bool"; break;
                case VAL_NULL:   tname = "null"; break;
                case VAL_STR:    tname = "str"; break;
                case VAL_ARRAY:  tname = "array"; break;
                case VAL_MAP:    tname = "map"; break;
                case VAL_STRUCT: tname = "struct"; break;
                case VAL_CLASS:  tname = "class"; break;
                case VAL_PTR:    tname = "ptr"; break;
                case VAL_FUNC:   tname = "func"; break;
            }
            return v_str(gc_new_str(gc, tname, strlen(tname)));
        }

        case BI_ARRAY_CONCAT: {
            if (argc != 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_ARRAY)
                bp_fatal("array_concat expects (array, array)");
            BpArray *a = args[0].as.arr, *b = args[1].as.arr;
            BpArray *out = gc_new_array(gc, a->len + b->len);
            for (size_t i = 0; i < a->len; i++) gc_array_push(gc, out, a->data[i]);
            for (size_t i = 0; i < b->len; i++) gc_array_push(gc, out, b->data[i]);
            return v_array(out);
        }
        case BI_ARRAY_COPY: {
            if (argc != 1 || args[0].type != VAL_ARRAY)
                bp_fatal("array_copy expects (array)");
            BpArray *a = args[0].as.arr;
            BpArray *out = gc_new_array(gc, a->len);
            for (size_t i = 0; i < a->len; i++) gc_array_push(gc, out, a->data[i]);
            return v_array(out);
        }
        case BI_ARRAY_CLEAR: {
            if (argc != 1 || args[0].type != VAL_ARRAY)
                bp_fatal("array_clear expects (array)");
            args[0].as.arr->len = 0;
            return v_null();
        }
        case BI_ARRAY_INDEX_OF: {
            if (argc != 2 || args[0].type != VAL_ARRAY)
                bp_fatal("array_index_of expects (array, value)");
            BpArray *arr = args[0].as.arr;
            for (size_t i = 0; i < arr->len; i++) {
                Value v = arr->data[i];
                if (v.type == args[1].type) {
                    if (v.type == VAL_INT && v.as.i == args[1].as.i) return v_int((int64_t)i);
                    if (v.type == VAL_FLOAT && v.as.f == args[1].as.f) return v_int((int64_t)i);
                    if (v.type == VAL_BOOL && v.as.b == args[1].as.b) return v_int((int64_t)i);
                    if (v.type == VAL_STR && v.as.s->len == args[1].as.s->len &&
                        memcmp(v.as.s->data, args[1].as.s->data, v.as.s->len) == 0) return v_int((int64_t)i);
                }
            }
            return v_int(-1);
        }
        case BI_ARRAY_CONTAINS: {
            if (argc != 2 || args[0].type != VAL_ARRAY)
                bp_fatal("array_contains expects (array, value)");
            BpArray *arr = args[0].as.arr;
            for (size_t i = 0; i < arr->len; i++) {
                Value v = arr->data[i];
                if (v.type == args[1].type) {
                    if (v.type == VAL_INT && v.as.i == args[1].as.i) return v_bool(true);
                    if (v.type == VAL_FLOAT && v.as.f == args[1].as.f) return v_bool(true);
                    if (v.type == VAL_BOOL && v.as.b == args[1].as.b) return v_bool(true);
                    if (v.type == VAL_STR && v.as.s->len == args[1].as.s->len &&
                        memcmp(v.as.s->data, args[1].as.s->data, v.as.s->len) == 0) return v_bool(true);
                }
            }
            return v_bool(false);
        }
        case BI_ARRAY_REVERSE: {
            if (argc != 1 || args[0].type != VAL_ARRAY)
                bp_fatal("array_reverse expects (array)");
            BpArray *arr = args[0].as.arr;
            for (size_t i = 0; i < arr->len / 2; i++) {
                Value tmp = arr->data[i];
                arr->data[i] = arr->data[arr->len - 1 - i];
                arr->data[arr->len - 1 - i] = tmp;
            }
            return v_null();
        }
        case BI_ARRAY_FILL: {
            if (argc != 2 || args[0].type != VAL_INT)
                bp_fatal("array_fill expects (int, value)");
            int64_t size = args[0].as.i;
            if (size < 0) size = 0;
            BpArray *arr = gc_new_array(gc, (size_t)size);
            for (int64_t i = 0; i < size; i++) gc_array_push(gc, arr, args[1]);
            return v_array(arr);
        }
        case BI_STR_FROM_CHARS: {
            if (argc != 1 || args[0].type != VAL_ARRAY)
                bp_fatal("str_from_chars expects (array)");
            BpArray *arr = args[0].as.arr;
            char *buf = malloc(arr->len + 1);
            for (size_t i = 0; i < arr->len; i++) {
                buf[i] = (char)(arr->data[i].as.i & 0xFF);
            }
            buf[arr->len] = '\0';
            BpStr *s = gc_new_str(gc, buf, arr->len);
            free(buf);
            return v_str(s);
        }
        case BI_STR_BYTES: {
            if (argc != 1 || args[0].type != VAL_STR)
                bp_fatal("str_bytes expects (str)");
            BpStr *s = args[0].as.s;
            BpArray *arr = gc_new_array(gc, s->len);
            for (size_t i = 0; i < s->len; i++) {
                gc_array_push(gc, arr, v_int((int64_t)(unsigned char)s->data[i]));
            }
            return v_array(arr);
        }

        case BI_JSON_PARSE: {
            if (argc != 1 || args[0].type != VAL_STR)
                bp_fatal("json_parse expects (str)");
            const char *src = args[0].as.s->data;
            size_t pos = 0;
            size_t slen = args[0].as.s->len;
            return json_parse_value(src, slen, &pos, gc);
        }

        case BI_TAG: {
            if (argc != 1 || args[0].type != VAL_STRUCT)
                bp_fatal("tag expects (struct/union)");
            // The __tag field is always field 0 in a flattened union struct
            Value tag_val = gc_struct_get(args[0].as.st, 0);
            if (tag_val.type == VAL_STR) return tag_val;
            if (tag_val.type == VAL_INT) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)tag_val.as.i);
                return v_str(gc_new_str(gc, buf, strlen(buf)));
            }
            return v_str(gc_new_str(gc, "unknown", 7));
        }

        default: break;
    }
    bp_fatal("unknown builtin id");
    return v_null();
}
