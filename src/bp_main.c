#include "parser.h"
#include "types.h"
#include "compiler.h"
#include "reg_compiler.h"
#include "c_transpiler.h"
#include "bytecode.h"
#include "vm.h"
#include "reg_vm.h"
#include "stdlib.h"
#include "util.h"
#include "module_resolver.h"
#include "multi_compile.h"
#include "jit/jit.h"
#include <string.h>
#include <stdio.h>

static char *read_file_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) bp_fatal("cannot open %s", path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) bp_fatal("cannot stat %s", path);

    char *buf = bp_xmalloc((size_t)sz + 1);
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static int cmd_bpcc(int argc, char **argv) {
    if (argc < 3) bp_fatal("usage: bpcc <input.bp> -o <output> [--transpile-c]");
    const char *in = argv[1];
    const char *out = NULL;
    bool transpile_c = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[i + 1];
        } else if (strcmp(argv[i], "--transpile-c") == 0 || strcmp(argv[i], "-c") == 0) {
            transpile_c = true;
        }
    }
    if (!out) bp_fatal("missing -o <output>");

    // C transpilation path: parse, typecheck, emit C code
    if (transpile_c) {
        char *src = read_file_text(in);
        Module m = parse_module(src);
        typecheck_module(&m);
        if (!transpile_module_to_c(&m, out)) {
            module_free(&m);
            free(src);
            bp_fatal("failed to write C output to %s", out);
        }
        module_free(&m);
        free(src);
        return 0;
    }

    // First, parse to check for imports
    char *src = read_file_text(in);
    Module m = parse_module(src);

    BpModule bc;
    memset(&bc, 0, sizeof(bc));

    if (m.importc > 0) {
        // Multi-module compilation needed
        module_free(&m);
        free(src);

        // Use module resolver to find all dependencies
        ModuleGraph graph;
        module_graph_init(&graph);

        if (!module_graph_resolve_all(&graph, in)) {
            module_graph_free(&graph);
            bp_fatal("failed to resolve module dependencies");
        }

        // Compile all modules together
        if (!multi_compile(&graph, &bc)) {
            module_graph_free(&graph);
            bp_fatal("failed to compile modules");
        }

        module_graph_free(&graph);
    } else {
        // Single-file compilation - always use register-based compiler
        typecheck_module(&m);
        bc = reg_compile_module(&m);
        module_free(&m);
        free(src);
    }

    if (!bc_write_file(out, &bc)) bp_fatal("failed to write %s", out);

    bc_module_free(&bc);
    return 0;
}

static int cmd_bpvm(int argc, char **argv) {
    if (argc < 2) bp_fatal("usage: bpvm <file.bpc> [args...]");
    const char *path = argv[1];

    // Set program arguments (argv[2:] become the program's argv)
    stdlib_set_args(argc - 2, argv + 2);

    BpModule m = bc_read_file(path);
    Vm vm;
    vm_init(&vm, m);

    // Initialize JIT for register-based bytecode
    JitContext jit;
    jit_init(&jit, m.fn_len);
    vm.jit = &jit;

    // Always use register-based VM
    int code = reg_vm_run(&vm);

    // Print JIT stats if any compilations happened
    if (jit.total_compilations > 0) {
        jit_print_stats(&jit);
    }

    // Cleanup
    jit_shutdown(&jit);
    vm_free(&vm);
    return code;
}

static int cmd_repl(void) {
    printf("BetterPython REPL v1.0.0\n");
    printf("Type expressions to evaluate, or 'exit' to quit.\n\n");

    char line[4096];
    while (1) {
        printf(">>> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        // Check for exit command
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }

        // Skip empty lines
        if (strlen(line) == 0) continue;

        // Wrap the expression in a main function
        char src[8192];
        snprintf(src, sizeof(src),
            "def main() -> int:\n"
            "    print(%s)\n"
            "    return 0\n",
            line);

        // Try to compile and run with register-based VM
        Module m = parse_module(src);
        typecheck_module(&m);
        BpModule bc = reg_compile_module(&m);

        Vm vm;
        vm_init(&vm, bc);

        JitContext jit;
        jit_init(&jit, bc.fn_len);
        vm.jit = &jit;

        reg_vm_run(&vm);

        jit_shutdown(&jit);
        vm_free(&vm);

        bc_module_free(&bc);
        module_free(&m);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *p = strrchr(argv[0], '/');
    const char *exe = p ? p + 1 : argv[0];

    if (strcmp(exe, "bpcc") == 0) return cmd_bpcc(argc, argv);
    if (strcmp(exe, "bpvm") == 0) return cmd_bpvm(argc, argv);
    if (strcmp(exe, "bprepl") == 0) return cmd_repl();

    bp_fatal("unknown entrypoint '%s' (expected bpcc, bpvm, or bprepl)", exe);
    return 1;
}
