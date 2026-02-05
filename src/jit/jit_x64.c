/*
 * BetterPython JIT - x86-64 Code Generation Implementation
 */

#include "jit_x64.h"
#include <string.h>
#include <stdio.h>

void cb_init(CodeBuffer *cb, uint8_t *mem, size_t capacity) {
    cb->code = mem;
    cb->capacity = capacity;
    cb->offset = 0;
    cb->label_count = 0;
    cb->fixup_count = 0;
    cb->error = false;
    memset(cb->labels, 0, sizeof(cb->labels));
}

size_t cb_size(CodeBuffer *cb) {
    return cb->offset;
}

void cb_emit8(CodeBuffer *cb, uint8_t b) {
    if (cb->offset >= cb->capacity) {
        cb->error = true;
        return;
    }
    cb->code[cb->offset++] = b;
}

void cb_emit16(CodeBuffer *cb, uint16_t w) {
    cb_emit8(cb, w & 0xFF);
    cb_emit8(cb, (w >> 8) & 0xFF);
}

void cb_emit32(CodeBuffer *cb, uint32_t d) {
    cb_emit8(cb, d & 0xFF);
    cb_emit8(cb, (d >> 8) & 0xFF);
    cb_emit8(cb, (d >> 16) & 0xFF);
    cb_emit8(cb, (d >> 24) & 0xFF);
}

void cb_emit64(CodeBuffer *cb, uint64_t q) {
    cb_emit32(cb, q & 0xFFFFFFFF);
    cb_emit32(cb, (q >> 32) & 0xFFFFFFFF);
}

size_t cb_new_label(CodeBuffer *cb) {
    if (cb->label_count >= 256) {
        cb->error = true;
        return 0;
    }
    size_t idx = cb->label_count++;
    cb->labels[idx].offset = 0;
    cb->labels[idx].defined = false;
    return idx;
}

void cb_define_label(CodeBuffer *cb, size_t label_idx) {
    if (label_idx >= cb->label_count) {
        cb->error = true;
        return;
    }
    cb->labels[label_idx].offset = cb->offset;
    cb->labels[label_idx].defined = true;
}

bool cb_apply_fixups(CodeBuffer *cb) {
    for (size_t i = 0; i < cb->fixup_count; i++) {
        size_t code_offset = cb->fixups[i].code_offset;
        size_t label_idx = cb->fixups[i].label_idx;
        int8_t disp_size = cb->fixups[i].disp_size;

        if (label_idx >= cb->label_count || !cb->labels[label_idx].defined) {
            fprintf(stderr, "JIT: Undefined label %zu\n", label_idx);
            return false;
        }

        // Calculate relative offset from end of instruction
        int64_t target = (int64_t)cb->labels[label_idx].offset;
        int64_t from = (int64_t)(code_offset + disp_size);
        int64_t rel = target - from;

        if (disp_size == 1) {
            if (rel < -128 || rel > 127) {
                fprintf(stderr, "JIT: rel8 overflow: %ld\n", (long)rel);
                return false;
            }
            cb->code[code_offset] = (int8_t)rel;
        } else if (disp_size == 4) {
            if (rel < INT32_MIN || rel > INT32_MAX) {
                fprintf(stderr, "JIT: rel32 overflow: %ld\n", (long)rel);
                return false;
            }
            int32_t rel32 = (int32_t)rel;
            cb->code[code_offset + 0] = rel32 & 0xFF;
            cb->code[code_offset + 1] = (rel32 >> 8) & 0xFF;
            cb->code[code_offset + 2] = (rel32 >> 16) & 0xFF;
            cb->code[code_offset + 3] = (rel32 >> 24) & 0xFF;
        }
    }
    return true;
}

// Helper to add a fixup
static void cb_add_fixup(CodeBuffer *cb, size_t code_offset, size_t label_idx, int8_t disp_size) {
    if (cb->fixup_count >= 1024) {
        cb->error = true;
        return;
    }
    cb->fixups[cb->fixup_count].code_offset = code_offset;
    cb->fixups[cb->fixup_count].label_idx = label_idx;
    cb->fixups[cb->fixup_count].disp_size = disp_size;
    cb->fixup_count++;
}

// ============================================================================
// Basic Instructions
// ============================================================================

void x64_nop(CodeBuffer *cb) {
    cb_emit8(cb, 0x90);
}

void x64_ret(CodeBuffer *cb) {
    cb_emit8(cb, 0xC3);
}

void x64_push_r64(CodeBuffer *cb, X64Reg reg) {
    if (needs_rex_b(reg)) {
        cb_emit8(cb, REX_B);
    }
    cb_emit8(cb, 0x50 + (reg & 7));
}

void x64_pop_r64(CodeBuffer *cb, X64Reg reg) {
    if (needs_rex_b(reg)) {
        cb_emit8(cb, REX_B);
    }
    cb_emit8(cb, 0x58 + (reg & 7));
}

void x64_mov_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x89);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_mov_r64_imm64(CodeBuffer *cb, X64Reg dst, int64_t imm) {
    // Use shorter encoding if possible
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        x64_mov_r64_imm32(cb, dst, (int32_t)imm);
        return;
    }
    // MOV r64, imm64 (REX.W + B8+rd)
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(dst)));
    cb_emit8(cb, 0xB8 + (dst & 7));
    cb_emit64(cb, (uint64_t)imm);
}

void x64_mov_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm) {
    // MOV r64, imm32 (sign-extended): REX.W C7 /0 id
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(dst)));
    cb_emit8(cb, 0xC7);
    cb_emit8(cb, modrm(3, 0, dst & 7));
    cb_emit32(cb, (uint32_t)imm);
}

void x64_mov_r64_m64(CodeBuffer *cb, X64Reg dst, X64Reg base) {
    // Special handling for RSP and R12 (need SIB byte)
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(dst), false, needs_rex_b(base)));
    cb_emit8(cb, 0x8B);
    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(0, dst & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));  // SIB for [RSP]
    } else if ((base & 7) == X64_RBP) {
        // [RBP] requires disp8 of 0
        cb_emit8(cb, modrm(1, dst & 7, base & 7));
        cb_emit8(cb, 0);
    } else {
        cb_emit8(cb, modrm(0, dst & 7, base & 7));
    }
}

void x64_mov_r64_m64_disp(CodeBuffer *cb, X64Reg dst, X64Reg base, int32_t disp) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(dst), false, needs_rex_b(base)));
    cb_emit8(cb, 0x8B);

    uint8_t mod = (disp >= -128 && disp <= 127) ? 1 : 2;

    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(mod, dst & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else {
        cb_emit8(cb, modrm(mod, dst & 7, base & 7));
    }

    if (mod == 1) {
        cb_emit8(cb, (int8_t)disp);
    } else {
        cb_emit32(cb, (uint32_t)disp);
    }
}

void x64_mov_m64_r64(CodeBuffer *cb, X64Reg base, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(base)));
    cb_emit8(cb, 0x89);
    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(0, src & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else if ((base & 7) == X64_RBP) {
        cb_emit8(cb, modrm(1, src & 7, base & 7));
        cb_emit8(cb, 0);
    } else {
        cb_emit8(cb, modrm(0, src & 7, base & 7));
    }
}

void x64_mov_m64_disp_r64(CodeBuffer *cb, X64Reg base, int32_t disp, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(base)));
    cb_emit8(cb, 0x89);

    uint8_t mod = (disp >= -128 && disp <= 127) ? 1 : 2;

    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(mod, src & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else {
        cb_emit8(cb, modrm(mod, src & 7, base & 7));
    }

    if (mod == 1) {
        cb_emit8(cb, (int8_t)disp);
    } else {
        cb_emit32(cb, (uint32_t)disp);
    }
}

// ============================================================================
// Arithmetic
// ============================================================================

void x64_add_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x01);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_add_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(dst)));
    if (imm >= -128 && imm <= 127) {
        cb_emit8(cb, 0x83);
        cb_emit8(cb, modrm(3, 0, dst & 7));
        cb_emit8(cb, (int8_t)imm);
    } else {
        cb_emit8(cb, 0x81);
        cb_emit8(cb, modrm(3, 0, dst & 7));
        cb_emit32(cb, (uint32_t)imm);
    }
}

void x64_sub_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x29);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_sub_r64_imm32(CodeBuffer *cb, X64Reg dst, int32_t imm) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(dst)));
    if (imm >= -128 && imm <= 127) {
        cb_emit8(cb, 0x83);
        cb_emit8(cb, modrm(3, 5, dst & 7));
        cb_emit8(cb, (int8_t)imm);
    } else {
        cb_emit8(cb, 0x81);
        cb_emit8(cb, modrm(3, 5, dst & 7));
        cb_emit32(cb, (uint32_t)imm);
    }
}

void x64_imul_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(dst), false, needs_rex_b(src)));
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0xAF);
    cb_emit8(cb, modrm(3, dst & 7, src & 7));
}

void x64_idiv_r64(CodeBuffer *cb, X64Reg divisor) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(divisor)));
    cb_emit8(cb, 0xF7);
    cb_emit8(cb, modrm(3, 7, divisor & 7));
}

void x64_cqo(CodeBuffer *cb) {
    cb_emit8(cb, REX_W);
    cb_emit8(cb, 0x99);
}

void x64_neg_r64(CodeBuffer *cb, X64Reg reg) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(reg)));
    cb_emit8(cb, 0xF7);
    cb_emit8(cb, modrm(3, 3, reg & 7));
}

// ============================================================================
// Logical
// ============================================================================

void x64_xor_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x31);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_and_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x21);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_or_r64_r64(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(src), false, needs_rex_b(dst)));
    cb_emit8(cb, 0x09);
    cb_emit8(cb, modrm(3, src & 7, dst & 7));
}

void x64_not_r64(CodeBuffer *cb, X64Reg reg) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(reg)));
    cb_emit8(cb, 0xF7);
    cb_emit8(cb, modrm(3, 2, reg & 7));
}

// ============================================================================
// Comparison
// ============================================================================

void x64_cmp_r64_r64(CodeBuffer *cb, X64Reg r1, X64Reg r2) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(r2), false, needs_rex_b(r1)));
    cb_emit8(cb, 0x39);
    cb_emit8(cb, modrm(3, r2 & 7, r1 & 7));
}

void x64_cmp_r64_imm32(CodeBuffer *cb, X64Reg reg, int32_t imm) {
    cb_emit8(cb, rex_wrxb(true, false, false, needs_rex_b(reg)));
    if (imm >= -128 && imm <= 127) {
        cb_emit8(cb, 0x83);
        cb_emit8(cb, modrm(3, 7, reg & 7));
        cb_emit8(cb, (int8_t)imm);
    } else {
        cb_emit8(cb, 0x81);
        cb_emit8(cb, modrm(3, 7, reg & 7));
        cb_emit32(cb, (uint32_t)imm);
    }
}

void x64_test_r64_r64(CodeBuffer *cb, X64Reg r1, X64Reg r2) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(r2), false, needs_rex_b(r1)));
    cb_emit8(cb, 0x85);
    cb_emit8(cb, modrm(3, r2 & 7, r1 & 7));
}

// SETcc - Set byte on condition
static void x64_setcc(CodeBuffer *cb, uint8_t cc, X64Reg dst) {
    if (needs_rex_b(dst)) {
        cb_emit8(cb, REX_B);
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x90 + cc);
    cb_emit8(cb, modrm(3, 0, dst & 7));
}

void x64_sete(CodeBuffer *cb, X64Reg dst)  { x64_setcc(cb, 0x04, dst); }
void x64_setne(CodeBuffer *cb, X64Reg dst) { x64_setcc(cb, 0x05, dst); }
void x64_setl(CodeBuffer *cb, X64Reg dst)  { x64_setcc(cb, 0x0C, dst); }
void x64_setle(CodeBuffer *cb, X64Reg dst) { x64_setcc(cb, 0x0E, dst); }
void x64_setg(CodeBuffer *cb, X64Reg dst)  { x64_setcc(cb, 0x0F, dst); }
void x64_setge(CodeBuffer *cb, X64Reg dst) { x64_setcc(cb, 0x0D, dst); }

void x64_movzx_r64_r8(CodeBuffer *cb, X64Reg dst, X64Reg src) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(dst), false, needs_rex_b(src)));
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0xB6);
    cb_emit8(cb, modrm(3, dst & 7, src & 7));
}

// ============================================================================
// Control Flow
// ============================================================================

void x64_jmp_label(CodeBuffer *cb, size_t label_idx) {
    cb_emit8(cb, 0xE9);  // JMP rel32
    size_t fixup_pos = cb->offset;
    cb_emit32(cb, 0);    // Placeholder
    cb_add_fixup(cb, fixup_pos, label_idx, 4);
}

void x64_jmp_short_label(CodeBuffer *cb, size_t label_idx) {
    cb_emit8(cb, 0xEB);  // JMP rel8
    size_t fixup_pos = cb->offset;
    cb_emit8(cb, 0);     // Placeholder
    cb_add_fixup(cb, fixup_pos, label_idx, 1);
}

// Jcc rel32
static void x64_jcc_label(CodeBuffer *cb, uint8_t cc, size_t label_idx) {
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x80 + cc);
    size_t fixup_pos = cb->offset;
    cb_emit32(cb, 0);
    cb_add_fixup(cb, fixup_pos, label_idx, 4);
}

void x64_jz_label(CodeBuffer *cb, size_t label_idx)  { x64_jcc_label(cb, 0x04, label_idx); }
void x64_jnz_label(CodeBuffer *cb, size_t label_idx) { x64_jcc_label(cb, 0x05, label_idx); }
void x64_jl_label(CodeBuffer *cb, size_t label_idx)  { x64_jcc_label(cb, 0x0C, label_idx); }
void x64_jle_label(CodeBuffer *cb, size_t label_idx) { x64_jcc_label(cb, 0x0E, label_idx); }
void x64_jg_label(CodeBuffer *cb, size_t label_idx)  { x64_jcc_label(cb, 0x0F, label_idx); }
void x64_jge_label(CodeBuffer *cb, size_t label_idx) { x64_jcc_label(cb, 0x0D, label_idx); }

void x64_call_rel32(CodeBuffer *cb, int32_t offset) {
    cb_emit8(cb, 0xE8);
    cb_emit32(cb, (uint32_t)offset);
}

void x64_call_r64(CodeBuffer *cb, X64Reg reg) {
    if (needs_rex_b(reg)) {
        cb_emit8(cb, REX_B);
    }
    cb_emit8(cb, 0xFF);
    cb_emit8(cb, modrm(3, 2, reg & 7));
}

void x64_lea_r64_m(CodeBuffer *cb, X64Reg dst, X64Reg base, int32_t disp) {
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(dst), false, needs_rex_b(base)));
    cb_emit8(cb, 0x8D);

    uint8_t mod = (disp >= -128 && disp <= 127) ? 1 : 2;

    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(mod, dst & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else {
        cb_emit8(cb, modrm(mod, dst & 7, base & 7));
    }

    if (mod == 1) {
        cb_emit8(cb, (int8_t)disp);
    } else {
        cb_emit32(cb, (uint32_t)disp);
    }
}

// ============================================================================
// SSE2 Floating Point
// ============================================================================

void x64_movsd_xmm_xmm(CodeBuffer *cb, int dst, int src) {
    cb_emit8(cb, 0xF2);
    if (dst >= 8 || src >= 8) {
        cb_emit8(cb, rex_wrxb(false, dst >= 8, false, src >= 8));
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x10);
    cb_emit8(cb, modrm(3, dst & 7, src & 7));
}

void x64_movsd_xmm_m64(CodeBuffer *cb, int xmm, X64Reg base, int32_t disp) {
    cb_emit8(cb, 0xF2);
    if (xmm >= 8 || base >= 8) {
        cb_emit8(cb, rex_wrxb(false, xmm >= 8, false, needs_rex_b(base)));
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x10);

    uint8_t mod = (disp >= -128 && disp <= 127) ? 1 : 2;

    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(mod, xmm & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else {
        cb_emit8(cb, modrm(mod, xmm & 7, base & 7));
    }

    if (mod == 1) {
        cb_emit8(cb, (int8_t)disp);
    } else {
        cb_emit32(cb, (uint32_t)disp);
    }
}

void x64_movsd_m64_xmm(CodeBuffer *cb, X64Reg base, int32_t disp, int xmm) {
    cb_emit8(cb, 0xF2);
    if (xmm >= 8 || base >= 8) {
        cb_emit8(cb, rex_wrxb(false, xmm >= 8, false, needs_rex_b(base)));
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x11);

    uint8_t mod = (disp >= -128 && disp <= 127) ? 1 : 2;

    if ((base & 7) == X64_RSP) {
        cb_emit8(cb, modrm(mod, xmm & 7, 4));
        cb_emit8(cb, sib(0, 4, 4));
    } else {
        cb_emit8(cb, modrm(mod, xmm & 7, base & 7));
    }

    if (mod == 1) {
        cb_emit8(cb, (int8_t)disp);
    } else {
        cb_emit32(cb, (uint32_t)disp);
    }
}

static void x64_sse_arith(CodeBuffer *cb, uint8_t op, int dst, int src) {
    cb_emit8(cb, 0xF2);
    if (dst >= 8 || src >= 8) {
        cb_emit8(cb, rex_wrxb(false, dst >= 8, false, src >= 8));
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, op);
    cb_emit8(cb, modrm(3, dst & 7, src & 7));
}

void x64_addsd(CodeBuffer *cb, int dst, int src) { x64_sse_arith(cb, 0x58, dst, src); }
void x64_subsd(CodeBuffer *cb, int dst, int src) { x64_sse_arith(cb, 0x5C, dst, src); }
void x64_mulsd(CodeBuffer *cb, int dst, int src) { x64_sse_arith(cb, 0x59, dst, src); }
void x64_divsd(CodeBuffer *cb, int dst, int src) { x64_sse_arith(cb, 0x5E, dst, src); }

void x64_ucomisd(CodeBuffer *cb, int xmm1, int xmm2) {
    cb_emit8(cb, 0x66);
    if (xmm1 >= 8 || xmm2 >= 8) {
        cb_emit8(cb, rex_wrxb(false, xmm1 >= 8, false, xmm2 >= 8));
    }
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x2E);
    cb_emit8(cb, modrm(3, xmm1 & 7, xmm2 & 7));
}

void x64_cvtsi2sd(CodeBuffer *cb, int xmm, X64Reg reg) {
    cb_emit8(cb, 0xF2);
    cb_emit8(cb, rex_wrxb(true, xmm >= 8, false, needs_rex_b(reg)));
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x2A);
    cb_emit8(cb, modrm(3, xmm & 7, reg & 7));
}

void x64_cvttsd2si(CodeBuffer *cb, X64Reg reg, int xmm) {
    cb_emit8(cb, 0xF2);
    cb_emit8(cb, rex_wrxb(true, needs_rex_r(reg), false, xmm >= 8));
    cb_emit8(cb, 0x0F);
    cb_emit8(cb, 0x2C);
    cb_emit8(cb, modrm(3, reg & 7, xmm & 7));
}
