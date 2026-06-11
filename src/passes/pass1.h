#ifndef PASS1_H
#define PASS1_H

#include "scope.h"
#include <stdint.h>

typedef struct compiler_context_s compiler_context_t;
typedef struct ast_node_s ast_node_t;

typedef struct symbol_table_s {
    scope_t* global;
    scope_t* current;
    int32_t nextOffset; // starts at -wordsize
} symbol_table_t;

// Symbol table pass: Collects globals, assigns stack offsets
symbol_table_t* pass1(ast_node_t* root, compiler_context_t* ctx);

void freeSymbolTable(symbol_table_t* symbols);

#endif