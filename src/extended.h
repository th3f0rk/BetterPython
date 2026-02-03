#ifndef EXTENDED_H
#define EXTENDED_H

#include "bytecode.h"
#include "gc.h"

// Security and cryptography functions
Value security_hash_sha256(Value *args, uint16_t argc, Gc *gc);
Value security_hash_md5(Value *args, uint16_t argc, Gc *gc);
Value security_compare(Value *args, uint16_t argc);
Value security_random_bytes(Value *args, uint16_t argc, Gc *gc);

// Extended string utilities
Value util_str_reverse(Value *args, uint16_t argc, Gc *gc);
Value util_str_repeat(Value *args, uint16_t argc, Gc *gc);
Value util_str_pad_left(Value *args, uint16_t argc, Gc *gc);
Value util_str_pad_right(Value *args, uint16_t argc, Gc *gc);
Value util_str_contains(Value *args, uint16_t argc);
Value util_str_count(Value *args, uint16_t argc);
Value util_str_char_at(Value *args, uint16_t argc, Gc *gc);
Value util_str_index_of(Value *args, uint16_t argc);

// Character validation functions
Value validate_is_digit(Value *args, uint16_t argc);
Value validate_is_alpha(Value *args, uint16_t argc);
Value validate_is_alnum(Value *args, uint16_t argc);
Value validate_is_space(Value *args, uint16_t argc);

// Conversion functions
Value convert_int_to_hex(Value *args, uint16_t argc, Gc *gc);
Value convert_hex_to_int(Value *args, uint16_t argc);

// Extended math functions
Value math_clamp(Value *args, uint16_t argc);
Value math_sign(Value *args, uint16_t argc);

// File utility functions
Value file_get_size(Value *args, uint16_t argc);
Value file_copy_content(Value *args, uint16_t argc);

#endif
