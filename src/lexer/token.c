#include "token.h"
#include <string.h>
#include <stdlib.h>

static char const* const gTokenNames[] = {
    [TOK_Eof]                   = "EOF",
    [TOK_Identifier]            = "Identifier",
    [TOK_Number]                = "Number",
    [TOK_String]                = "String",
    [TOK_Keyword]               = "Keyword",
    [TOK_ParenthesisOpen]       = "(",
    [TOK_ParenthesisClose]      = ")",
    [TOK_SquareBracketOpen]     = "[",
    [TOK_SquareBracketClose]    = "]",
    [TOK_CurlyBracketOpen]      = "{",
    [TOK_CurlyBracketClose]     = "}",
    [TOK_Comma]                 = ",",
    [TOK_Semicolon]             = ";",
    [TOK_AsmBlock]              = "AsmBlock",
    [TOK_Operator]              = "Operator"
};

void initToken(token_t* p, token_type_t type, char const* const text) {
    memset(p, 0, sizeof(*p));
    p->type = type;
    p->text = strdup(text ? text : "");
    p->number = 0;
}

void initTokenNumeric(token_t* p, token_type_t type, char const* const text) {
    memset(p, 0, sizeof(*p));
    p->type = type;
    p->text = strdup(text ? text : "");
    if(text != NULL) {
        if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            p->number = (int64_t)strtoll(text, NULL, 16);
        } else if (text[0] == '0' && text[1] != '\0') {
            p->number = (int64_t)strtoll(text, NULL, 8);
        } else {
            p->number = (int64_t)strtoll(text, NULL, 10);
        }
    } else {
        p->number = 0;
    }
}

void freeToken(token_t* p) {
    free(p->text);
    memset(p, 0, sizeof(*p));
}

char const* const getTokenName(token_type_t type) {
    if((uint32_t)type < sizeof(gTokenNames) / sizeof(gTokenNames[0])) {
        return gTokenNames[type];
    }
    return "<unknown>";
}