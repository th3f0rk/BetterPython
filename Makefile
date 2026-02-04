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
    src/vm.c \
    src/gc.c \
    src/stdlib.c \
    src/security.c \
    src/util.c

OBJ := $(SRC:.c=.o)

BIN_DIR := bin
BPCC := $(BIN_DIR)/bpcc
BPVM := $(BIN_DIR)/bpvm
BPRUN := $(BIN_DIR)/betterpython

all: $(BPCC) $(BPVM) $(BPRUN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BPCC): $(BIN_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) -lm

$(BPVM): $(BIN_DIR) $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS) -lm

$(BPRUN): $(BIN_DIR) tools/betterpython.sh
	cp tools/betterpython.sh $@
	chmod +x $@

install: all
	install -d $(PREFIX)/bin
	install -m 0755 $(BPCC) $(PREFIX)/bin/bpcc
	install -m 0755 $(BPVM) $(PREFIX)/bin/bpvm
	install -m 0755 $(BPRUN) $(PREFIX)/bin/betterpython

clean:
	rm -f $(OBJ)
	rm -rf $(BIN_DIR)

.PHONY: all clean install
