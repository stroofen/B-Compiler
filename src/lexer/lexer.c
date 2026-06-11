#include "lexer.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define __COMPONENT__ "Lexer"

// Keyword helpers

static char const* const gKeyWords[] = {
    "auto",
    "extrn",
    "return",
    "if",
    "else",
    "while",
    "goto",
    "switch",
    "case",
    "break",
    "asm"
};

static bool isKeyword(char const* const text) {
    static size_t const sNumKeywords = sizeof(gKeyWords) / sizeof(gKeyWords[0]);

    for(size_t i = 0u; i < sNumKeywords; ++i) {
        if(strcmp(gKeyWords[i], text) == 0) {
            return true;
        }
    }
    return false;
}

// Internal lexing functionality

static char lexPeek(lexer_t* p) {
    if(p->pos >= p->len) {
        return -1;
    }
    return p->src[p->pos];
}

// Peek one step ahead
static char lexPeek2(lexer_t* p) {
    if(p->pos +1 >= p->len){
        return -1;
    }
    return p->src[p->pos + 1];
}

static char lexAdvance(lexer_t* p) {
    if(p->pos >= p->len) {
        return -1;
    }
    char const ch = p->src[p->pos++];
    if(ch == '\0') {
        ++p->line;
        p->col = 1;
    } else {
        ++p->col;
    }
    return ch;
}

static void skipWhitespaceAndComments(lexer_t* p) {
    while(1) {

        while(p->pos < p->len && isspace((int)p->src[p->pos])) {
            lexAdvance(p);
        }

        // Multi-line/block comment
        if(
            p->pos < p->len &&
            p->src[p->pos] == '/' && p->src[p->pos + 1] == '*'
        ) {
            // Consue / and *
            lexAdvance(p);
            lexAdvance(p);

            // Consume until * and /
            while(p->pos + 1 < p->len) {
                if(p->src[p->pos] == '*' && p->src[p->pos + 1] == '/') {
                    lexAdvance(p);
                    lexAdvance(p);
                    break;
                }
                lexAdvance(p);
            }
            continue;
        }

        // Single line comment
        if(
            p->pos < p->len &&
            p->src[p->pos] == '/' && p->src[p->pos + 1] == '/'
        ) {
            while(p->pos < p->len && p->src[p->pos] != '\0') {
                lexAdvance(p);
            }

            continue;
        }

        break;

    }
}

static char* bufAppend(char* buf, size_t* len, size_t* cap, char c) {
    if (*len + 1 >= *cap) {
        *cap = (*cap == 0) ? 64 : (*cap * 2);
        buf  = realloc(buf, *cap);
        if (!buf) {
            fprintf(stderr, "Realloc failed\n");
            exit(1);
        }
    }
    buf[(*len)++] = c;
    buf[*len] = '\0';
    return buf;
}

static int decodeEscape(lexer_t* l) {
    char ch = lexAdvance(l);
    switch (ch) {
        case 'n': return '\n';
        case 't': return '\t';
        case '0': return '\0';
        case '*': return '*';
        case '\'': return '\'';
        case '"': return '"';
        case 'e': return 4;
        default: return ch;
    }
}

// Token specific lexers

static token_t lexString(lexer_t* p, char const delim) {
    // Consume opening quote
    lexAdvance(p);
 
    char*  buf = NULL;
    size_t len = 0;
    size_t cap = 0;
 
    while (p->pos < p->len) {
        char const ch = lexPeek(p);
        if (ch == delim) {
            lexAdvance(p);
            break;
        }

        if (ch == '*') {
            lexAdvance(p);
            char const esc = (char)decodeEscape(p);
            buf = bufAppend(buf, &len, &cap, esc);
        } else {
            buf = bufAppend(buf, &len, &cap, (char)ch);
            lexAdvance(p);
        }
    }
    if (!buf) {
        cap = 1;
        buf = calloc(1, 1);
    }
 
    token_t tok;
    initToken(&tok, TOK_String, buf);
    free(buf);
    return tok;
}

static token_t lexNumber(lexer_t* p) {
    char* buf = NULL;
    size_t len = 0;
    size_t cap = 0;
 
    if (lexPeek(p) == '0' &&
        (lexPeek2(p) == 'x' || lexPeek2(p) == 'X')) {
        buf = bufAppend(buf, &len, &cap, (char)lexAdvance(p));
        buf = bufAppend(buf, &len, &cap, (char)lexAdvance(p));
        while (p->pos < p->len && isxdigit(p->src[p->pos]))
            buf = bufAppend(buf, &len, &cap, (char)lexAdvance(p));
    } else {
        while (p->pos < p->len && isdigit(p->src[p->pos])) {
            buf = bufAppend(buf, &len, &cap, lexAdvance(p));
        }
    }
 
    token_t tok;
    initTokenNumeric(&tok, TOK_Number, buf);
    free(buf);
    return tok;
}

static token_t lexIdentifierOrKeyword(lexer_t* p) {
    char* buf = NULL;
    size_t len = 0;
    size_t cap = 0;
 
    while (p->pos < p->len) {
        char const ch = p->src[p->pos];
        if (!isalnum(ch) && ch != '_') {
            break;
        }
        buf = bufAppend(buf, &len, &cap, (char)lexAdvance(p));
    }

    if (!buf) {
        cap = 1;
        buf = calloc(1, 1);
    }
 
    token_t tok;
    initToken(&tok, isKeyword(buf) ? TOK_Keyword : TOK_Identifier, buf);
    free(buf);
    return tok;
}

static token_t lexAsmBlock(lexer_t* p) {
    skipWhitespaceAndComments(p);
 
    if (lexPeek(p) != '{') {
        fprintf(
            stderr,
            "[%s] : expected '{' after 'asm' at line %d col %d\n",
            __COMPONENT__,
            p->line,
            p->col
        );
        token_t err;
        initToken(&err, TOK_Eof, NULL);
        return err;
    }
    lexAdvance(p); /* consume '{' */
 
    char* buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    int32_t depth = 1;
 
    while (p->pos < p->len && depth > 0) {
        char ch = lexPeek(p);
        if (ch == '{') {
            depth++;
        }
        else if (ch == '}') {
            depth--;
            if (depth == 0) {
                lexAdvance(p);
                break;
            }
        }
        buf = bufAppend(buf, &len, &cap, ch);
        lexAdvance(p);
    }
    if (!buf) {
        cap = 1;
        buf = calloc(1, 1);
    }
 
    token_t tok;
    initToken(&tok, TOK_AsmBlock, buf);
    free(buf);
    return tok;
}

static token_t lexOperator(lexer_t* l) {
    char buf[4] = {0};
    char ch = lexAdvance(l);
    char ch2 = lexPeek(l);
    buf[0] = ch;
 
    switch (ch) {
        case '=': if (ch2 == '=') { buf[1] = lexAdvance(l); } break;
        case '!': if (ch2 == '=') { buf[1] = lexAdvance(l); } break;
        case '<': if (ch2 == '=' || ch2 == '<') { buf[1] = lexAdvance(l); } break;
        case '>': if (ch2 == '=' || ch2 == '>') { buf[1] = lexAdvance(l); } break;
        case '&': if (ch2 == '&') { buf[1] = lexAdvance(l); } break;
        case '|': if (ch2 == '|') { buf[1] = lexAdvance(l); } break;
        case '+': if (ch2 == '+') { buf[1] = lexAdvance(l); } break;
        case '-': if (ch2 == '-') { buf[1] = lexAdvance(l); } break;
        default: break;
    }
 
    token_t tok;
    initToken(&tok, TOK_Operator, buf);
    return tok;
}


// Public API

void initLexer(lexer_t* p, char const* const src) {
    if(p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
    p->src = src;
    p->pos = 0;
    p->len = strlen(src);
    p->line = 1;
    p->col = 1;
    p->pendingAsm = false;
}
void freeLexer(lexer_t* p) {
    p->src = NULL;
}

int32_t lexerLine(lexer_t const* p) {
    return p->line;
}
int32_t lexerCol(lexer_t const* p) {
    return p->col;
}

// Consume + return next token. Caller must call freeToken() on the result.
token_t lexerNext(lexer_t* p) {
    if(p->pendingAsm) {
        p->pendingAsm = false;
        return lexAsmBlock(p);
    }

    skipWhitespaceAndComments(p);

    // eof
    if(p->pos >= p->len) {
        token_t eof;
        initToken(&eof, TOK_Eof, NULL);
        return eof;
    }

    char ch = lexPeek(p);

    if(ch == '"' || ch == '\'') {
        return lexString(p, ch);
    }
    
    if(isdigit(ch)) {
        return lexNumber(p);
    }

    if(isalpha(ch) || ch == '_') {
        token_t token = lexIdentifierOrKeyword(p);
        if(token.type == TOK_Keyword && strcmp(token.text, "asm") == 0) {
            p->pendingAsm = true;
        }
        return token;
    }

    switch(ch) {
        case '(': lexAdvance(p); { token_t t; initToken(&t, TOK_ParenthesisOpen,    "("); return t; }
        case ')': lexAdvance(p); { token_t t; initToken(&t, TOK_ParenthesisClose,   ")"); return t; }
        case '[': lexAdvance(p); { token_t t; initToken(&t, TOK_SquareBracketOpen,  "["); return t; }
        case ']': lexAdvance(p); { token_t t; initToken(&t, TOK_SquareBracketClose, "]"); return t; }
        case '{': lexAdvance(p); { token_t t; initToken(&t, TOK_CurlyBracketOpen,   "{"); return t; }
        case '}': lexAdvance(p); { token_t t; initToken(&t, TOK_CurlyBracketClose,  "}"); return t; }
        case ',': lexAdvance(p); { token_t t; initToken(&t, TOK_Comma,              ","); return t; }
        case ';': lexAdvance(p); { token_t t; initToken(&t, TOK_Semicolon,          ";"); return t; }
        default:  break;
    }

    if(strchr("=+-*/%&|^!~<>.:?", ch)) {
        return lexOperator(p);
    }

    fprintf(
        stderr,
        "[%s] : unexpected character '%c' (0x%02x) at line %d col %d\n",
        __COMPONENT__,
        isprint(ch) ? ch : '?',
        ch,
        p->line,
        p->col
    );

    lexAdvance(p);
    return lexerNext(p);
}
// Return next token without consuming. Caller must call freeToken() on the result.
token_t lexerPeek(lexer_t* p) {
    size_t const savedPos = p->pos;
    int32_t const savedLine = p->line;
    int32_t const savedCol = p->col;
    bool const savedPendingAsm = p->pendingAsm;

    token_t token = lexerNext(p);

    p->pos = savedPos;
    p->line = savedLine;
    p->col = savedCol;
    p->pendingAsm = savedPendingAsm;

    return token;
}