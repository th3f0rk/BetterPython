/*
 * Multi-module compilation for BetterPython
 *
 * This module handles the compilation of multiple BetterPython source files
 * that use import/export relationships. It creates a single merged bytecode
 * module with qualified function names for cross-module references.
 */

#include "multi_compile.h"
#include "compiler.h"
#include "types.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

// Create qualified name: module$function
static char *make_qualified_name(const char *module, const char *func) {
    if (strcmp(module, "__main__") == 0) {
        // Main module functions keep their original names
        return bp_xstrdup(func);
    }
    size_t len = strlen(module) + 1 + strlen(func) + 1;
    char *qname = bp_xmalloc(len);
    snprintf(qname, len, "%s$%s", module, func);
    return qname;
}

// Add symbol to table
static void symbol_table_add(SymbolTable *st, const char *short_name,
                             const char *qualified_name, const char *module_name,
                             size_t fn_index, bool is_exported) {
    if (st->count + 1 > st->cap) {
        st->cap = st->cap ? st->cap * 2 : 32;
        st->entries = bp_xrealloc(st->entries, st->cap * sizeof(*st->entries));
    }
    SymbolEntry *e = &st->entries[st->count++];
    e->short_name = bp_xstrdup(short_name);
    e->qualified_name = bp_xstrdup(qualified_name);
    e->module_name = bp_xstrdup(module_name);
    e->fn_index = fn_index;
    e->is_exported = is_exported;
}

// Find symbol by qualified name (used for future enhancements)
__attribute__((unused))
static SymbolEntry *symbol_table_find_qualified(SymbolTable *st, const char *qname) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].qualified_name, qname) == 0) {
            return &st->entries[i];
        }
    }
    return NULL;
}

// Find symbol by module and short name (used for future enhancements)
__attribute__((unused))
static SymbolEntry *symbol_table_find(SymbolTable *st, const char *module, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].module_name, module) == 0 &&
            strcmp(st->entries[i].short_name, name) == 0) {
            return &st->entries[i];
        }
    }
    return NULL;
}

// Find exported symbol from a module (used for future enhancements)
__attribute__((unused))
static SymbolEntry *symbol_table_find_export(SymbolTable *st, const char *module, const char *name) {
    for (size_t i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].module_name, module) == 0 &&
            strcmp(st->entries[i].short_name, name) == 0 &&
            st->entries[i].is_exported) {
            return &st->entries[i];
        }
    }
    return NULL;
}

void symbol_table_free(SymbolTable *st) {
    for (size_t i = 0; i < st->count; i++) {
        free(st->entries[i].short_name);
        free(st->entries[i].qualified_name);
        free(st->entries[i].module_name);
    }
    free(st->entries);
    memset(st, 0, sizeof(*st));
}

// Rename function calls in bytecode to use qualified names
// This patches the function indices in CALL opcodes
// Note: Currently unused as we resolve indices in AST before compilation
__attribute__((unused))
static void patch_function_calls(BpFunc *func, size_t *fn_remap, size_t fn_count) {
    uint8_t *code = func->code;
    size_t len = func->code_len;
    size_t i = 0;

    while (i < len) {
        uint8_t op = code[i];
        switch (op) {
            case OP_CONST_I64:
                i += 9;  // opcode + 8 bytes
                break;
            case OP_CONST_F64:
                i += 9;
                break;
            case OP_CONST_BOOL:
                i += 2;
                break;
            case OP_CONST_STR:
                i += 5;  // opcode + 4 bytes
                break;
            case OP_LOAD_LOCAL:
            case OP_STORE_LOCAL:
                i += 3;  // opcode + 2 bytes
                break;
            case OP_JMP:
            case OP_JMP_IF_FALSE:
                i += 5;  // opcode + 4 bytes
                break;
            case OP_CALL: {
                // Patch function index
                i++;  // skip opcode
                uint16_t old_idx;
                memcpy(&old_idx, &code[i], 2);
                if (old_idx < fn_count) {
                    uint16_t new_idx = (uint16_t)fn_remap[old_idx];
                    memcpy(&code[i], &new_idx, 2);
                }
                i += 3;  // fn_idx (2) + argc (1)
                break;
            }
            case OP_CALL_BUILTIN:
                i += 4;  // opcode + builtin_id (2) + argc (1)
                break;
            case OP_ARRAY_NEW:
            case OP_MAP_NEW:
                i += 5;  // opcode + count (u32 = 4)
                break;
            case OP_STRUCT_NEW:
                i += 5;  // opcode + type_id (u16) + field_count (u16) = 4
                break;
            case OP_STRUCT_GET:
            case OP_STRUCT_SET:
                i += 3;  // opcode + field_idx (2)
                break;
            case OP_TRY_BEGIN:
                i += 11;  // opcode + catch_addr (4) + finally_addr (4) + var_slot (2)
                break;
            default:
                i++;  // Most opcodes are 1 byte
                break;
        }
    }
}


// Forward declaration for recursive expression walk
static void resolve_calls_in_expr(Expr *e, SymbolTable *symbols, const char *current_module);

// Resolve function call indices in a statement
static void resolve_calls_in_stmt(Stmt *st, SymbolTable *symbols, const char *current_module) {
    if (!st) return;

    switch (st->kind) {
        case ST_LET:
            if (st->as.let.init) resolve_calls_in_expr(st->as.let.init, symbols, current_module);
            break;
        case ST_ASSIGN:
            resolve_calls_in_expr(st->as.assign.value, symbols, current_module);
            break;
        case ST_INDEX_ASSIGN:
            resolve_calls_in_expr(st->as.idx_assign.array, symbols, current_module);
            resolve_calls_in_expr(st->as.idx_assign.index, symbols, current_module);
            resolve_calls_in_expr(st->as.idx_assign.value, symbols, current_module);
            break;
        case ST_EXPR:
            resolve_calls_in_expr(st->as.expr.expr, symbols, current_module);
            break;
        case ST_IF:
            resolve_calls_in_expr(st->as.ifs.cond, symbols, current_module);
            for (size_t i = 0; i < st->as.ifs.then_len; i++)
                resolve_calls_in_stmt(st->as.ifs.then_stmts[i], symbols, current_module);
            for (size_t i = 0; i < st->as.ifs.else_len; i++)
                resolve_calls_in_stmt(st->as.ifs.else_stmts[i], symbols, current_module);
            break;
        case ST_WHILE:
            resolve_calls_in_expr(st->as.wh.cond, symbols, current_module);
            for (size_t i = 0; i < st->as.wh.body_len; i++)
                resolve_calls_in_stmt(st->as.wh.body[i], symbols, current_module);
            break;
        case ST_FOR:
            resolve_calls_in_expr(st->as.forr.start, symbols, current_module);
            resolve_calls_in_expr(st->as.forr.end, symbols, current_module);
            for (size_t i = 0; i < st->as.forr.body_len; i++)
                resolve_calls_in_stmt(st->as.forr.body[i], symbols, current_module);
            break;
        case ST_RETURN:
            if (st->as.ret.value) resolve_calls_in_expr(st->as.ret.value, symbols, current_module);
            break;
        case ST_TRY:
            for (size_t i = 0; i < st->as.try_catch.try_len; i++)
                resolve_calls_in_stmt(st->as.try_catch.try_stmts[i], symbols, current_module);
            for (size_t i = 0; i < st->as.try_catch.catch_len; i++)
                resolve_calls_in_stmt(st->as.try_catch.catch_stmts[i], symbols, current_module);
            for (size_t i = 0; i < st->as.try_catch.finally_len; i++)
                resolve_calls_in_stmt(st->as.try_catch.finally_stmts[i], symbols, current_module);
            break;
        case ST_THROW:
            if (st->as.throw.value) resolve_calls_in_expr(st->as.throw.value, symbols, current_module);
            break;
        default:
            break;
    }
}

// Resolve all function call indices to global indices
static void resolve_calls_in_expr(Expr *e, SymbolTable *symbols, const char *current_module) {
    if (!e) return;

    switch (e->kind) {
        case EX_CALL:
            // Skip builtin calls (fn_index == -1)
            if (e->as.call.fn_index != -1) {
                // Look up function by name in symbol table
                // For cross-module calls (fn_index == -2), use the qualified name directly
                // For within-module calls (fn_index >= 0), construct qualified name
                const char *lookup_name = e->as.call.name;
                char *qualified_name = NULL;

                if (e->as.call.fn_index == -2) {
                    // Cross-module call - name is already qualified
                    lookup_name = e->as.call.name;
                } else {
                    // Within-module call - construct qualified name
                    if (strcmp(current_module, "__main__") != 0) {
                        size_t qlen = strlen(current_module) + 1 + strlen(e->as.call.name) + 1;
                        qualified_name = bp_xmalloc(qlen);
                        snprintf(qualified_name, qlen, "%s$%s", current_module, e->as.call.name);
                        lookup_name = qualified_name;
                    }
                }

                // Find in symbol table
                for (size_t i = 0; i < symbols->count; i++) {
                    if (strcmp(symbols->entries[i].qualified_name, lookup_name) == 0) {
                        e->as.call.fn_index = (int)symbols->entries[i].fn_index;
                        break;
                    }
                }

                if (qualified_name) free(qualified_name);
            }
            // Resolve in arguments
            for (size_t i = 0; i < e->as.call.argc; i++)
                resolve_calls_in_expr(e->as.call.args[i], symbols, current_module);
            break;
        case EX_UNARY:
            resolve_calls_in_expr(e->as.unary.rhs, symbols, current_module);
            break;
        case EX_BINARY:
            resolve_calls_in_expr(e->as.binary.lhs, symbols, current_module);
            resolve_calls_in_expr(e->as.binary.rhs, symbols, current_module);
            break;
        case EX_ARRAY:
            for (size_t i = 0; i < e->as.array.len; i++)
                resolve_calls_in_expr(e->as.array.elements[i], symbols, current_module);
            break;
        case EX_INDEX:
            resolve_calls_in_expr(e->as.index.array, symbols, current_module);
            resolve_calls_in_expr(e->as.index.index, symbols, current_module);
            break;
        case EX_MAP:
            for (size_t i = 0; i < e->as.map.len; i++) {
                resolve_calls_in_expr(e->as.map.keys[i], symbols, current_module);
                resolve_calls_in_expr(e->as.map.values[i], symbols, current_module);
            }
            break;
        case EX_STRUCT_LITERAL:
            for (size_t i = 0; i < e->as.struct_literal.field_count; i++)
                resolve_calls_in_expr(e->as.struct_literal.field_values[i], symbols, current_module);
            break;
        case EX_FIELD_ACCESS:
            resolve_calls_in_expr(e->as.field_access.object, symbols, current_module);
            break;
        case EX_METHOD_CALL:
            resolve_calls_in_expr(e->as.method_call.object, symbols, current_module);
            for (size_t i = 0; i < e->as.method_call.argc; i++)
                resolve_calls_in_expr(e->as.method_call.args[i], symbols, current_module);
            break;
        default:
            break;
    }
}

// Resolve all function calls in a module's AST to global indices
static void resolve_module_calls(Module *ast, SymbolTable *symbols, const char *module_name) {
    for (size_t f = 0; f < ast->fnc; f++) {
        Function *fn = &ast->fns[f];
        for (size_t s = 0; s < fn->body_len; s++) {
            resolve_calls_in_stmt(fn->body[s], symbols, module_name);
        }
    }
}

bool multi_compile(ModuleGraph *g, BpModule *out) {
    // Get topologically sorted module order
    size_t order_count = 0;
    size_t *order = module_graph_topo_sort(g, &order_count);
    if (!order && g->count > 0) {
        fprintf(stderr, "error: circular dependency detected\n");
        return false;
    }

    // Build symbol table with all functions from all modules
    SymbolTable symbols = {0};

    // First pass: collect all functions and create qualified names
    size_t total_funcs = 0;
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);
        total_funcs += mi->ast.fnc;
    }

    // Create function index mapping for each module
    typedef struct {
        size_t module_idx;
        size_t *local_to_global;  // Maps local fn index to global fn index
        size_t fn_count;
    } ModuleFnMap;

    ModuleFnMap *fn_maps = bp_xmalloc(order_count * sizeof(*fn_maps));
    memset(fn_maps, 0, order_count * sizeof(*fn_maps));

    // Clear the type checker's function table - we'll register all functions globally
    types_clear_external_functions();

    size_t global_fn_idx = 0;
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);
        fn_maps[i].module_idx = order[i];
        fn_maps[i].fn_count = mi->ast.fnc;
        fn_maps[i].local_to_global = bp_xmalloc(mi->ast.fnc * sizeof(size_t));

        for (size_t f = 0; f < mi->ast.fnc; f++) {
            Function *fn = &mi->ast.fns[f];
            char *qname = make_qualified_name(mi->name, fn->name);

            symbol_table_add(&symbols, fn->name, qname, mi->name,
                           global_fn_idx, fn->is_export);

            // Register function with type checker using qualified name
            // This allows cross-module calls like math_utils.square() to be resolved
            Type *param_types = NULL;
            if (fn->paramc > 0) {
                param_types = bp_xmalloc(fn->paramc * sizeof(Type));
                for (size_t p = 0; p < fn->paramc; p++) {
                    param_types[p] = fn->params[p].type;
                }
            }
            types_register_function(qname, param_types, fn->paramc, fn->ret_type, global_fn_idx);

            fn_maps[i].local_to_global[f] = global_fn_idx;
            global_fn_idx++;
            free(qname);
        }
    }

    // Second pass: type-check each module with original names
    // This allows within-module recursive calls to work correctly
    // Set preserve flag so pre-registered external functions are kept
    types_set_preserve_external(true);
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);
        typecheck_module(&mi->ast);
    }
    types_set_preserve_external(false);  // Reset flag

    // Third pass: resolve ALL function call indices to global indices
    // This handles both:
    // - Cross-module calls (fn_index == -2): look up qualified name directly
    // - Within-module calls: construct qualified name from module$function
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);
        resolve_module_calls(&mi->ast, &symbols, mi->name);
    }

    // Fourth pass: rename functions to qualified names after resolution
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);
        if (strcmp(mi->name, "__main__") == 0) continue;  // Skip main module

        for (size_t f = 0; f < mi->ast.fnc; f++) {
            Function *fn = &mi->ast.fns[f];
            char *qname = make_qualified_name(mi->name, fn->name);
            free(fn->name);
            fn->name = qname;
        }
    }

    // Fifth pass: compile each module
    // We'll compile each module separately and merge them

    // Allocate output arrays
    out->fn_len = total_funcs;
    out->funcs = bp_xmalloc(total_funcs * sizeof(*out->funcs));
    memset(out->funcs, 0, total_funcs * sizeof(*out->funcs));
    out->entry = 0;

    // Collect all strings
    char **all_strings = NULL;
    size_t all_str_count = 0;
    size_t all_str_cap = 0;

    size_t fn_offset = 0;
    for (size_t i = 0; i < order_count; i++) {
        ModuleInfo *mi = module_graph_get(g, order[i]);

        // Compile this module
        BpModule compiled = compile_module(&mi->ast);

        // Copy functions with updated indices
        for (size_t f = 0; f < compiled.fn_len; f++) {
            BpFunc *src = &compiled.funcs[f];
            BpFunc *dst = &out->funcs[fn_offset + f];

            dst->name = src->name;  // Take ownership
            src->name = NULL;
            dst->arity = src->arity;
            dst->locals = src->locals;
            dst->code = src->code;
            src->code = NULL;
            dst->code_len = src->code_len;
            dst->format = src->format;
            dst->reg_count = src->reg_count;

            // Update string constant IDs
            dst->str_const_len = src->str_const_len;
            if (src->str_const_len > 0) {
                dst->str_const_ids = bp_xmalloc(src->str_const_len * sizeof(uint32_t));
                for (size_t s = 0; s < src->str_const_len; s++) {
                    // Add string to merged pool and get new ID
                    uint32_t old_id = src->str_const_ids[s];
                    const char *str = compiled.strings[old_id];

                    // Find or add string to merged pool
                    uint32_t new_id = (uint32_t)-1;
                    for (size_t si = 0; si < all_str_count; si++) {
                        if (strcmp(all_strings[si], str) == 0) {
                            new_id = (uint32_t)si;
                            break;
                        }
                    }
                    if (new_id == (uint32_t)-1) {
                        if (all_str_count + 1 > all_str_cap) {
                            all_str_cap = all_str_cap ? all_str_cap * 2 : 32;
                            all_strings = bp_xrealloc(all_strings, all_str_cap * sizeof(char *));
                        }
                        new_id = (uint32_t)all_str_count;
                        all_strings[all_str_count++] = bp_xstrdup(str);
                    }

                    dst->str_const_ids[s] = new_id;
                }
                free(src->str_const_ids);
                src->str_const_ids = NULL;
            }

            // Note: Function call indices are already resolved to global indices
            // by resolve_module_calls() in the AST before compilation.
            // No bytecode patching needed here.
            (void)fn_maps;  // Suppress unused warning

            // Find main function
            if (strcmp(mi->name, "__main__") == 0 && strcmp(dst->name, "main") == 0) {
                out->entry = (uint32_t)(fn_offset + f);
            }
        }

        fn_offset += compiled.fn_len;

        // Free compiled module (but not the transferred data)
        for (size_t s = 0; s < compiled.str_len; s++) {
            free(compiled.strings[s]);
        }
        free(compiled.strings);
        free(compiled.funcs);
    }

    // Set merged string pool
    out->strings = all_strings;
    out->str_len = all_str_count;

    // Merge extern functions from all modules and patch OP_FFI_CALL indices
    {
        BpExternFunc *all_externs = NULL;
        size_t all_extern_count = 0;
        size_t all_extern_cap = 0;

        // Collect all externs, tracking per-module offset for bytecode patching
        size_t *extern_offsets = bp_xmalloc(order_count * sizeof(size_t));
        size_t fn_off = 0;
        for (size_t i = 0; i < order_count; i++) {
            ModuleInfo *mi = module_graph_get(g, order[i]);
            extern_offsets[i] = all_extern_count;

            for (size_t e = 0; e < mi->ast.externc; e++) {
                ExternDef *ed = &mi->ast.externs[e];
                // Add to merged extern list
                if (all_extern_count + 1 > all_extern_cap) {
                    all_extern_cap = all_extern_cap ? all_extern_cap * 2 : 8;
                    all_externs = bp_xrealloc(all_externs, all_extern_cap * sizeof(*all_externs));
                }
                BpExternFunc *ef = &all_externs[all_extern_count++];
                memset(ef, 0, sizeof(*ef));
                ef->name = bp_xstrdup(ed->name);
                ef->c_name = ed->c_name ? bp_xstrdup(ed->c_name) : bp_xstrdup(ed->name);
                ef->library = bp_xstrdup(ed->library);
                ef->param_count = (uint16_t)ed->paramc;
                ef->is_variadic = ed->is_variadic;
                ef->ret_type = 0; /* Will be set from type info below */
                switch (ed->ret_type.kind) {
                    case TY_FLOAT: ef->ret_type = FFI_TC_FLOAT; break;
                    case TY_STR:   ef->ret_type = FFI_TC_STR; break;
                    case TY_PTR:   ef->ret_type = FFI_TC_PTR; break;
                    case TY_VOID:  ef->ret_type = FFI_TC_VOID; break;
                    default:       ef->ret_type = FFI_TC_INT; break;
                }
                if (ed->paramc > 0) {
                    ef->param_types = bp_xmalloc(ed->paramc);
                    for (size_t p = 0; p < ed->paramc; p++) {
                        switch (ed->params[p].type.kind) {
                            case TY_FLOAT: ef->param_types[p] = FFI_TC_FLOAT; break;
                            case TY_STR:   ef->param_types[p] = FFI_TC_STR; break;
                            case TY_PTR:   ef->param_types[p] = FFI_TC_PTR; break;
                            default:       ef->param_types[p] = FFI_TC_INT; break;
                        }
                    }
                }
            }

            // Patch OP_FFI_CALL instructions in this module's functions
            if (extern_offsets[i] > 0) {
                for (size_t f = 0; f < mi->ast.fnc; f++) {
                    BpFunc *func = &out->funcs[fn_off + f];
                    uint8_t *code = func->code;
                    size_t ip = 0;
                    while (ip < func->code_len) {
                        uint8_t op = code[ip++];
                        if (op == OP_FFI_CALL) {
                            // Patch the extern_id by adding the module's offset
                            uint16_t old_id;
                            memcpy(&old_id, &code[ip], 2);
                            uint16_t new_id = old_id + (uint16_t)extern_offsets[i];
                            memcpy(&code[ip], &new_id, 2);
                            ip += 2; // extern_id
                            ip += 1; // argc
                        } else {
                            // Skip instruction operands based on opcode
                            switch (op) {
                                case OP_CONST_I64: case OP_CONST_F64: ip += 8; break;
                                case OP_CONST_BOOL: ip += 1; break;
                                case OP_CONST_STR: ip += 4; break;
                                case OP_LOAD_LOCAL: case OP_STORE_LOCAL: ip += 2; break;
                                case OP_JMP: case OP_JMP_IF_FALSE: ip += 4; break;
                                case OP_CALL: case OP_CALL_BUILTIN: ip += 3; break;
                                case OP_ARRAY_NEW: case OP_MAP_NEW: ip += 4; break;
                                case OP_STRUCT_NEW: ip += 4; break;
                                case OP_STRUCT_GET: case OP_STRUCT_SET: ip += 2; break;
                                case OP_CLASS_NEW: case OP_METHOD_CALL: case OP_SUPER_CALL: ip += 3; break;
                                case OP_CLASS_GET: case OP_CLASS_SET: ip += 2; break;
                                case OP_TRY_BEGIN: ip += 10; break;
                                default: break; /* All other opcodes have no operands */
                            }
                        }
                    }
                }
            }

            fn_off += mi->ast.fnc;
        }

        out->extern_funcs = all_externs;
        out->extern_func_len = all_extern_count;
        free(extern_offsets);
    }

    // Cleanup
    for (size_t i = 0; i < order_count; i++) {
        free(fn_maps[i].local_to_global);
    }
    free(fn_maps);
    free(order);
    symbol_table_free(&symbols);

    return true;
}
