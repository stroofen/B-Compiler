#include "pass1.h"
#include "scope.h"
#include "walk_nodes.h"
#include "../compile.h"
#include "../ast/ast_node.h"
#include <stdlib.h>

#define __COMPONENT__ "Compiler Pass 1"

typedef struct pass_context_s {
    compiler_context_t* ctx;
    symbol_table_t* table;
} pass_context_t;

static void symbolPreVisit(ast_node_t* node, void* ctx) {
    pass_context_t* context = (pass_context_t*)ctx;
    compiler_context_t* compiler = context->ctx;
    symbol_table_t* table = context->table;

    int32_t const wordSize = ((int32_t)getWordSize(ctx));

    switch(node->type) {
        case AST_FuncDef: {
            // One scope deeper
            symbol_t* sym = lookupSymbol(table->global, node->text);
            if(sym == NULL) {
                // fallback in case pre-registration missed it (should never happen)
                sym = insertSymbol(table->global, node->text, 0, SYM_Function);
                addWarning(
                    compiler,
                    "[%s] Function symbol '%s' was somehow not detected during pre-pass registration. Adding and continuing...\n",
                    __COMPONENT__,
                    sym ? sym->name : "<null>"
                );
            }
            table->current = scopePush(table->current);
            sym->scope = table->current;
            table->nextOffset = -wordSize;
            for(uint32_t i = 0u; i < node->numParams; ++i) {
                insertSymbol(table->current, node->paramNames[i], table->nextOffset, SYM_Param);
                table->nextOffset -= wordSize;
            }

            break;
        }
        case AST_AutoDecl: {
                if(compiler->flags.verbose) {
                    fprintf(stdout, "AST_AutoDecl: current=%p global=%p numChildren=%u\n",
                        (void*)table->current,
                        (void*)table->global,
                        node->numChildren
                    );
                }
            for(uint32_t i = 0u; i < node->numChildren; ++i) {
                ast_node_t* var = node->children[i];
                insertSymbol(table->current, var->text, table->nextOffset, SYM_Local);
                table->nextOffset -= wordSize * (var->value > 1 ? var->value : 1);
            }
            break;
        }
        case AST_VarDef: {
            // Global variables have a stack offset of 0
            if(table->current == table->global) {
                insertSymbol(table->global, node->text, 0, SYM_Global);
            }
            break;
        }
        case AST_ExtrnDecl: {
            // Insert symbols into global scope regardless of current scope
            for(uint32_t i = 0u; i < node->numChildren; ++i) {
                ast_node_t* child = node->children[i];
                symbol_t* existing = lookupSymbol(table->global, child->text);

                if(compiler->flags.verbose) {
                    fprintf(stdout, "ExtrnDecl: '%s' existing=%p type=%d\n",
                        child->text,
                        (void*)existing,
                        existing ? existing->type : -1
                    );
                }

                if(existing == NULL) {
                    insertSymbol(table->global, child->text, 0, SYM_Extern);
                }
            }
            break;
        }
        default: {
            break;
        }
    }
}
static void symbolPostVisit(ast_node_t* node, void* ctx) {
    symbol_table_t* table = ((pass_context_t*)ctx)->table;
    
    if(node->type != AST_FuncDef) {
        return;
    }

    
    if(((pass_context_t*)ctx)->ctx->flags.verbose) {
        fprintf(stdout, "symbolPostVisit: before pop current=%p global=%p\n",
            (void*)table->current, (void*)table->global
        );
        table->current = scopePop(table->current);
        fprintf(stdout, "symbolPostVisit: after pop current=%p\n",
            (void*)table->current
        );
    }
}

// Symbol table pass: Collects globals, assigns stack offsets
symbol_table_t* pass1(ast_node_t* root, compiler_context_t* ctx) {
    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Executing...\n",
            __COMPONENT__
        );
    }

    if( ctx == NULL ||
        root == NULL ||
        root->type != AST_ProgramRoot
    ) {
        return NULL;
    }

    symbol_table_t* table = malloc(sizeof(symbol_table_t));
    table->global = scopePush(NULL);
    table->current = table->global;
    table->nextOffset = ((int32_t)getWordSize(ctx)) * -1;

    pass_context_t context;
    context.ctx = ctx;
    context.table = table;

    // Pre-register top-level defs
    for(uint32_t i = 0u; i < root->numChildren; ++i) {
        ast_node_t* node = root->children[i];

        if(ctx->flags.verbose) {
            fprintf(stdout, "pre-reg[%u]: type=%s text=%s\n",
                i, getAstTypeName(node->type),
                node->text ? node->text : "<null>"
            );
        }

        if(node->type == AST_FuncDef) {
            insertSymbol(table->global, node->text, 0, SYM_Function);
        } else if(node->type == AST_VarDef) {
            insertSymbol(table->global, node->text, 0, SYM_Global);
        }
    }

    walkNodes(root, &context, &symbolPreVisit, &symbolPostVisit);

    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Finished with table='%p' , \n",
            __COMPONENT__,
            table
        );
        
        fprintf(stdout, "pass1 done: current=%p global=%p\n",
        (void*)table->current, (void*)table->global
    );
    }
    return table;
}

void freeSymbolTable(symbol_table_t* symbols) {
    if(symbols == NULL) {
        return;
    }
    if(symbols->global == NULL) {
        return;
    }

    for(uint32_t i = 0u; i < symbols->global->numSymbols; ++i) {
        symbol_t* sym = &symbols->global->symbols[i];
        if(sym->type == SYM_Function && sym->scope != NULL) {
            for(uint32_t j = 0u; j < sym->scope->numSymbols; ++j) {
                free(sym->scope->symbols[j].name);
            }
            free(sym->scope->symbols);
            free(sym->scope);
            sym->scope = NULL;
        }
    }
    for(uint32_t i = 0u; i < symbols->global->numSymbols; ++i) {
        free(symbols->global->symbols[i].name);
    }
    free(symbols->global->symbols);
    free(symbols->global);
    free(symbols);
}