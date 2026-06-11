#ifndef PARSER_H
#define PARSER_H

#include "../ast/ast_node.h"
#include "../lexer/lexer.h"

typedef struct compiler_context_s compiler_context_t;

typedef struct parser_s {
    lexer_t* lexer;
    token_t current;
    token_t peek;
    bool hasPeek;
    compiler_context_t* ctx;
} parser_t;

void initParser(parser_t* p, lexer_t* lexer, compiler_context_t* ctx);
void freeParser(parser_t* p);

// Parse full program
ast_node_t* parse(parser_t* p);

#endif