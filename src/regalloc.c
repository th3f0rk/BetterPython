/*
 * BetterPython Register Allocator
 * Linear scan register allocation for the register-based VM
 */

#include "regalloc.h"
#include "util.h"
#include <string.h>

void reg_alloc_init(RegAlloc *ra, size_t param_count) {
    memset(ra, 0, sizeof(*ra));

    // r0 is reserved for return value
    ra->regs[0] = REG_RETURN;
    ra->max_used = 0;

    // Parameters go in r1, r2, r3, ... (r0 is return)
    // Actually, let's use r0..rN-1 for params, they can also be the return
    // Simpler: params in r0..r(param_count-1), return in r0
    for (size_t i = 0; i < param_count && i < MAX_REGS; i++) {
        ra->regs[i] = REG_PARAM;
        if (i > ra->max_used) ra->max_used = (uint8_t)i;
    }

    ra->param_count = (uint8_t)param_count;
    ra->next_free = (uint8_t)param_count;  // First free reg after params
    if (ra->next_free == 0) ra->next_free = 1;  // At least start at 1 if no params
    ra->var_count = 0;
}

void reg_alloc_free(RegAlloc *ra) {
    for (size_t i = 0; i < ra->var_count; i++) {
        free(ra->var_map[i].name);
    }
    memset(ra, 0, sizeof(*ra));
}

uint8_t reg_alloc_temp(RegAlloc *ra) {
    // Find first free register (use int to avoid type-limits warning)
    for (int i = ra->next_free; i < MAX_REGS; i++) {
        if (ra->regs[i] == REG_FREE) {
            ra->regs[i] = REG_ALLOCATED;
            if ((uint8_t)i > ra->max_used) ra->max_used = (uint8_t)i;
            ra->next_free = (uint8_t)(i + 1);
            return (uint8_t)i;
        }
    }
    // Wrap around and search from beginning (after params)
    for (int i = ra->param_count; i < ra->next_free; i++) {
        if (ra->regs[i] == REG_FREE) {
            ra->regs[i] = REG_ALLOCATED;
            if ((uint8_t)i > ra->max_used) ra->max_used = (uint8_t)i;
            return (uint8_t)i;
        }
    }
    bp_fatal("out of registers");
    return 0;
}

void reg_free_temp(RegAlloc *ra, uint8_t reg) {
    // Don't free parameter registers or return register
    if (ra->regs[reg] == REG_PARAM || ra->regs[reg] == REG_RETURN) {
        return;
    }
    // Don't free named variable registers
    for (size_t i = 0; i < ra->var_count; i++) {
        if (ra->var_map[i].reg == reg) {
            return;  // This is a variable, don't free
        }
    }
    ra->regs[reg] = REG_FREE;
    if (reg < ra->next_free) {
        ra->next_free = reg;  // Hint for next allocation
    }
}

// Allocate a parameter to its pre-reserved register slot
// Must be called in order: param 0 -> r0, param 1 -> r1, etc.
uint8_t reg_alloc_param(RegAlloc *ra, const char *name, uint8_t param_idx) {
    if (param_idx >= ra->param_count) {
        bp_fatal("parameter index %u out of range", param_idx);
    }

    // Add to variable map pointing to the pre-allocated parameter register
    if (ra->var_count >= MAX_REGS) {
        bp_fatal("too many variables");
    }
    ra->var_map[ra->var_count].name = bp_xstrdup(name);
    ra->var_map[ra->var_count].reg = param_idx;  // Parameters are r0, r1, r2...
    ra->var_count++;

    return param_idx;
}

uint8_t reg_alloc_var(RegAlloc *ra, const char *name) {
    // Check if already allocated
    for (size_t i = 0; i < ra->var_count; i++) {
        if (strcmp(ra->var_map[i].name, name) == 0) {
            return ra->var_map[i].reg;
        }
    }

    // Allocate new register for variable
    uint8_t reg = reg_alloc_temp(ra);

    // Add to variable map
    if (ra->var_count >= MAX_REGS) {
        bp_fatal("too many variables");
    }
    ra->var_map[ra->var_count].name = bp_xstrdup(name);
    ra->var_map[ra->var_count].reg = reg;
    ra->var_count++;

    return reg;
}

uint8_t reg_get_var(RegAlloc *ra, const char *name) {
    for (size_t i = 0; i < ra->var_count; i++) {
        if (strcmp(ra->var_map[i].name, name) == 0) {
            return ra->var_map[i].reg;
        }
    }
    bp_fatal("unknown variable '%s'", name);
    return 0;
}

bool reg_has_var(RegAlloc *ra, const char *name) {
    for (size_t i = 0; i < ra->var_count; i++) {
        if (strcmp(ra->var_map[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

uint8_t reg_max_used(RegAlloc *ra) {
    return ra->max_used + 1;  // Return count (max index + 1)
}

uint8_t reg_alloc_block(RegAlloc *ra, size_t count) {
    if (count == 0) return 0;
    if (count > MAX_REGS) bp_fatal("block too large");

    // Find a contiguous block of 'count' free registers
    // Start searching from after parameters
    for (int start = ra->param_count; start + (int)count <= MAX_REGS; start++) {
        bool found = true;
        for (size_t i = 0; i < count; i++) {
            if (ra->regs[start + i] != REG_FREE) {
                found = false;
                // Skip ahead past the occupied register
                start = start + (int)i;
                break;
            }
        }
        if (found) {
            // Mark all registers in the block as allocated
            for (size_t i = 0; i < count; i++) {
                ra->regs[start + i] = REG_ALLOCATED;
                if ((uint8_t)(start + i) > ra->max_used) {
                    ra->max_used = (uint8_t)(start + i);
                }
            }
            // Update next_free hint
            if (start + count < MAX_REGS) {
                ra->next_free = (uint8_t)(start + count);
            }
            return (uint8_t)start;
        }
    }
    bp_fatal("out of registers for block allocation");
    return 0;
}
