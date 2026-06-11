#ifndef PASS3_H
#define PASS3_H

#include "scope.h"
#include <stdio.h>

typedef struct compiler_context_s compiler_context_t;
typedef struct ast_node_s ast_node_t;
typedef struct ir_inst_s ir_inst_t;
typedef struct symbol_table_s symbol_table_t;

// Codegen pass: Uses IR to generate asm
void pass3(ir_inst_t const* instrs, uint32_t const numInstrs, symbol_table_t const* symbols, compiler_context_t* ctx, FILE* out);

#endif