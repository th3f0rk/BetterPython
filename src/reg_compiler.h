/*
 * BetterPython Register-Based Compiler
 * Generates register-based bytecode from AST
 */

#pragma once
#include "ast.h"
#include "bytecode.h"

// Compile module to register-based bytecode
BpModule reg_compile_module(const Module *m);
