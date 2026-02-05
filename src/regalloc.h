/*
 * BetterPython Register Allocator
 * Linear scan register allocation for the register-based VM
 */

#pragma once
#include "bytecode.h"
#include "ast.h"
#include <stdint.h>
#include <stdbool.h>

// Maximum registers per function
#define MAX_REGS 256

// Register state
typedef enum {
    REG_FREE = 0,
    REG_ALLOCATED,
    REG_PARAM,      // Reserved for function parameters
    REG_RETURN      // r0 is reserved for return value
} RegState;

// Register allocator context
typedef struct {
    RegState regs[MAX_REGS];
    uint8_t next_free;          // Next register to try allocating
    uint8_t max_used;           // Highest register used (for reg_count)
    uint8_t param_count;        // Number of parameter registers

    // Variable to register mapping
    struct {
        char *name;
        uint8_t reg;
    } var_map[MAX_REGS];
    size_t var_count;
} RegAlloc;

// Initialize register allocator for a function
void reg_alloc_init(RegAlloc *ra, size_t param_count);

// Free register allocator resources
void reg_alloc_free(RegAlloc *ra);

// Allocate a new temporary register
uint8_t reg_alloc_temp(RegAlloc *ra);

// Free a temporary register
void reg_free_temp(RegAlloc *ra, uint8_t reg);

// Allocate a parameter to its pre-reserved register slot (r0, r1, r2...)
uint8_t reg_alloc_param(RegAlloc *ra, const char *name, uint8_t param_idx);

// Allocate a register for a named variable
uint8_t reg_alloc_var(RegAlloc *ra, const char *name);

// Get the register for a named variable (must exist)
uint8_t reg_get_var(RegAlloc *ra, const char *name);

// Check if a variable has been allocated
bool reg_has_var(RegAlloc *ra, const char *name);

// Get the maximum register used (for BpFunc.reg_count)
uint8_t reg_max_used(RegAlloc *ra);
