#ifndef SCOPE_H
#define SCOPE_H

#include <stdint.h>

typedef struct symbol_s symbol_t;
typedef struct scope_s scope_t;

typedef enum {
    SYM_Global,
    SYM_Extern,
    SYM_Function,
    SYM_Local,
    SYM_Param
} sym_type_t;

typedef struct symbol_s {
    char* name;
    sym_type_t type;
    int32_t offset; // offset from ebp (x86) / rbp (x64)
    scope_t* scope;
} symbol_t;

typedef struct scope_s {
    symbol_t* symbols;
    uint32_t numSymbols;
    uint32_t capacity;
    scope_t* parent;
} scope_t;

void freeScope(scope_t* p);
scope_t* scopePush(scope_t* parent);
scope_t* scopePop(scope_t* p);
symbol_t* insertSymbol(scope_t* scope, char const* const symbolName, int32_t offset, sym_type_t type);
symbol_t* lookupSymbol(scope_t* scope, char const* const symbolName);

#endif