#include "security.h"
#include "util.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/* Simple SHA256 implementation for hashing */
static uint32_t rotate_right(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32 - count));
}

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
    uint32_t a, b, c, d, e, f, g, h;
    
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
    }
    
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotate_right(w[i-15], 7) ^ rotate_right(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotate_right(w[i-2], 17) ^ rotate_right(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void security_sha256(const char *data, size_t len, char output[65]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *padded = bp_xmalloc(padded_len);
    memset(padded, 0, padded_len);
    memcpy(padded, data, len);
    
    padded[len] = 0x80;
    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; i++) {
        padded[padded_len - 1 - i] = (bit_len >> (i * 8)) & 0xFF;
    }
    
    for (size_t i = 0; i < padded_len; i += 64) {
        sha256_transform(state, padded + i);
    }
    
    for (int i = 0; i < 8; i++) {
        sprintf(output + i * 8, "%08x", state[i]);
    }
    output[64] = '\0';
    
    free(padded);
}

/* Simple MD5 implementation */
static uint32_t md5_f(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static uint32_t md5_g(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static uint32_t md5_h(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static uint32_t md5_i(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
static uint32_t md5_rotate_left(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

void security_md5(const char *data, size_t len, char output[33]) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *padded = bp_xmalloc(padded_len);
    memset(padded, 0, padded_len);
    memcpy(padded, data, len);
    
    padded[len] = 0x80;
    uint64_t bit_len = len * 8;
    memcpy(padded + padded_len - 8, &bit_len, 8);
    
    for (size_t offset = 0; offset < padded_len; offset += 64) {
        uint32_t m[16];
        for (int i = 0; i < 16; i++) {
            m[i] = ((uint32_t)padded[offset + i*4]) | ((uint32_t)padded[offset + i*4 + 1] << 8) |
                   ((uint32_t)padded[offset + i*4 + 2] << 16) | ((uint32_t)padded[offset + i*4 + 3] << 24);
        }
        
        uint32_t a = a0, b = b0, c = c0, d = d0;
        
        for (int i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16) {
                f = md5_f(b, c, d);
                g = i;
            } else if (i < 32) {
                f = md5_g(b, c, d);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = md5_h(b, c, d);
                g = (3 * i + 5) % 16;
            } else {
                f = md5_i(b, c, d);
                g = (7 * i) % 16;
            }
            
            static const uint32_t k[64] = {
                0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
            };
            
            static const uint32_t s[64] = {
                7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
            };
            
            uint32_t temp = d;
            d = c;
            c = b;
            b = b + md5_rotate_left(a + f + k[i] + m[g], s[i]);
            a = temp;
        }
        
        a0 += a; b0 += b; c0 += c; d0 += d;
    }
    
    sprintf(output, "%08x%08x%08x%08x", a0, b0, c0, d0);
    free(padded);
}

/* Constant-time string comparison for security */
int security_constant_time_compare(const char *a, const char *b, size_t len) {
    int result = 0;
    for (size_t i = 0; i < len; i++) {
        result |= a[i] ^ b[i];
    }
    return result == 0;
}

/* Generate cryptographically random bytes */
int security_random_bytes(uint8_t *buffer, size_t length) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return 0;
    
    size_t total = 0;
    while (total < length) {
        ssize_t n = read(fd, buffer + total, length - total);
        if (n <= 0) {
            close(fd);
            return 0;
        }
        total += n;
    }
    
    close(fd);
    return 1;
}
