#include "parser.h"
#include "types.h"
#include "compiler.h"
#include "bytecode.h"
#include "vm.h"
#include "util.h"
#include <string.h>

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
    if (argc < 3) bp_fatal("usage: bpcc <input.bp> -o <output.bpc>");
    const char *in = argv[1];
    const char *out = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out = argv[i + 1];
    }
    if (!out) bp_fatal("missing -o <output.bpc>");

    char *src = read_file_text(in);
    Module m = parse_module(src);
    typecheck_module(&m);
    BpModule bc = compile_module(&m);

    if (!bc_write_file(out, &bc)) bp_fatal("failed to write %s", out);

    bc_module_free(&bc);
    module_free(&m);
    free(src);
    return 0;
}

static int cmd_bpvm(int argc, char **argv) {
    if (argc < 2) bp_fatal("usage: bpvm <file.bpc>");
    const char *path = argv[1];
    BpModule m = bc_read_file(path);
    Vm vm;
    vm_init(&vm, m);
    int code = vm_run(&vm);
    vm_free(&vm);
    return code;
}

int main(int argc, char **argv) {
    const char *p = strrchr(argv[0], '/');
    const char *exe = p ? p + 1 : argv[0];

    if (strcmp(exe, "bpcc") == 0) return cmd_bpcc(argc, argv);
    if (strcmp(exe, "bpvm") == 0) return cmd_bpvm(argc, argv);

    bp_fatal("unknown entrypoint '%s' (expected bpcc or bpvm)", exe);
    return 1;
}
