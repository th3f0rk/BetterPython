/**
 * External scanner for indentation-based syntax
 * Handles INDENT, DEDENT, and NEWLINE tokens
 */

#include <tree_sitter/parser.h>
#include <wctype.h>
#include <string.h>
#include <stdlib.h>

enum TokenType {
  INDENT,
  DEDENT,
  NEWLINE,
};

typedef struct {
  int *indent_stack;
  size_t indent_stack_cap;
  size_t indent_stack_size;
} Scanner;

void *tree_sitter_betterpython_external_scanner_create() {
  Scanner *scanner = (Scanner *)calloc(1, sizeof(Scanner));
  scanner->indent_stack_cap = 16;
  scanner->indent_stack = (int *)calloc(scanner->indent_stack_cap, sizeof(int));
  scanner->indent_stack[0] = 0;
  scanner->indent_stack_size = 1;
  return scanner;
}

void tree_sitter_betterpython_external_scanner_destroy(void *payload) {
  Scanner *scanner = (Scanner *)payload;
  free(scanner->indent_stack);
  free(scanner);
}

unsigned tree_sitter_betterpython_external_scanner_serialize(
  void *payload,
  char *buffer
) {
  Scanner *scanner = (Scanner *)payload;
  size_t size = 0;
  
  if (scanner->indent_stack_size > 256) {
    return 0;
  }
  
  memcpy(buffer, &scanner->indent_stack_size, sizeof(size_t));
  size += sizeof(size_t);
  
  memcpy(buffer + size, scanner->indent_stack, 
         scanner->indent_stack_size * sizeof(int));
  size += scanner->indent_stack_size * sizeof(int);
  
  return size;
}

void tree_sitter_betterpython_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
  Scanner *scanner = (Scanner *)payload;
  
  if (length == 0) {
    scanner->indent_stack_size = 1;
    scanner->indent_stack[0] = 0;
    return;
  }
  
  size_t size = 0;
  memcpy(&scanner->indent_stack_size, buffer, sizeof(size_t));
  size += sizeof(size_t);
  
  if (scanner->indent_stack_size > scanner->indent_stack_cap) {
    scanner->indent_stack_cap = scanner->indent_stack_size;
    scanner->indent_stack = (int *)realloc(
      scanner->indent_stack,
      scanner->indent_stack_cap * sizeof(int)
    );
  }
  
  memcpy(scanner->indent_stack, buffer + size,
         scanner->indent_stack_size * sizeof(int));
}

static void push_indent(Scanner *scanner, int indent) {
  if (scanner->indent_stack_size >= scanner->indent_stack_cap) {
    scanner->indent_stack_cap *= 2;
    scanner->indent_stack = (int *)realloc(
      scanner->indent_stack,
      scanner->indent_stack_cap * sizeof(int)
    );
  }
  scanner->indent_stack[scanner->indent_stack_size++] = indent;
}

static int pop_indent(Scanner *scanner) {
  if (scanner->indent_stack_size > 1) {
    return scanner->indent_stack[--scanner->indent_stack_size];
  }
  return 0;
}

static int current_indent(Scanner *scanner) {
  return scanner->indent_stack[scanner->indent_stack_size - 1];
}

bool tree_sitter_betterpython_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  Scanner *scanner = (Scanner *)payload;
  
  bool has_newline = false;
  int indent_level = 0;
  
  while (true) {
    if (lexer->lookahead == ' ') {
      indent_level++;
      lexer->advance(lexer, true);
    } else if (lexer->lookahead == '\t') {
      indent_level += 4;
      lexer->advance(lexer, true);
    } else if (lexer->lookahead == '\r') {
      lexer->advance(lexer, true);
    } else if (lexer->lookahead == '\n') {
      has_newline = true;
      indent_level = 0;
      lexer->advance(lexer, true);
    } else if (lexer->lookahead == '#') {
      while (lexer->lookahead != '\n' && lexer->lookahead != 0) {
        lexer->advance(lexer, true);
      }
      if (lexer->lookahead == '\n') {
        has_newline = true;
        indent_level = 0;
        lexer->advance(lexer, true);
      }
    } else {
      break;
    }
  }
  
  if (lexer->lookahead == 0) {
    if (valid_symbols[DEDENT] && scanner->indent_stack_size > 1) {
      pop_indent(scanner);
      lexer->result_symbol = DEDENT;
      return true;
    }
    return false;
  }
  
  if (has_newline) {
    int current = current_indent(scanner);
    
    if (indent_level > current && valid_symbols[INDENT]) {
      push_indent(scanner, indent_level);
      lexer->result_symbol = INDENT;
      return true;
    }
    
    if (indent_level < current && valid_symbols[DEDENT]) {
      pop_indent(scanner);
      lexer->result_symbol = DEDENT;
      return true;
    }
    
    if (valid_symbols[NEWLINE]) {
      lexer->result_symbol = NEWLINE;
      return true;
    }
  }
  
  return false;
}
