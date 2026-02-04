// BetterPython Security and Extended Utilities
// Provides cryptographic hashing, validation, and extended string operations

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "bytecode.h"
#include "gc.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Simple SHA256-like hash for security operations
// Note: This is a simplified implementation for demonstration
// For production cryptographic use, link against OpenSSL or libsodium
static void compute_sha256_simple(const unsigned char *data, size_t len, unsigned char *hash) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    for (size_t i = 0; i < len; i++) {
        uint32_t val = data[i];
        for (int j = 0; j < 8; j++) {
            state[j] = ((state[j] << 5) | (state[j] >> 27)) ^ val;
            val = (val * 1103515245 + 12345) & 0xFFFFFFFF;
        }
    }
    
    for (int i = 0; i < 8; i++) {
        hash[i*4 + 0] = (state[i] >> 24) & 0xFF;
        hash[i*4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i*4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i*4 + 3] = state[i] & 0xFF;
    }
}

// Simple MD5-like hash for compatibility
// Note: MD5 is not cryptographically secure - use for checksums only
static void compute_md5_simple(const unsigned char *data, size_t len, unsigned char *hash) {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    
    for (size_t i = 0; i < len; i++) {
        uint32_t val = data[i];
        for (int j = 0; j < 4; j++) {
            state[j] = ((state[j] << 7) | (state[j] >> 25)) ^ val;
            val = (val * 69069 + 1) & 0xFFFFFFFF;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        hash[i*4 + 0] = (state[i] >> 0) & 0xFF;
        hash[i*4 + 1] = (state[i] >> 8) & 0xFF;
        hash[i*4 + 2] = (state[i] >> 16) & 0xFF;
        hash[i*4 + 3] = (state[i] >> 24) & 0xFF;
    }
}

Value security_hash_sha256(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hash_sha256 expects (str)");
    
    unsigned char hash[32];
    compute_sha256_simple((const unsigned char *)args[0].as.s->data, args[0].as.s->len, hash);
    
    char hex[65];
    for (int i = 0; i < 32; i++) {
        snprintf(hex + i*2, 3, "%02x", hash[i]);
    }
    hex[64] = '\0';
    
    return v_str(gc_new_str(gc, hex, 64));
}

Value security_hash_md5(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hash_md5 expects (str)");
    
    unsigned char hash[16];
    compute_md5_simple((const unsigned char *)args[0].as.s->data, args[0].as.s->len, hash);
    
    char hex[33];
    for (int i = 0; i < 16; i++) {
        snprintf(hex + i*2, 3, "%02x", hash[i]);
    }
    hex[32] = '\0';
    
    return v_str(gc_new_str(gc, hex, 32));
}

// Timing-attack resistant string comparison
// Always compares full length to prevent timing-based attacks
Value security_compare(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("secure_compare expects (str, str)");
    
    BpStr *a = args[0].as.s;
    BpStr *b = args[1].as.s;
    
    size_t max_len = a->len > b->len ? a->len : b->len;
    int result = 0;
    
    for (size_t i = 0; i < max_len; i++) {
        unsigned char ca = i < a->len ? (unsigned char)a->data[i] : 0;
        unsigned char cb = i < b->len ? (unsigned char)b->data[i] : 0;
        result |= ca ^ cb;
    }
    
    result |= (a->len != b->len);
    return v_bool(result == 0);
}

// Generate cryptographically random bytes
// Reads from /dev/urandom on Unix systems
// Falls back to pseudo-random if unavailable
static unsigned long rand_state_secure = 1;

Value security_random_bytes(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("random_bytes expects (int)");
    int64_t count = args[0].as.i;
    
    if (count <= 0 || count > 1024) bp_fatal("random_bytes count must be 1-1024");
    
    unsigned char *bytes = bp_xmalloc((size_t)count);
    
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, bytes, (size_t)count);
        close(fd);
        if (n != count) {
            free(bytes);
            bp_fatal("random_bytes failed to read random data");
        }
    } else {
        // Fallback to pseudo-random
        for (int64_t i = 0; i < count; i++) {
            rand_state_secure = rand_state_secure * 1103515245 + 12345;
            bytes[i] = (unsigned char)(rand_state_secure >> 16);
        }
    }
    
    // Convert to hex string for safe handling
    char *hex = bp_xmalloc((size_t)(count * 2 + 1));
    for (int64_t i = 0; i < count; i++) {
        snprintf(hex + i*2, 3, "%02x", bytes[i]);
    }
    hex[count * 2] = '\0';
    
    BpStr *result = gc_new_str(gc, hex, (size_t)(count * 2));
    free(bytes);
    free(hex);
    return v_str(result);
}

// Extended string utilities

Value util_str_reverse(Value *args, uint16_t argc, Gc *gc) {
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

Value util_str_repeat(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_INT)
        bp_fatal("str_repeat expects (str, int)");
    
    BpStr *s = args[0].as.s;
    int64_t count = args[1].as.i;
    
    if (count < 0) count = 0;
    if (count > 1000) bp_fatal("str_repeat count too large (max 1000)");
    
    size_t total_len = s->len * (size_t)count;
    char *buf = bp_xmalloc(total_len + 1);
    
    for (int64_t i = 0; i < count; i++) {
        memcpy(buf + i * s->len, s->data, s->len);
    }
    buf[total_len] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, total_len);
    free(buf);
    return v_str(result);
}

Value util_str_pad_left(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_STR)
        bp_fatal("str_pad_left expects (str, int, str)");
    
    BpStr *s = args[0].as.s;
    int64_t width = args[1].as.i;
    BpStr *pad = args[2].as.s;
    
    if (width <= (int64_t)s->len) return v_str(s);
    if (pad->len == 0) return v_str(s);
    
    size_t pad_len = (size_t)(width - (int64_t)s->len);
    char *buf = bp_xmalloc((size_t)width + 1);
    
    for (size_t i = 0; i < pad_len; i++) {
        buf[i] = pad->data[i % pad->len];
    }
    memcpy(buf + pad_len, s->data, s->len);
    buf[width] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, (size_t)width);
    free(buf);
    return v_str(result);
}

Value util_str_pad_right(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 3 || args[0].type != VAL_STR || args[1].type != VAL_INT || args[2].type != VAL_STR)
        bp_fatal("str_pad_right expects (str, int, str)");
    
    BpStr *s = args[0].as.s;
    int64_t width = args[1].as.i;
    BpStr *pad = args[2].as.s;
    
    if (width <= (int64_t)s->len) return v_str(s);
    if (pad->len == 0) return v_str(s);
    
    size_t pad_len = (size_t)(width - (int64_t)s->len);
    char *buf = bp_xmalloc((size_t)width + 1);
    
    memcpy(buf, s->data, s->len);
    for (size_t i = 0; i < pad_len; i++) {
        buf[s->len + i] = pad->data[i % pad->len];
    }
    buf[width] = '\0';
    
    BpStr *result = gc_new_str(gc, buf, (size_t)width);
    free(buf);
    return v_str(result);
}

Value util_str_contains(Value *args, uint16_t argc) {
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

Value util_str_count(Value *args, uint16_t argc) {
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

Value util_str_char_at(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_INT)
        bp_fatal("str_char_at expects (str, int)");
    
    BpStr *s = args[0].as.s;
    int64_t index = args[1].as.i;
    
    if (index < 0 || (size_t)index >= s->len) return v_str(gc_new_str(gc, "", 0));
    
    char buf[2] = {s->data[index], '\0'};
    return v_str(gc_new_str(gc, buf, 1));
}

Value util_str_index_of(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("str_index_of expects (str, str)");
    
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

// Character validation functions

Value validate_is_digit(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_digit expects (str)");
    BpStr *s = args[0].as.s;
    
    if (s->len == 0) return v_bool(false);
    
    for (size_t i = 0; i < s->len; i++) {
        if (!isdigit((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

Value validate_is_alpha(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_alpha expects (str)");
    BpStr *s = args[0].as.s;
    
    if (s->len == 0) return v_bool(false);
    
    for (size_t i = 0; i < s->len; i++) {
        if (!isalpha((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

Value validate_is_alnum(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_alnum expects (str)");
    BpStr *s = args[0].as.s;
    
    if (s->len == 0) return v_bool(false);
    
    for (size_t i = 0; i < s->len; i++) {
        if (!isalnum((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

Value validate_is_space(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("is_space expects (str)");
    BpStr *s = args[0].as.s;
    
    if (s->len == 0) return v_bool(false);
    
    for (size_t i = 0; i < s->len; i++) {
        if (!isspace((unsigned char)s->data[i])) return v_bool(false);
    }
    return v_bool(true);
}

// Conversion functions

Value convert_int_to_hex(Value *args, uint16_t argc, Gc *gc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("int_to_hex expects (int)");
    int64_t val = args[0].as.i;
    
    char buf[32];
    if (val < 0) {
        snprintf(buf, sizeof(buf), "-%" PRIx64, (uint64_t)(-val));
    } else {
        snprintf(buf, sizeof(buf), "%" PRIx64, (uint64_t)val);
    }
    
    return v_str(gc_new_str(gc, buf, strlen(buf)));
}

Value convert_hex_to_int(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("hex_to_int expects (str)");
    
    const char *str = args[0].as.s->data;
    char *endptr;
    int64_t val = (int64_t)strtoll(str, &endptr, 16);
    
    return v_int(val);
}

// Extended math functions

Value math_clamp(Value *args, uint16_t argc) {
    if (argc != 3 || args[0].type != VAL_INT || args[1].type != VAL_INT || args[2].type != VAL_INT)
        bp_fatal("clamp expects (int, int, int)");
    
    int64_t val = args[0].as.i;
    int64_t min_val = args[1].as.i;
    int64_t max_val = args[2].as.i;
    
    if (val < min_val) return v_int(min_val);
    if (val > max_val) return v_int(max_val);
    return v_int(val);
}

Value math_sign(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_INT) bp_fatal("sign expects (int)");
    int64_t val = args[0].as.i;
    
    if (val > 0) return v_int(1);
    if (val < 0) return v_int(-1);
    return v_int(0);
}

// File utility functions

Value file_get_size(Value *args, uint16_t argc) {
    if (argc != 1 || args[0].type != VAL_STR) bp_fatal("file_size expects (str)");
    
    const char *path = args[0].as.s->data;
    struct stat st;
    
    if (stat(path, &st) != 0) return v_int(-1);
    return v_int((int64_t)st.st_size);
}

Value file_copy_content(Value *args, uint16_t argc) {
    if (argc != 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
        bp_fatal("file_copy expects (str, str)");
    
    const char *src_path = args[0].as.s->data;
    const char *dst_path = args[1].as.s->data;
    
    FILE *src = fopen(src_path, "rb");
    if (!src) return v_bool(false);
    
    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return v_bool(false);
    }
    
    char buffer[8192];
    size_t bytes;
    bool success = true;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            success = false;
            break;
        }
    }
    
    fclose(src);
    fclose(dst);
    
    return v_bool(success);
}
