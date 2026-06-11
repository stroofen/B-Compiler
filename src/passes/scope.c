#include "scope.h"
#include <stdlib.h>
#include <string.h>

void freeScope(scope_t* p) {
    for(uint32_t i = 0u; i < p->numSymbols; ++i) {
        free(p->symbols[i].name);
    }
    free(p->symbols);
    free(p);
}

scope_t* scopePush(scope_t* parent) {
    scope_t* scope = malloc(sizeof(scope_t));
    scope->symbols = NULL;
    scope->numSymbols = 0u;
    scope->capacity = 0u;
    scope->parent = parent;
    return scope;
}

scope_t* scopePop(scope_t* p) {
    scope_t* parent = p->parent;
    //freeScope(p);
    return parent;
}

symbol_t* insertSymbol(scope_t* scope, char const* const symbolName, int32_t offset, sym_type_t type) {
    if(scope == NULL || symbolName == NULL) {
        return NULL;
    }
    
    if(scope->numSymbols >= scope->capacity) {
        // Preallocate 8 symbols just in case
        scope->capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        scope->symbols = realloc(scope->symbols, scope->capacity * sizeof(symbol_t));
    }
    symbol_t* sym = &scope->symbols[scope->numSymbols++];
    sym->type = type;
    sym->name = strdup(symbolName);
    sym->offset = offset;
    sym->scope = scope;
    return sym;
}

symbol_t* lookupSymbol(scope_t* scope, char const* const symbolName) {
    if(scope == NULL || symbolName == NULL) {
        return NULL;
    }

    for(uint32_t i = 0u; i < scope->numSymbols; ++i) {
        if(strcmp(scope->symbols[i].name, symbolName) == 0) {
            return &scope->symbols[i];
        }
    }
    
    return lookupSymbol(scope->parent, symbolName);
}