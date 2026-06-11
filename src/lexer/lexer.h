#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <stdbool.h>

typedef struct lexer_s {
    char const* src;
    size_t pos;
    size_t len;
    int32_t line;
    int32_t col;
    bool pendingAsm; // Set after emitting 'asm' keyword
} lexer_t;

void initLexer(lexer_t* p, char const* const src);
void freeLexer(lexer_t* p);

int32_t lexerLine(lexer_t const* p);
int32_t lexerCol(lexer_t const* p);

// Consume + return next token. Caller must call freeToken() on the result.
token_t lexerNext(lexer_t* p);
// Return next token without consuming. Caller must call freeToken() on the result.
token_t lexerPeek(lexer_t* p);

#endif