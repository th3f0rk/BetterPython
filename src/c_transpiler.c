/*
 * BetterPython → C Transpiler
 *
 * Walks the type-checked AST and emits equivalent C source code.
 * The generated C links against bp_runtime.{c,h}.
 */
#include "c_transpiler.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ── helpers ─────────────────────────────────────────── */

typedef struct {
    FILE       *out;
    int         indent;
    const Module *mod;
    int         label_counter;   /* unique labels for loops */
    /* Loop label stack for break/continue */
    int         loop_labels[256];
    int         loop_depth;
} CGen;

static void cg_indent(CGen *cg) {
    for (int i = 0; i < cg->indent; i++) fputs("    ", cg->out);
}

static int cg_fresh_label(CGen *cg) {
    return cg->label_counter++;
}

static void push_loop_label(CGen *cg, int label) {
    if (cg->loop_depth < 256) cg->loop_labels[cg->loop_depth++] = label;
}

static void pop_loop_label(CGen *cg) {
    if (cg->loop_depth > 0) cg->loop_depth--;
}

/* ── C-safe name mangling ─────────────────────────────── */

static bool is_c_keyword(const char *s) {
    static const char *kw[] = {
        "auto","break","case","char","const","continue","default","do",
        "double","else","enum","extern","float","for","goto","if",
        "int","long","register","return","short","signed","sizeof","static",
        "struct","switch","typedef","union","unsigned","void","volatile","while",
        "_Bool","_Complex","_Imaginary","inline","restrict",
        NULL
    };
    for (int i = 0; kw[i]; i++) {
        if (strcmp(s, kw[i]) == 0) return true;
    }
    return false;
}

static void emit_safe_name(CGen *cg, const char *name) {
    if (strcmp(name, "main") == 0) {
        fputs("bp_main", cg->out);
    } else if (is_c_keyword(name)) {
        fprintf(cg->out, "bp_%s", name);
    } else {
        fputs(name, cg->out);
    }
}

/* ── Type emission ────────────────────────────────────── */

static void emit_type(CGen *cg, const Type *t) {
    switch (t->kind) {
        case TY_INT:  case TY_I64: fputs("int64_t", cg->out); break;
        case TY_I8:  fputs("int8_t", cg->out); break;
        case TY_I16: fputs("int16_t", cg->out); break;
        case TY_I32: fputs("int32_t", cg->out); break;
        case TY_U8:  fputs("uint8_t", cg->out); break;
        case TY_U16: fputs("uint16_t", cg->out); break;
        case TY_U32: fputs("uint32_t", cg->out); break;
        case TY_U64: fputs("uint64_t", cg->out); break;
        case TY_FLOAT: fputs("double", cg->out); break;
        case TY_BOOL:  fputs("bool", cg->out); break;
        case TY_STR:   fputs("BpRtStr*", cg->out); break;
        case TY_VOID:  fputs("void", cg->out); break;
        case TY_ARRAY: fputs("BpRtArray*", cg->out); break;
        case TY_MAP:   fputs("BpRtMap*", cg->out); break;
        case TY_STRUCT:
            fprintf(cg->out, "BpStruct_%s", t->struct_name ? t->struct_name : "anon");
            break;
        case TY_ENUM:
            fputs("int64_t", cg->out);  /* enums are ints */
            break;
        case TY_TUPLE: fputs("RtVal", cg->out); break;
        case TY_FUNC:  fputs("void*", cg->out); break;
        case TY_CLASS: fputs("RtVal", cg->out); break;
        case TY_PTR:   fputs("void*", cg->out); break;
    }
}

/* Return true if the type is a "simple" scalar (not heap-allocated) */
static bool is_scalar_type(TypeKind k) {
    return k == TY_INT || k == TY_FLOAT || k == TY_BOOL ||
           k == TY_I8 || k == TY_I16 || k == TY_I32 || k == TY_I64 ||
           k == TY_U8 || k == TY_U16 || k == TY_U32 || k == TY_U64 ||
           k == TY_ENUM || k == TY_VOID;
}

/* ── Expression emission ──────────────────────────────── */

static void emit_expr(CGen *cg, const Expr *e);
static const char *binop_str(BinaryOp op);

/* Emit an expression, casting integer values/results to double.
   This is used when assigning an int expression to a float variable. */
static void emit_expr_as_float(CGen *cg, const Expr *e) {
    if (e->kind == EX_INT) {
        fprintf(cg->out, "%lld.0", (long long)e->as.int_val);
        return;
    }
    if (e->kind == EX_BINARY && e->inferred.kind != TY_FLOAT) {
        /* Recursively force float on binary operands for arithmetic ops */
        BinaryOp op = e->as.binary.op;
        if (op == BOP_ADD || op == BOP_SUB || op == BOP_MUL || op == BOP_DIV || op == BOP_MOD) {
            fputc('(', cg->out);
            emit_expr_as_float(cg, e->as.binary.lhs);
            fprintf(cg->out, " %s ", binop_str(op));
            emit_expr_as_float(cg, e->as.binary.rhs);
            fputc(')', cg->out);
            return;
        }
    }
    if (e->kind == EX_UNARY && e->as.unary.op == UOP_NEG && e->inferred.kind != TY_FLOAT) {
        fputs("(-(double)", cg->out);
        emit_expr(cg, e->as.unary.rhs);
        fputc(')', cg->out);
        return;
    }
    if (e->kind == EX_VAR && e->inferred.kind != TY_FLOAT) {
        fputs("(double)", cg->out);
        emit_expr(cg, e);
        return;
    }
    /* Default: if the expression is already float, emit normally */
    if (e->inferred.kind == TY_FLOAT) {
        emit_expr(cg, e);
        return;
    }
    /* Otherwise wrap with (double) cast */
    fputs("(double)(", cg->out);
    emit_expr(cg, e);
    fputc(')', cg->out);
}

/* Emit a C string literal with proper escaping */
static void emit_c_string_literal(CGen *cg, const char *s) {
    fputc('"', cg->out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", cg->out); break;
            case '"':  fputs("\\\"", cg->out); break;
            case '\n': fputs("\\n", cg->out); break;
            case '\r': fputs("\\r", cg->out); break;
            case '\t': fputs("\\t", cg->out); break;
            case '\0': fputs("\\0", cg->out); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    fprintf(cg->out, "\\x%02x", (unsigned char)*p);
                } else {
                    fputc(*p, cg->out);
                }
        }
    }
    fputc('"', cg->out);
}

static const char *binop_str(BinaryOp op) {
    switch (op) {
        case BOP_ADD: return "+";
        case BOP_SUB: return "-";
        case BOP_MUL: return "*";
        case BOP_DIV: return "/";
        case BOP_MOD: return "%";
        case BOP_EQ:  return "==";
        case BOP_NEQ: return "!=";
        case BOP_LT:  return "<";
        case BOP_LTE: return "<=";
        case BOP_GT:  return ">";
        case BOP_GTE: return ">=";
        case BOP_AND: return "&&";
        case BOP_OR:  return "||";
    }
    return "?";
}

/* Emit a call to a builtin function */
static void emit_builtin_call(CGen *cg, const char *name, Expr **args, size_t argc) {
    /* I/O */
    if (strcmp(name, "print") == 0) {
        if (argc == 1 && is_scalar_type(args[0]->inferred.kind)) {
            /* For scalar types, wrap in RtVal for the generic printer */
            fputs("bp_print_val(", cg->out);
            TypeKind k = args[0]->inferred.kind;
            if (k == TY_INT || k == TY_I64 || k == TY_I8 || k == TY_I16 || k == TY_I32 ||
                k == TY_U8 || k == TY_U16 || k == TY_U32 || k == TY_U64 || k == TY_ENUM)
                fputs("rv_int(", cg->out);
            else if (k == TY_FLOAT)
                fputs("rv_float(", cg->out);
            else if (k == TY_BOOL)
                fputs("rv_bool(", cg->out);
            else
                fputs("rv_int(", cg->out);
            emit_expr(cg, args[0]);
            fputs("))", cg->out);
        } else if (argc == 1 && args[0]->inferred.kind == TY_STR) {
            fputs("bp_print_val(rv_str(", cg->out);
            emit_expr(cg, args[0]);
            fputs("))", cg->out);
        } else {
            /* variadic: bp_print_vals(count, rv_xxx(a), rv_xxx(b), ...) */
            fprintf(cg->out, "bp_print_vals(%zu", argc);
            for (size_t i = 0; i < argc; i++) {
                fputs(", ", cg->out);
                TypeKind k = args[i]->inferred.kind;
                if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, args[i]); fputc(')', cg->out); }
                else if (k == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, args[i]); fputc(')', cg->out); }
                else if (k == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, args[i]); fputc(')', cg->out); }
                else if (k == TY_ARRAY) { fputs("rv_arr(", cg->out); emit_expr(cg, args[i]); fputc(')', cg->out); }
                else if (k == TY_MAP) { fputs("rv_map(", cg->out); emit_expr(cg, args[i]); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, args[i]); fputs("))", cg->out); }
            }
            fputc(')', cg->out);
        }
        return;
    }
    if (strcmp(name, "read_line") == 0) { fputs("bp_read_line()", cg->out); return; }

    /* Conversion */
    if (strcmp(name, "to_str") == 0 && argc == 1) {
        TypeKind k = args[0]->inferred.kind;
        fputs("bp_to_str(", cg->out);
        if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, args[0]); fputc(')', cg->out); }
        else if (k == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, args[0]); fputc(')', cg->out); }
        else if (k == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, args[0]); fputc(')', cg->out); }
        else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, args[0]); fputs("))", cg->out); }
        fputc(')', cg->out);
        return;
    }

    /* String ops */
    if (strcmp(name, "len") == 0 && argc == 1) {
        fputs("bp_len(", cg->out); emit_expr(cg, args[0]); fputc(')', cg->out); return;
    }
    if (strcmp(name, "substr") == 0 && argc == 3) {
        fputs("bp_substr(", cg->out);
        emit_expr(cg, args[0]); fputs(", ", cg->out);
        emit_expr(cg, args[1]); fputs(", ", cg->out);
        emit_expr(cg, args[2]); fputc(')', cg->out); return;
    }

    /* Map 1:1 builtins: name -> bp_name */
    /* Simple 1-arg builtins */
    struct { const char *bp; const char *c; int nargs; } map[] = {
        {"str_upper", "bp_str_upper", 1},
        {"str_lower", "bp_str_lower", 1},
        {"str_trim", "bp_str_trim", 1},
        {"str_reverse", "bp_str_reverse", 1},
        {"starts_with", "bp_starts_with", 2},
        {"ends_with", "bp_ends_with", 2},
        {"str_find", "bp_str_find", 2},
        {"str_replace", "bp_str_replace", 3},
        {"str_repeat", "bp_str_repeat", 2},
        {"str_pad_left", "bp_str_pad_left", 3},
        {"str_pad_right", "bp_str_pad_right", 3},
        {"str_contains", "bp_str_contains", 2},
        {"str_count", "bp_str_count", 2},
        {"str_char_at", "bp_str_char_at", 2},
        {"str_index_of", "bp_str_index_of", 2},
        {"str_split", "bp_str_split", 2},
        {"str_join", "bp_str_join", 2},
        {"str_split_str", "bp_str_split_str", 2},
        {"str_join_arr", "bp_str_join_arr", 2},
        {"str_concat_all", "bp_str_concat_all", 1},
        {"chr", "bp_chr", 1},
        {"ord", "bp_ord", 1},
        {"int_to_hex", "bp_int_to_hex", 1},
        {"hex_to_int", "bp_hex_to_int", 1},
        {"is_digit", "bp_is_digit", 1},
        {"is_alpha", "bp_is_alpha", 1},
        {"is_alnum", "bp_is_alnum", 1},
        {"is_space", "bp_is_space", 1},
        /* Math (int) */
        {"abs", "bp_abs", 1},
        {"min", "bp_min", 2},
        {"max", "bp_max", 2},
        {"pow", "bp_pow_int", 2},
        {"sqrt", "bp_sqrt_int", 1},
        {"floor", "bp_floor_int", 1},
        {"ceil", "bp_ceil_int", 1},
        {"round", "bp_round_int", 1},
        {"clamp", "bp_clamp", 3},
        {"sign", "bp_sign", 1},
        /* Math (float) */
        {"sin", "bp_sin", 1},
        {"cos", "bp_cos", 1},
        {"tan", "bp_tan", 1},
        {"asin", "bp_asin_f", 1},
        {"acos", "bp_acos_f", 1},
        {"atan", "bp_atan_f", 1},
        {"atan2", "bp_atan2_f", 2},
        {"log", "bp_log_f", 1},
        {"log10", "bp_log10_f", 1},
        {"log2", "bp_log2_f", 1},
        {"exp", "bp_exp_f", 1},
        {"fabs", "bp_fabs_f", 1},
        {"ffloor", "bp_ffloor", 1},
        {"fceil", "bp_fceil", 1},
        {"fround", "bp_fround", 1},
        {"fsqrt", "bp_fsqrt", 1},
        {"fpow", "bp_fpow", 2},
        /* Float conversion */
        {"int_to_float", "bp_int_to_float", 1},
        {"float_to_int", "bp_float_to_int", 1},
        {"float_to_str", "bp_float_to_str", 1},
        {"str_to_float", "bp_str_to_float", 1},
        {"is_nan", "bp_is_nan", 1},
        {"is_inf", "bp_is_inf", 1},
        /* File I/O */
        {"file_read", "bp_file_read", 1},
        {"file_write", "bp_file_write", 2},
        {"file_append", "bp_file_append", 2},
        {"file_exists", "bp_file_exists", 1},
        {"file_delete", "bp_file_delete", 1},
        {"file_size", "bp_file_size", 1},
        {"file_copy", "bp_file_copy", 2},
        /* Encoding */
        {"base64_encode", "bp_base64_encode", 1},
        {"base64_decode", "bp_base64_decode", 1},
        /* Random */
        {"rand", "bp_rand", 0},
        {"rand_range", "bp_rand_range", 2},
        {"rand_seed", "bp_rand_seed", 1},
        /* System */
        {"clock_ms", "bp_clock_ms", 0},
        {"sleep", "bp_sleep_ms", 1},
        {"getenv", "bp_getenv_str", 1},
        {"exit", "bp_exit", 1},
        {"argv", "bp_argv_get", 1},
        {"argc", "bp_argc_get", 0},
        /* Array operations */
        {"array_len", "bp_array_len", 1},
        {"array_push", "bp_array_push", 2},
        {"array_pop", "bp_array_pop", 1},
        /* Map operations */
        {"map_len", "bp_map_len", 1},
        {"map_keys", "bp_map_keys", 1},
        {"map_values", "bp_map_values", 1},
        {"map_has_key", "bp_map_has_key", 2},
        {"map_delete", "bp_map_delete", 2},
        /* Regex */
        {"regex_match", "bp_regex_match", 2},
        {"regex_search", "bp_regex_search", 2},
        {"regex_replace", "bp_regex_replace", 3},
        {"regex_split", "bp_regex_split", 2},
        {"regex_find_all", "bp_regex_find_all", 2},
        /* Security */
        {"hash_sha256", "bp_hash_sha256", 1},
        {"hash_md5", "bp_hash_md5", 1},
        {"secure_compare", "bp_secure_compare", 2},
        {"random_bytes", "bp_random_bytes", 1},
        {NULL, NULL, 0}
    };

    for (int i = 0; map[i].bp; i++) {
        if (strcmp(name, map[i].bp) == 0) {
            fprintf(cg->out, "%s(", map[i].c);
            for (size_t a = 0; a < argc; a++) {
                if (a) fputs(", ", cg->out);
                /* For array_push, the second arg needs wrapping */
                if (strcmp(name, "array_push") == 0 && a == 1) {
                    TypeKind k = args[a]->inferred.kind;
                    if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else if (k == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else if (k == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else if (k == TY_ARRAY) { fputs("rv_arr(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else if (k == TY_MAP) { fputs("rv_map(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, args[a]); fputs("))", cg->out); }
                }
                /* For map_set / map_has_key / map_delete / map_get: key needs wrapping */
                else if ((strcmp(name, "map_has_key") == 0 || strcmp(name, "map_delete") == 0) && a == 1) {
                    TypeKind k = args[a]->inferred.kind;
                    if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, args[a]); fputc(')', cg->out); }
                    else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, args[a]); fputs("))", cg->out); }
                }
                else {
                    emit_expr(cg, args[a]);
                }
            }
            fputc(')', cg->out);
            return;
        }
    }

    /* Fallback: emit as a user function call */
    emit_safe_name(cg, name);
    fputc('(', cg->out);
    for (size_t a = 0; a < argc; a++) {
        if (a) fputs(", ", cg->out);
        emit_expr(cg, args[a]);
    }
    fputc(')', cg->out);
}

static void emit_expr(CGen *cg, const Expr *e) {
    switch (e->kind) {
        case EX_INT:
            /* If the type checker promoted this int to float, emit as double */
            if (e->inferred.kind == TY_FLOAT) {
                fprintf(cg->out, "%lld.0", (long long)e->as.int_val);
            } else {
                fprintf(cg->out, "INT64_C(%lld)", (long long)e->as.int_val);
            }
            return;
        case EX_FLOAT:
            fprintf(cg->out, "%.17g", e->as.float_val);
            /* Ensure it looks like a float literal in C (not bare integer) */
            { char buf[64]; snprintf(buf, sizeof(buf), "%.17g", e->as.float_val);
              if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')
                  && !strchr(buf, 'n') && !strchr(buf, 'N')) {
                  fputs(".0", cg->out);
              }
            }
            return;
        case EX_BOOL:
            fputs(e->as.bool_val ? "true" : "false", cg->out);
            return;
        case EX_STR:
            fputs("bp_str_from_cstr(", cg->out);
            emit_c_string_literal(cg, e->as.str_val);
            fputc(')', cg->out);
            return;
        case EX_VAR:
            emit_safe_name(cg, e->as.var_name);
            return;
        case EX_CALL:
            if (e->as.call.fn_index <= -100) {
                /* FFI extern function call — emit as dlopen/dlsym dispatch.
                   We generate a self-contained call that resolves the symbol on first use. */
                int ext_idx = -(e->as.call.fn_index + 100);
                const ExternDef *ed = (ext_idx >= 0 && (size_t)ext_idx < cg->mod->externc)
                    ? &cg->mod->externs[ext_idx] : NULL;
                const char *c_name = ed ? ed->c_name : e->as.call.name;

                /* Emit: <c_name>(args...) — rely on system headers or user-provided headers */
                fputs(c_name, cg->out);
                fputc('(', cg->out);
                for (size_t i = 0; i < e->as.call.argc; i++) {
                    if (i) fputs(", ", cg->out);
                    /* For string args, extract ->data for C compatibility */
                    if (ed && i < ed->paramc && ed->params[i].type.kind == TY_STR) {
                        fputc('(', cg->out);
                        emit_expr(cg, e->as.call.args[i]);
                        fputs(")->data", cg->out);
                    } else {
                        emit_expr(cg, e->as.call.args[i]);
                    }
                }
                fputc(')', cg->out);
                return;
            }
            if (e->as.call.fn_index < 0) {
                /* array_pop returns RtVal, needs unwrapping to the inferred type */
                if (strcmp(e->as.call.name, "array_pop") == 0) {
                    fputs("bp_array_pop(", cg->out);
                    if (e->as.call.argc > 0) emit_expr(cg, e->as.call.args[0]);
                    fputc(')', cg->out);
                    TypeKind rk = e->inferred.kind;
                    if (rk == TY_INT || rk == TY_I64) fputs(".as.i", cg->out);
                    else if (rk == TY_FLOAT) fputs(".as.f", cg->out);
                    else if (rk == TY_BOOL) fputs(".as.b", cg->out);
                    else if (rk == TY_STR) fputs(".as.s", cg->out);
                    else if (rk == TY_ARRAY) fputs(".as.arr", cg->out);
                    else if (rk == TY_MAP) fputs(".as.map", cg->out);
                    return;
                }
                emit_builtin_call(cg, e->as.call.name, e->as.call.args, e->as.call.argc);
            } else {
                emit_safe_name(cg, e->as.call.name);
                fputc('(', cg->out);
                for (size_t i = 0; i < e->as.call.argc; i++) {
                    if (i) fputs(", ", cg->out);
                    emit_expr(cg, e->as.call.args[i]);
                }
                fputc(')', cg->out);
            }
            return;
        case EX_UNARY:
            fputc('(', cg->out);
            if (e->as.unary.op == UOP_NEG) fputc('-', cg->out);
            else fputs("!", cg->out);
            emit_expr(cg, e->as.unary.rhs);
            fputc(')', cg->out);
            return;
        case EX_BINARY:
            /* String concatenation */
            if (e->as.binary.op == BOP_ADD && e->inferred.kind == TY_STR) {
                fputs("bp_str_concat(", cg->out);
                emit_expr(cg, e->as.binary.lhs);
                fputs(", ", cg->out);
                emit_expr(cg, e->as.binary.rhs);
                fputc(')', cg->out);
                return;
            }
            /* Comparison of strings by value */
            if ((e->as.binary.op == BOP_EQ || e->as.binary.op == BOP_NEQ)
                && e->as.binary.lhs->inferred.kind == TY_STR) {
                if (e->as.binary.op == BOP_NEQ) fputc('!', cg->out);
                fputs("(strcmp(", cg->out);
                emit_expr(cg, e->as.binary.lhs);
                fputs("->data, ", cg->out);
                emit_expr(cg, e->as.binary.rhs);
                fputs("->data) == 0)", cg->out);
                return;
            }
            {
            /* Determine if we need float casts on operands */
            bool need_float_cast = (e->inferred.kind == TY_FLOAT);
            /* For division, always use float if either operand is int
               to avoid integer div-by-zero crashes in C */
            if (e->as.binary.op == BOP_DIV &&
                (e->as.binary.lhs->inferred.kind == TY_INT ||
                 e->as.binary.rhs->inferred.kind == TY_INT)) {
                need_float_cast = true;
            }
            fputc('(', cg->out);
            if (need_float_cast && e->as.binary.lhs->inferred.kind != TY_FLOAT) {
                fputs("(double)", cg->out);
            }
            emit_expr(cg, e->as.binary.lhs);
            fprintf(cg->out, " %s ", binop_str(e->as.binary.op));
            if (need_float_cast && e->as.binary.rhs->inferred.kind != TY_FLOAT) {
                fputs("(double)", cg->out);
            }
            emit_expr(cg, e->as.binary.rhs);
            fputc(')', cg->out);
            return;
            }
        case EX_ARRAY: {
            /* Create and populate array */
            fputs("({BpRtArray *_a=bp_array_new(", cg->out);
            fprintf(cg->out, "%zu); ", e->as.array.len);
            for (size_t i = 0; i < e->as.array.len; i++) {
                fputs("bp_array_push(_a, ", cg->out);
                TypeKind k = e->as.array.elements[i]->inferred.kind;
                if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputc(')', cg->out); }
                else if (k == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputc(')', cg->out); }
                else if (k == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputc(')', cg->out); }
                else if (k == TY_ARRAY) { fputs("rv_arr(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputc(')', cg->out); }
                else if (k == TY_MAP) { fputs("rv_map(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, e->as.array.elements[i]); fputs("))", cg->out); }
                fputs("); ", cg->out);
            }
            fputs("_a;})", cg->out);
            return;
        }
        case EX_INDEX: {
            if (e->as.index.array->inferred.kind == TY_MAP) {
                /* Map access: need to unwrap from RtVal */
                TypeKind rk = e->inferred.kind;
                fputs("bp_map_get(", cg->out);
                emit_expr(cg, e->as.index.array);
                fputs(", ", cg->out);
                /* Wrap key in RtVal */
                TypeKind kk = e->as.index.index->inferred.kind;
                if (kk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, e->as.index.index); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, e->as.index.index); fputs("))", cg->out); }
                fputc(')', cg->out);
                /* Unwrap result */
                if (rk == TY_INT || rk == TY_I64) fputs(".as.i", cg->out);
                else if (rk == TY_FLOAT) fputs(".as.f", cg->out);
                else if (rk == TY_BOOL) fputs(".as.b", cg->out);
                else if (rk == TY_STR) fputs(".as.s", cg->out);
                else if (rk == TY_ARRAY) fputs(".as.arr", cg->out);
                else if (rk == TY_MAP) fputs(".as.map", cg->out);
            } else {
                /* Array access: unwrap from RtVal */
                TypeKind rk = e->inferred.kind;
                fputs("bp_array_get(", cg->out);
                emit_expr(cg, e->as.index.array);
                fputs(", ", cg->out);
                emit_expr(cg, e->as.index.index);
                fputc(')', cg->out);
                if (rk == TY_INT || rk == TY_I64) fputs(".as.i", cg->out);
                else if (rk == TY_FLOAT) fputs(".as.f", cg->out);
                else if (rk == TY_BOOL) fputs(".as.b", cg->out);
                else if (rk == TY_STR) fputs(".as.s", cg->out);
                else if (rk == TY_ARRAY) fputs(".as.arr", cg->out);
                else if (rk == TY_MAP) fputs(".as.map", cg->out);
            }
            return;
        }
        case EX_MAP: {
            fputs("({BpRtMap *_m=bp_map_new(", cg->out);
            fprintf(cg->out, "%zu); ", e->as.map.len * 2);
            for (size_t i = 0; i < e->as.map.len; i++) {
                fputs("bp_map_set(_m, ", cg->out);
                /* Wrap key */
                TypeKind kk = e->as.map.keys[i]->inferred.kind;
                if (kk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, e->as.map.keys[i]); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, e->as.map.keys[i]); fputs("))", cg->out); }
                fputs(", ", cg->out);
                /* Wrap value */
                TypeKind vk = e->as.map.values[i]->inferred.kind;
                if (vk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, e->as.map.values[i]); fputc(')', cg->out); }
                else if (vk == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, e->as.map.values[i]); fputc(')', cg->out); }
                else if (vk == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, e->as.map.values[i]); fputc(')', cg->out); }
                else if (vk == TY_ARRAY) { fputs("rv_arr(", cg->out); emit_expr(cg, e->as.map.values[i]); fputc(')', cg->out); }
                else if (vk == TY_MAP) { fputs("rv_map(", cg->out); emit_expr(cg, e->as.map.values[i]); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, e->as.map.values[i]); fputs("))", cg->out); }
                fputs("); ", cg->out);
            }
            fputs("_m;})", cg->out);
            return;
        }
        case EX_STRUCT_LITERAL: {
            /* Emit as C struct literal */
            fprintf(cg->out, "((BpStruct_%s){", e->as.struct_literal.struct_name);
            for (size_t i = 0; i < e->as.struct_literal.field_count; i++) {
                if (i) fputs(", ", cg->out);
                fprintf(cg->out, ".%s = ", e->as.struct_literal.field_names[i]);
                emit_expr(cg, e->as.struct_literal.field_values[i]);
            }
            fputs("})", cg->out);
            return;
        }
        case EX_FIELD_ACCESS:
            emit_expr(cg, e->as.field_access.object);
            fprintf(cg->out, ".%s", e->as.field_access.field_name);
            return;
        case EX_ENUM_MEMBER:
            fprintf(cg->out, "BP_ENUM_%s_%s", e->as.enum_member.enum_name, e->as.enum_member.member_name);
            return;
        case EX_FSTRING:
            /* Simple: treat as string for now */
            fputs("bp_str_from_cstr(", cg->out);
            emit_c_string_literal(cg, e->as.fstring.template_str);
            fputc(')', cg->out);
            return;
        case EX_METHOD_CALL:
            /* Emit as: structname_methodname(obj, args...) */
            emit_safe_name(cg, e->as.method_call.method_name);
            fputc('(', cg->out);
            emit_expr(cg, e->as.method_call.object);
            for (size_t i = 0; i < e->as.method_call.argc; i++) {
                fputs(", ", cg->out);
                emit_expr(cg, e->as.method_call.args[i]);
            }
            fputc(')', cg->out);
            return;
        case EX_TUPLE:
            /* Emit first element for now (tuples not fully supported in C target) */
            if (e->as.tuple.len > 0) emit_expr(cg, e->as.tuple.elements[0]);
            else fputs("0", cg->out);
            return;
        case EX_LAMBDA:
            /* Lambda: emit as 0 (not supported in C target) */
            fputs("((void*)0) /* lambda unsupported */", cg->out);
            return;
        case EX_NEW:
            /* Class instantiation: not yet supported */
            fputs("rv_null() /* class new unsupported */", cg->out);
            return;
        case EX_SUPER_CALL:
            fputs("rv_null() /* super unsupported */", cg->out);
            return;
    }
}

/* ── Statement emission ───────────────────────────────── */

static void emit_stmt(CGen *cg, const Stmt *s);

static void emit_block(CGen *cg, Stmt **stmts, size_t len) {
    for (size_t i = 0; i < len; i++) {
        emit_stmt(cg, stmts[i]);
    }
}

static void emit_stmt(CGen *cg, const Stmt *s) {
    switch (s->kind) {
        case ST_LET: {
            cg_indent(cg);
            emit_type(cg, &s->as.let.type);
            fputc(' ', cg->out);
            emit_safe_name(cg, s->as.let.name);
            fputs(" = ", cg->out);
            /* If the declared type is float but the init isn't, force float context */
            if (s->as.let.type.kind == TY_FLOAT && s->as.let.init->inferred.kind != TY_FLOAT) {
                emit_expr_as_float(cg, s->as.let.init);
            } else {
                emit_expr(cg, s->as.let.init);
            }
            fputs(";\n", cg->out);
            return;
        }
        case ST_ASSIGN: {
            cg_indent(cg);
            emit_safe_name(cg, s->as.assign.name);
            fputs(" = ", cg->out);
            emit_expr(cg, s->as.assign.value);
            fputs(";\n", cg->out);
            return;
        }
        case ST_INDEX_ASSIGN: {
            cg_indent(cg);
            if (s->as.idx_assign.array->inferred.kind == TY_MAP) {
                fputs("bp_map_set(", cg->out);
                emit_expr(cg, s->as.idx_assign.array);
                fputs(", ", cg->out);
                /* Wrap key */
                TypeKind kk = s->as.idx_assign.index->inferred.kind;
                if (kk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, s->as.idx_assign.index); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, s->as.idx_assign.index); fputs("))", cg->out); }
                fputs(", ", cg->out);
                /* Wrap value */
                TypeKind vk = s->as.idx_assign.value->inferred.kind;
                if (vk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else if (vk == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else if (vk == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputs("))", cg->out); }
                fputs(");\n", cg->out);
            } else {
                fputs("bp_array_set(", cg->out);
                emit_expr(cg, s->as.idx_assign.array);
                fputs(", ", cg->out);
                emit_expr(cg, s->as.idx_assign.index);
                fputs(", ", cg->out);
                /* Wrap value */
                TypeKind vk = s->as.idx_assign.value->inferred.kind;
                if (vk == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else if (vk == TY_FLOAT) { fputs("rv_float(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else if (vk == TY_BOOL) { fputs("rv_bool(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputc(')', cg->out); }
                else { fputs("rv_int((int64_t)(", cg->out); emit_expr(cg, s->as.idx_assign.value); fputs("))", cg->out); }
                fputs(");\n", cg->out);
            }
            return;
        }
        case ST_EXPR: {
            cg_indent(cg);
            emit_expr(cg, s->as.expr.expr);
            fputs(";\n", cg->out);
            return;
        }
        case ST_RETURN: {
            cg_indent(cg);
            if (s->as.ret.value) {
                fputs("return ", cg->out);
                emit_expr(cg, s->as.ret.value);
                fputs(";\n", cg->out);
            } else {
                fputs("return;\n", cg->out);
            }
            return;
        }
        case ST_IF: {
            cg_indent(cg);
            fputs("if (", cg->out);
            emit_expr(cg, s->as.ifs.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_block(cg, s->as.ifs.then_stmts, s->as.ifs.then_len);
            cg->indent--;
            if (s->as.ifs.else_len > 0) {
                cg_indent(cg);
                /* Check if the else branch is a single if statement (elif) */
                if (s->as.ifs.else_len == 1 && s->as.ifs.else_stmts[0]->kind == ST_IF) {
                    fputs("} else ", cg->out);
                    /* Don't indent the chained if */
                    int saved = cg->indent;
                    cg->indent = 0;
                    emit_stmt(cg, s->as.ifs.else_stmts[0]);
                    cg->indent = saved;
                } else {
                    fputs("} else {\n", cg->out);
                    cg->indent++;
                    emit_block(cg, s->as.ifs.else_stmts, s->as.ifs.else_len);
                    cg->indent--;
                    cg_indent(cg);
                    fputs("}\n", cg->out);
                }
            } else {
                cg_indent(cg);
                fputs("}\n", cg->out);
            }
            return;
        }
        case ST_WHILE: {
            int lbl = cg_fresh_label(cg);
            push_loop_label(cg, lbl);
            cg_indent(cg);
            fputs("while (", cg->out);
            emit_expr(cg, s->as.wh.cond);
            fputs(") {\n", cg->out);
            cg->indent++;
            emit_block(cg, s->as.wh.body, s->as.wh.body_len);
            cg->indent--;
            cg_indent(cg);
            fputs("}\n", cg->out);
            pop_loop_label(cg);
            return;
        }
        case ST_FOR: {
            int lbl = cg_fresh_label(cg);
            push_loop_label(cg, lbl);
            cg_indent(cg);
            fputs("for (int64_t ", cg->out);
            emit_safe_name(cg, s->as.forr.var);
            fputs(" = ", cg->out);
            emit_expr(cg, s->as.forr.start);
            fputs("; ", cg->out);
            emit_safe_name(cg, s->as.forr.var);
            fputs(" < ", cg->out);
            emit_expr(cg, s->as.forr.end);
            fputs("; ", cg->out);
            emit_safe_name(cg, s->as.forr.var);
            fputs("++) {\n", cg->out);
            cg->indent++;
            emit_block(cg, s->as.forr.body, s->as.forr.body_len);
            cg->indent--;
            cg_indent(cg);
            fputs("}\n", cg->out);
            pop_loop_label(cg);
            return;
        }
        case ST_BREAK:
            cg_indent(cg);
            fputs("break;\n", cg->out);
            return;
        case ST_CONTINUE:
            cg_indent(cg);
            fputs("continue;\n", cg->out);
            return;
        case ST_TRY: {
            cg_indent(cg);
            fputs("BP_TRY {\n", cg->out);
            cg->indent++;
            emit_block(cg, s->as.try_catch.try_stmts, s->as.try_catch.try_len);
            cg->indent--;
            cg_indent(cg);
            if (s->as.try_catch.catch_var) {
                fprintf(cg->out, "} BP_CATCH(_exc_%s) {\n", s->as.try_catch.catch_var);
                cg->indent++;
                /* Extract the string from the RtVal for use as BpRtStr* */
                cg_indent(cg);
                fprintf(cg->out, "BpRtStr *%s = (_exc_%s.type == RT_STR) ? _exc_%s.as.s : bp_to_str(_exc_%s);\n",
                        s->as.try_catch.catch_var, s->as.try_catch.catch_var,
                        s->as.try_catch.catch_var, s->as.try_catch.catch_var);
                cg->indent--;
            } else {
                fputs("} BP_CATCH(_exc_unused) {\n", cg->out);
            }
            cg->indent++;
            emit_block(cg, s->as.try_catch.catch_stmts, s->as.try_catch.catch_len);
            cg->indent--;
            cg_indent(cg);
            fputs("} BP_END_TRY;\n", cg->out);
            /* Finally block (runs unconditionally after try/catch) */
            if (s->as.try_catch.finally_len > 0) {
                emit_block(cg, s->as.try_catch.finally_stmts, s->as.try_catch.finally_len);
            }
            return;
        }
        case ST_THROW: {
            cg_indent(cg);
            fputs("bp_throw(", cg->out);
            TypeKind k = s->as.throw.value->inferred.kind;
            if (k == TY_STR) { fputs("rv_str(", cg->out); emit_expr(cg, s->as.throw.value); fputc(')', cg->out); }
            else if (k == TY_INT) { fputs("rv_int(", cg->out); emit_expr(cg, s->as.throw.value); fputc(')', cg->out); }
            else { fputs("rv_str(", cg->out); emit_expr(cg, s->as.throw.value); fputc(')', cg->out); }
            fputs(");\n", cg->out);
            return;
        }
    }
}

/* ── Top-level: structs, enums, functions ────────────── */

static void emit_struct_def(CGen *cg, const StructDef *sd) {
    fprintf(cg->out, "typedef struct {\n");
    for (size_t i = 0; i < sd->field_count; i++) {
        fputs("    ", cg->out);
        emit_type(cg, &sd->field_types[i]);
        fprintf(cg->out, " %s;\n", sd->field_names[i]);
    }
    fprintf(cg->out, "} BpStruct_%s;\n\n", sd->name);
}

static void emit_enum_def(CGen *cg, const EnumDef *ed) {
    for (size_t i = 0; i < ed->variant_count; i++) {
        fprintf(cg->out, "#define BP_ENUM_%s_%s INT64_C(%lld)\n",
                ed->name, ed->variant_names[i], (long long)ed->variant_values[i]);
    }
    fputc('\n', cg->out);
}

static void emit_function_forward(CGen *cg, const Function *f) {
    if (f->ret_type.kind == TY_VOID) fputs("void", cg->out);
    else emit_type(cg, &f->ret_type);
    fputc(' ', cg->out);
    emit_safe_name(cg, f->name);
    fputc('(', cg->out);
    if (f->paramc == 0) {
        fputs("void", cg->out);
    } else {
        for (size_t i = 0; i < f->paramc; i++) {
            if (i) fputs(", ", cg->out);
            emit_type(cg, &f->params[i].type);
            fputc(' ', cg->out);
            emit_safe_name(cg, f->params[i].name);
        }
    }
    fputs(");\n", cg->out);
}

static void emit_function(CGen *cg, const Function *f) {
    if (f->ret_type.kind == TY_VOID) fputs("void", cg->out);
    else emit_type(cg, &f->ret_type);
    fputc(' ', cg->out);
    emit_safe_name(cg, f->name);
    fputc('(', cg->out);
    if (f->paramc == 0) {
        fputs("void", cg->out);
    } else {
        for (size_t i = 0; i < f->paramc; i++) {
            if (i) fputs(", ", cg->out);
            emit_type(cg, &f->params[i].type);
            fputc(' ', cg->out);
            emit_safe_name(cg, f->params[i].name);
        }
    }
    fputs(") {\n", cg->out);
    cg->indent = 1;
    emit_block(cg, f->body, f->body_len);
    cg->indent = 0;
    fputs("}\n\n", cg->out);
}

/* ── Main entry ──────────────────────────────────────── */

bool transpile_module_to_c(const Module *m, const char *output_path) {
    FILE *out = fopen(output_path, "w");
    if (!out) return false;

    CGen cg;
    memset(&cg, 0, sizeof(cg));
    cg.out = out;
    cg.mod = m;

    /* ── Header ──────────────────────────────────── */
    fputs("/* Generated by BetterPython C Transpiler */\n", out);
    fputs("#include \"bp_runtime.h\"\n", out);
    fputs("#include <dlfcn.h>\n\n", out);

    /* ── Extern (FFI) declarations ──────────────── */
    for (size_t i = 0; i < m->externc; i++) {
        const ExternDef *ed = &m->externs[i];
        fputs("/* extern: ", out);
        fputs(ed->name, out);
        fprintf(out, " from \"%s\"", ed->library);
        if (ed->c_name && strcmp(ed->c_name, ed->name) != 0)
            fprintf(out, " as \"%s\"", ed->c_name);
        fputs(" */\n", out);
    }
    if (m->externc > 0) fputc('\n', out);

    /* ── Struct definitions ──────────────────────── */
    for (size_t i = 0; i < m->structc; i++) {
        emit_struct_def(&cg, &m->structs[i]);
    }

    /* ── Enum definitions ────────────────────────── */
    for (size_t i = 0; i < m->enumc; i++) {
        emit_enum_def(&cg, &m->enums[i]);
    }

    /* ── Forward declarations ────────────────────── */
    for (size_t i = 0; i < m->fnc; i++) {
        emit_function_forward(&cg, &m->fns[i]);
    }
    fputc('\n', out);

    /* ── Function definitions ────────────────────── */
    for (size_t i = 0; i < m->fnc; i++) {
        emit_function(&cg, &m->fns[i]);
    }

    /* ── main() entry point ──────────────────────── */
    fputs("int main(int argc, char **argv) {\n", out);
    fputs("    bp_runtime_init(argc, argv);\n", out);
    fputs("    int64_t ret = bp_main();\n", out);
    fputs("    bp_runtime_cleanup();\n", out);
    fputs("    return (int)ret;\n", out);
    fputs("}\n", out);

    fclose(out);
    return true;
}
