#include "bytecode.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

static void w_u32(FILE *f, uint32_t x) { fwrite(&x, 4, 1, f); }
static void w_u16(FILE *f, uint16_t x) { fwrite(&x, 2, 1, f); }

static uint32_t r_u32(FILE *f) { uint32_t x; if (fread(&x, 4, 1, f) != 1) bp_fatal("read"); return x; }
static uint16_t r_u16(FILE *f) { uint16_t x; if (fread(&x, 2, 1, f) != 1) bp_fatal("read"); return x; }

static void w_bytes(FILE *f, const void *p, size_t n) {
    if (n && fwrite(p, 1, n, f) != n) bp_fatal("write");
}

static void r_bytes(FILE *f, void *p, size_t n) {
    if (n && fread(p, 1, n, f) != n) bp_fatal("read");
}

void bc_module_free(BpModule *m) {
    for (size_t i = 0; i < m->str_len; i++) free(m->strings[i]);
    free(m->strings);
    for (size_t i = 0; i < m->fn_len; i++) {
        free(m->funcs[i].name);
        free(m->funcs[i].code);
        free(m->funcs[i].str_const_ids);
    }
    free(m->funcs);
    memset(m, 0, sizeof(*m));
}

int bc_write_file(const char *path, const BpModule *m) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    // magic "BPC0" + version 1
    w_bytes(f, "BPC0", 4);
    w_u32(f, 1);

    w_u32(f, (uint32_t)m->entry);

    w_u32(f, (uint32_t)m->str_len);
    for (size_t i = 0; i < m->str_len; i++) {
        uint32_t n = (uint32_t)strlen(m->strings[i]);
        w_u32(f, n);
        w_bytes(f, m->strings[i], n);
    }

    w_u32(f, (uint32_t)m->fn_len);
    for (size_t i = 0; i < m->fn_len; i++) {
        BpFunc *fn = &m->funcs[i];
        uint32_t n = (uint32_t)strlen(fn->name);
        w_u32(f, n);
        w_bytes(f, fn->name, n);

        w_u16(f, fn->arity);
        w_u16(f, fn->locals);

        w_u32(f, (uint32_t)fn->str_const_len);
        for (size_t k = 0; k < fn->str_const_len; k++) w_u32(f, fn->str_const_ids[k]);

        w_u32(f, (uint32_t)fn->code_len);
        w_bytes(f, fn->code, fn->code_len);
    }

    fclose(f);
    return 1;
}

BpModule bc_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) bp_fatal("cannot open %s", path);

    char magic[4];
    r_bytes(f, magic, 4);
    if (memcmp(magic, "BPC0", 4) != 0) bp_fatal("bad .bpc file");

    uint32_t ver = r_u32(f);
    if (ver != 1) bp_fatal("unsupported .bpc version");

    BpModule m;
    memset(&m, 0, sizeof(m));
    m.entry = r_u32(f);

    m.str_len = r_u32(f);
    m.strings = bp_xmalloc(m.str_len * sizeof(*m.strings));
    for (size_t i = 0; i < m.str_len; i++) {
        uint32_t n = r_u32(f);
        char *s = bp_xmalloc(n + 1);
        r_bytes(f, s, n);
        s[n] = '\0';
        m.strings[i] = s;
    }

    m.fn_len = r_u32(f);
    m.funcs = bp_xmalloc(m.fn_len * sizeof(*m.funcs));
    memset(m.funcs, 0, m.fn_len * sizeof(*m.funcs));

    for (size_t i = 0; i < m.fn_len; i++) {
        uint32_t n = r_u32(f);
        char *name = bp_xmalloc(n + 1);
        r_bytes(f, name, n);
        name[n] = '\0';

        m.funcs[i].name = name;
        m.funcs[i].arity = r_u16(f);
        m.funcs[i].locals = r_u16(f);

        m.funcs[i].str_const_len = r_u32(f);
        if (m.funcs[i].str_const_len) {
            m.funcs[i].str_const_ids = bp_xmalloc(m.funcs[i].str_const_len * sizeof(uint32_t));
            for (size_t k = 0; k < m.funcs[i].str_const_len; k++) m.funcs[i].str_const_ids[k] = r_u32(f);
        }

        m.funcs[i].code_len = r_u32(f);
        m.funcs[i].code = bp_xmalloc(m.funcs[i].code_len);
        r_bytes(f, m.funcs[i].code, m.funcs[i].code_len);
    }

    fclose(f);
    return m;
}
