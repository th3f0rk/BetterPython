#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "stdlib.h"
#include "security.h"
#include "util.h"
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

static BpStr *val_to_str(Value v, Gc *gc) {
    char buf[64];
    if (v.type == VAL_INT) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
        return gc_new_str(gc, buf, strlen(buf));
    }
    if (v.type == VAL_BOOL) {
        const char *s = v.as.b ? "true" : "false";
        return gc_new_str(gc, s, strlen(s));
    }
    if (v.type == VAL_NULL) {
        return gc_new_str(gc, "null", 4);
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

static Value bi_str_index_of(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_index_of expects (str, str)");
    return bi_str_find(args, argc);
}

/* Validation functions */
static Value bi_is_digit(Value *args, uint16_t argc) {
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
        
        default: break;
    }
    bp_fatal("unknown builtin id");
    return v_null();
}
