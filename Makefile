PREFIX ?= /usr/local
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
LDFLAGS ?=

SRC := \
    src/bp_main.c \
    src/lexer.c \
    src/parser.c \
    src/ast.c \
    src/types.c \
    src/bytecode.c \
    src/compiler.c \
    src/regalloc.c \
    src/reg_compiler.c \
    src/vm.c \
    src/reg_vm.c \
    src/gc.c \
    src/stdlib.c \
    src/security.c \
    src/util.c \
    src/thread.c \
    src/module_resolver.c \
    src/multi_compile.c \
    src/jit/jit_profile.c \
    src/jit/jit_x64.c \
    src/jit/jit_compile.c

OBJ := $(SRC:.c=.o)

BIN_DIR := bin
BPCC := $(BIN_DIR)/bpcc
BPVM := $(BIN_DIR)/bpvm
BPREPL := $(BIN_DIR)/bprepl
BPRUN := $(BIN_DIR)/betterpython
BPPKG := $(BIN_DIR)/bppkg
BPLSP := $(BIN_DIR)/bplsp
BPLINT := $(BIN_DIR)/bplint
BPFMT := $(BIN_DIR)/bpfmt

all: $(BPCC) $(BPVM) $(BPREPL) $(BPRUN) $(BPPKG) $(BPLSP) $(BPLINT) $(BPFMT)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BPCC): $(BIN_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) -lm -lpthread

$(BPVM): $(BIN_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) -lm -lpthread

$(BPREPL): $(BIN_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) -lm -lpthread

$(BPRUN): $(BIN_DIR) tools/betterpython.sh
	cp tools/betterpython.sh $@
	chmod +x $@

$(BPPKG): $(BIN_DIR) src/bppkg.c
	$(CC) $(CFLAGS) src/bppkg.c -o $@ $(LDFLAGS)

$(BPLSP): $(BIN_DIR) src/bplsp.c
	$(CC) $(CFLAGS) src/bplsp.c -o $@ $(LDFLAGS)

$(BPLINT): $(BIN_DIR) src/bplint.c
	$(CC) $(CFLAGS) src/bplint.c -o $@ $(LDFLAGS)

$(BPFMT): $(BIN_DIR) src/bpfmt.c
	$(CC) $(CFLAGS) src/bpfmt.c -o $@ $(LDFLAGS)

install: all
	install -d $(PREFIX)/bin
	install -m 0755 $(BPCC) $(PREFIX)/bin/bpcc
	install -m 0755 $(BPVM) $(PREFIX)/bin/bpvm
	install -m 0755 $(BPREPL) $(PREFIX)/bin/bprepl
	install -m 0755 $(BPRUN) $(PREFIX)/bin/betterpython
	install -m 0755 $(BPPKG) $(PREFIX)/bin/bppkg
	install -m 0755 $(BPLSP) $(PREFIX)/bin/bplsp
	install -m 0755 $(BPLINT) $(PREFIX)/bin/bplint
	install -m 0755 $(BPFMT) $(PREFIX)/bin/bpfmt

clean:
	rm -f $(OBJ)
	rm -rf $(BIN_DIR)

.PHONY: all clean install
