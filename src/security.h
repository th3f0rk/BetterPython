#pragma once
#include <stddef.h>
#include <stdint.h>

/* SHA256 hashing function
 * Produces a 64-character hexadecimal hash from input data
 * Thread-safe, stateless implementation
 */
void security_sha256(const char *data, size_t len, char output[65]);

/* MD5 hashing function
 * Produces a 32-character hexadecimal hash from input data
 * Note: MD5 is cryptographically broken, use only for checksums
 */
void security_md5(const char *data, size_t len, char output[33]);

/* Constant-time string comparison
 * Prevents timing attacks by ensuring comparison always takes same time
 * Returns 1 if strings match, 0 otherwise
 */
int security_constant_time_compare(const char *a, const char *b, size_t len);

/* Generate cryptographically secure random bytes
 * Uses /dev/urandom on Unix systems
 * Returns 1 on success, 0 on failure
 */
int security_random_bytes(uint8_t *buffer, size_t length);
