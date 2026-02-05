/*
 * BetterPython JIT - x86-64 Code Generation
 * Macros and helpers for emitting x86-64 machine code
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// x86-64 Register encoding
typedef enum {
    X64_RAX = 0,
    X64_RCX = 1,
    X64_RDX = 2,
    X64_RBX = 3,
    X64_RSP = 4,
    X64_RBP = 5,
    X64_RSI = 6,
    X64_RDI = 7,
    X64_R8  = 8,
    X64_R9  = 9,
    X64_R10 = 10,
    X64_R11 = 11,
    X64_R12 = 12,
    X64_R13 = 13,
    X64_R14 = 14,
    X64_R15 = 15
} X64Reg;

// Code buffer for generating machine code
typedef struct {
    uint8_t *code;          // Code memory
    size_t capacity;        // Total capacity
    size_t offset;          // Current write position

    // Labels for jump fixups
    struct {
        size_t offset;      // Position of label
        bool defined;       // Has been defined
    } labels[256];
    size_t label_count;

    // Jump fixups (forward references)
    struct {
        size_t code_offset; // Where the jump instruction is
        size_t label_idx;   // Which label it jumps to
        int8_t disp_size;   // 1 for rel8, 4 for rel32
    } fixups[1024];
    size_t fixup_count;

    bool error;             // Error flag
} CodeBuffer;

// Initialize code buffer
void cb_init(CodeBuffer *cb, uint8_t *mem, size_t capacity);

// Get current code size
size_t cb_size(CodeBuffer *cb);

// Emit raw bytes
void cb_emit8(CodeBuffer *cb, uint8_t b);
void cb_emit16(CodeBuffer *cb, uint16_t w);
void cb_emit32(CodeBuffer *cb, uint32_t d);
void cb_emit64(CodeBuffer *cb, uint64_t q);

// Create a new label, returns label index
size_t cb_new_label(CodeBuffer *cb);

// Define a label at current position
void cb_define_label(CodeBuffer *cb, size_t label_idx);

// Apply all jump fixups
bool cb_apply_fixups(CodeBuffer *cb);

// ============================================================================
// x86-64 Instruction Emission
// ============================================================================

// REX prefix helpers
#define REX_W   0x48    // 64-bit operand
#define REX_R   0x44    // Extension of ModR/M reg field
#define REX_X   0x42    // Extension of SIB index field
#define REX_B   0x41    // Extension of ModR/M r/m field

static inline uint8_t rex_wrxb(bool w, bool r, bool x, bool b) {
    return 0x40 | (w ? 8 : 0) | (r ? 4 : 0) | (x ? 2 : 0) | (b ? 1 : 0);
}

// ModR/M byte helpers
static inline uint8_t modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    return ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
}

// SIB byte helpers
static inline uint8_t sib(uint8_t scale, uint8_t index, uint8_t base) {
    return ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
}

// Need REX.R for registers r8-r15
static inline bool needs_rex_r(X64Reg reg) {
    return reg >= X64_R8;
}

// Need REX.B for registers r8-r15
static inline bool needs_rex_b(X64Reg rm) {
    return rm >= X64_R8;
}

// ============================================================================
// Common x86-64 Instructions
// ============================================================================

// NOP
void x64_nop(CodeBuffer *cb);

// RET
void x64_ret(CodeBuffer *cb);

// PUSH reg64
void x64_push_r64(CodeBuffer *cb, X64Reg reg);

// POP reg64
void x64_pop_r64(CodeBuffer *cb, X64Reg reg);

// MOV reg64, reg64
void x64_mov_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// MOV reg64, imm64
void x64_mov_r64_imm64(CodeBuffer *cb, X64Reg dst, int64_t imm);

// MOV reg64, imm32 (sign-extended)
void x64_mov_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm);

// MOV reg64, [reg64]
void x64_mov_r64_m64(CodeBuffer *cb, X64Reg dst, X64Reg base);

// MOV reg64, [reg64 + disp32]
void x64_mov_r64_m64_disp(CodeBuffer *cb, X64Reg dst, X64Reg base, int32_t disp);

// MOV [reg64], reg64
void x64_mov_m64_r64(CodeBuffer *cb, X64Reg base, X64Reg src);

// MOV [reg64 + disp32], reg64
void x64_mov_m64_disp_r64(CodeBuffer *cb, X64Reg base, int32_t disp, X64Reg src);

// ADD reg64, reg64
void x64_add_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// ADD reg64, imm32
void x64_add_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm);

// SUB reg64, reg64
void x64_sub_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// SUB reg64, imm32
void x64_sub_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm);

// IMUL reg64, reg64
void x64_imul_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// IDIV reg64 (divides RDX:RAX by reg, quotient in RAX, remainder in RDX)
void x64_idiv_r64(CodeBuffer *cb, X64Reg divisor);

// CQO (sign-extend RAX into RDX:RAX)
void x64_cqo(CodeBuffer *cb);

// NEG reg64
void x64_neg_r64(CodeBuffer *cb, X64Reg reg);

// XOR reg64, reg64
void x64_xor_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// AND reg64, reg64
void x64_and_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// OR reg64, reg64
void x64_or_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src);

// NOT reg64
void x64_not_r64(CodeBuffer *cb, X64Reg reg);

// CMP reg64, reg64
void x64_cmp_r64_r64(CodeBuffer *cb, X64Reg r1, X64Reg r2);

// CMP reg64, imm32
void x64_cmp_r64_imm32(CodeBuffer *cb, X64Reg reg, int32_t imm);

// TEST reg64, reg64
void x64_test_r64_r64(CodeBuffer *cb, X64Reg r1, X64Reg r2);

// SETcc reg8 (set byte based on condition)
void x64_sete(CodeBuffer *cb, X64Reg dst);   // Set if equal (ZF=1)
void x64_setne(CodeBuffer *cb, X64Reg dst);  // Set if not equal
void x64_setl(CodeBuffer *cb, X64Reg dst);   // Set if less (SF!=OF)
void x64_setle(CodeBuffer *cb, X64Reg dst);  // Set if less or equal
void x64_setg(CodeBuffer *cb, X64Reg dst);   // Set if greater
void x64_setge(CodeBuffer *cb, X64Reg dst);  // Set if greater or equal

// MOVZX reg64, reg8 (zero-extend byte to qword)
void x64_movzx_r64_r8(CodeBuffer *cb, X64Reg dst, X64Reg src);

// JMP rel32 (to label)
void x64_jmp_label(CodeBuffer *cb, size_t label_idx);

// JMP rel8 (short jump to label)
void x64_jmp_short_label(CodeBuffer *cb, size_t label_idx);

// Jcc rel32 (conditional jump to label)
void x64_jz_label(CodeBuffer *cb, size_t label_idx);   // Jump if zero
void x64_jnz_label(CodeBuffer *cb, size_t label_idx);  // Jump if not zero
void x64_jl_label(CodeBuffer *cb, size_t label_idx);   // Jump if less
void x64_jle_label(CodeBuffer *cb, size_t label_idx);  // Jump if less or equal
void x64_jg_label(CodeBuffer *cb, size_t label_idx);   // Jump if greater
void x64_jge_label(CodeBuffer *cb, size_t label_idx);  // Jump if greater or equal

// CALL rel32 (call to address)
void x64_call_rel32(CodeBuffer *cb, int32_t offset);

// CALL reg64 (indirect call)
void x64_call_r64(CodeBuffer *cb, X64Reg reg);

// LEA reg64, [reg64 + disp32]
void x64_lea_r64_m(CodeBuffer *cb, X64Reg dst, X64Reg base, int32_t disp);

// ============================================================================
// Floating Point (SSE2)
// ============================================================================

// MOVSD xmm, xmm
void x64_movsd_xmm_xmm(CodeBuffer *cb, int dst, int src);

// MOVSD xmm, [reg64 + disp]
void x64_movsd_xmm_m64(CodeBuffer *cb, int xmm, X64Reg base, int32_t disp);

// MOVSD [reg64 + disp], xmm
void x64_movsd_m64_xmm(CodeBuffer *cb, X64Reg base, int32_t disp, int xmm);

// ADDSD xmm, xmm
void x64_addsd(CodeBuffer *cb, int dst, int src);

// SUBSD xmm, xmm
void x64_subsd(CodeBuffer *cb, int dst, int src);

// MULSD xmm, xmm
void x64_mulsd(CodeBuffer *cb, int dst, int src);

// DIVSD xmm, xmm
void x64_divsd(CodeBuffer *cb, int dst, int src);

// UCOMISD xmm, xmm (compare floats, set flags)
void x64_ucomisd(CodeBuffer *cb, int xmm1, int xmm2);

// CVTSI2SD xmm, reg64 (convert int to double)
void x64_cvtsi2sd(CodeBuffer *cb, int xmm, X64Reg reg);

// CVTTSD2SI reg64, xmm (convert double to int, truncate)
void x64_cvttsd2si(CodeBuffer *cb, X64Reg reg, int xmm);
