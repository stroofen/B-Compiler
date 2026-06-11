#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

typedef enum {
    TOK_Eof,
    TOK_Identifier,
    TOK_Number,
    TOK_String,
    TOK_Keyword,    // return, auto, if, while, extrn, asm
    TOK_ParenthesisOpen,
    TOK_ParenthesisClose,
    TOK_SquareBracketOpen,
    TOK_SquareBracketClose,
    TOK_CurlyBracketOpen,
    TOK_CurlyBracketClose,
    TOK_Comma,
    TOK_Semicolon,
    TOK_AsmBlock,   // Assembly code block
    TOK_Operator    // = + - * / % & | ^ ! ~ < > . == != <= >= << >> && || ++ --
} token_type_t;

typedef struct token_s {
    token_type_t type;
    char* text;
    int64_t number; // only for numbers
} token_t;

void initToken(token_t* p, token_type_t type, char const* const text);
void initTokenNumeric(token_t* p, token_type_t type, char const* const text);
void freeToken(token_t* p);

char const* const getTokenName(token_type_t type);

#endif