#include "pass2.h"
#include "pass1.h"
#include "../compile.h"
#include "../ast/ast_node.h"
#include <stdlib.h>
#include <string.h>

#define __COMPONENT__ "Compiler Pass 2"

typedef struct ir_context_s {
    compiler_context_t* ctx;
    ir_inst_t* instrs;
    uint32_t numInstrs;
    uint32_t capacity;
    int32_t nextTemp;
    int32_t nextLabel;
    symbol_table_t* table;
    string_entry_t* strings;
    uint32_t numStrings;
} ir_context_t;

// Creates generic instruction, returns ptr for user to fill in instruction-specific fields
static ir_inst_t* emit(ir_context_t* ctx, ir_op_t op);
static int32_t newTemp(ir_context_t* ctx);
static int32_t newLabel(ir_context_t* ctx);
static ir_op_t binOpToIR(char const* const opStr);
static ir_op_t unaryOpToIR(char const* const opStr);
static void lowerStmt(ir_context_t* ctx, ast_node_t* node);
static int32_t lowerExpr(ir_context_t* ctx, ast_node_t* node);
static int32_t lowerIndexAddr(ir_context_t* ctx, ast_node_t* node);
static char* labelStr(uint32_t id);
static char* internString(ir_context_t* ctx, char const* content);

// IR lowering pass: flattens nested expressions into linear instructions
ir_result_t* pass2(ast_node_t const* root, symbol_table_t* symbols, compiler_context_t* ctx) {
    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Executing...\n",
            __COMPONENT__
        );
    }

    if( root == NULL ||
        ctx == NULL ||
        root->type != AST_ProgramRoot ||
        symbols == NULL
    ) {
        return NULL;
    }

    ir_result_t* res = malloc(sizeof(ir_result_t));
    memset(res, 0, sizeof(*res));

    ir_context_t context;
    memset(&context, 0, sizeof(context));
    context.table = symbols;
    context.ctx = ctx;

    for(uint32_t i = 0u; i < root->numChildren; ++i) {
        lowerStmt(&context, root->children[i]);
    }

    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Finished with instrs='%p' , numInsts='%d'\n",
            __COMPONENT__,
            context.instrs,
            context.numInstrs
        );
    }

    res->instrs = context.instrs;
    res->numInstrs = context.numInstrs;
    res->strings = context.strings;
    res->numStrings = context.numStrings;
    return res;
}

// Creates generic instruction, returns ptr for user to fill in instruction-specific fields
ir_inst_t* emit(ir_context_t* ctx, ir_op_t op) {
    if(ctx->numInstrs >= ctx->capacity) {
        ctx->capacity = ctx->capacity == 0u ? 8 : ctx->capacity * 2;
        ctx->instrs = realloc(ctx->instrs, ctx->capacity * sizeof(ir_inst_t));
    }
    ir_inst_t* const inst = &ctx->instrs[ctx->numInstrs++];
    memset(inst, 0, sizeof(ir_inst_t));
    inst->op = op;
    inst->dst = -1;
    inst->src1 = -1;
    inst->src2 = -1;
    return inst;
}
int32_t newTemp(ir_context_t* ctx) {
    if(ctx->ctx->flags.verbose) {
        fprintf(stdout, "newTemp: %d\n", ctx->nextTemp);
    }
    return ctx->nextTemp++;
}
int32_t newLabel(ir_context_t* ctx) {
    return ctx->nextLabel++;
}
ir_op_t binOpToIR(char const* const opStr) {
    if (strcmp(opStr, "+") == 0) return IR_Add;
    if (strcmp(opStr, "-") == 0) return IR_Sub;
    if (strcmp(opStr, "*") == 0) return IR_Mul;
    if (strcmp(opStr, "/") == 0) return IR_Div;
    if (strcmp(opStr, "%") == 0) return IR_Mod;
    if (strcmp(opStr, "==") == 0) return IR_Eq;
    if (strcmp(opStr, "!=") == 0) return IR_Ne;
    if (strcmp(opStr, "<") == 0) return IR_Lt;
    if (strcmp(opStr, ">") == 0) return IR_Gt;
    if (strcmp(opStr, "<=") == 0) return IR_Le;
    if (strcmp(opStr, ">=") == 0) return IR_Ge;
    if (strcmp(opStr, "&") == 0) return IR_And;
    if (strcmp(opStr, "|") == 0) return IR_Or;
    if (strcmp(opStr, "^") == 0) return IR_Xor;
    if (strcmp(opStr, "<<") == 0) return IR_Shl;
    if (strcmp(opStr, ">>") == 0) return IR_Shr;
    return -1;
}

static ir_op_t unaryOpToIR(char const* const opStr) {
    if (strcmp(opStr, "-") == 0) return IR_Neg;
    if (strcmp(opStr, "!") == 0) return IR_Not;
    if (strcmp(opStr, "~") == 0) return IR_BitNot;
    /* ++ and -- are omitted here */
    return -1;
}

void lowerStmt(ir_context_t* ctx, ast_node_t* node) {
    if(ctx->ctx->flags.verbose) {
        fprintf(stderr, "lowerStmt(): ctx->table->current = %p\n", ctx->table->current);
    }

    if(!node) {
        return;
    }

    switch (node->type) {
        case AST_Block: {
            for (uint32_t i = 0u; i < node->numChildren; ++i) {
                lowerStmt(ctx, node->children[i]);
            }
            break;
        }
        case AST_ExprStmt: {
            // discard result
            lowerExpr(ctx, node->left);
            break;
        }
        case AST_Return: {
            /*
            ir_inst_t* inst = emit(ctx, IR_Return);
            if (node->left) {
                inst->src1 = lowerExpr(ctx, node->left);
            }
            */
           int32_t src1 = -1;
           if(node->left) {
            src1 = lowerExpr(ctx, node->left); // eval first
           }
           ir_inst_t* inst = emit(ctx, IR_Return); // emit evaluation
           inst->src1 = src1;
            break;
        }
        case AST_If: {
            int32_t elseLabel = newLabel(ctx);
            int32_t endLabel  = newLabel(ctx);

            int32_t cond = lowerExpr(ctx, node->left);

            ir_inst_t* jz = emit(ctx, IR_Jz);
            jz->src1  = cond;
            // e.g. ".L3"
            jz->label = labelStr(elseLabel);

            // then
            lowerStmt(ctx, node->right);

            ir_inst_t* jmp = emit(ctx, IR_Jmp);
            jmp->label = labelStr(endLabel);

            ir_inst_t* el = emit(ctx, IR_Label);
            el->label = labelStr(elseLabel);

            // else (may be NULL, lowerStmt handles it)
            lowerStmt(ctx, node->extra);

            ir_inst_t* end = emit(ctx, IR_Label);
            end->label = labelStr(endLabel);
            break;
        }
        case AST_While: {
            int32_t topLabel  = newLabel(ctx);
            int32_t endLabel  = newLabel(ctx);

            ir_inst_t* top = emit(ctx, IR_Label);
            top->label = labelStr(topLabel);

            int32_t cond = lowerExpr(ctx, node->left);

            ir_inst_t* jz = emit(ctx, IR_Jz);
            jz->src1  = cond;
            jz->label = labelStr(endLabel);

            // body
            lowerStmt(ctx, node->right);

            ir_inst_t* jmp = emit(ctx, IR_Jmp);
            jmp->label = labelStr(topLabel);

            ir_inst_t* end = emit(ctx, IR_Label);
            end->label = labelStr(endLabel);
            break;
        }
        case AST_FuncDef: {
            ir_inst_t* lbl = emit(ctx, IR_Label);
            lbl->label = strdup(node->text);
            // update scope
            symbol_t* funcSym = lookupSymbol(ctx->table->global, node->text);

            if(ctx->ctx->flags.verbose) {

                fprintf(stdout, "lowerStmt FuncDef: '%s' funcSym=%p scope=%p current before=%p\n",
                    node->text,
                    (void*)funcSym,
                    funcSym ? (void*)funcSym->scope : NULL,
                    (void*)ctx->table->current
                );
                fprintf(stdout, "pass2 FuncDef '%s': funcSym=%p scope=%p\n",
                    node->text, (void*)funcSym, funcSym ? (void*)funcSym->scope : NULL
                );

            }

            if(funcSym && funcSym->scope) {
                scope_t* s = funcSym->scope;
                while(s != NULL) {
                    
                    if(ctx->ctx->flags.verbose) {
                        fprintf(stdout, "  scope=%p numSymbols=%u parent=%p\n",
                            (void*)s, s->numSymbols, (void*)s->parent
                        );
                        for(uint32_t i = 0u; i < s->numSymbols; ++i) {
                            fprintf(stdout, "    [%u] name=%s offset=%d\n",
                                i, s->symbols[i].name, s->symbols[i].offset
                            );
                        }
                    }
                    s = s->parent;
                }
            }
            scope_t* savedScope = ctx->table->current;
            if(funcSym != NULL && funcSym->scope != NULL) {
                ctx->table->current = funcSym->scope;
            }

            if(ctx->ctx->flags.verbose) {
                fprintf(stdout, "lowerStmt FuncDef: current after=%p\n",
                    (void*)ctx->table->current
                );
            }

            // body
            lowerStmt(ctx, node->left);
            // emit return if not already done
            if(ctx->numInstrs != 0u && ctx->instrs[ctx->numInstrs - 1u].op != IR_Return) {
                emit(ctx, IR_Return)->src1 = -1; // no rval
            }
            // restore
            ctx->table->current = savedScope;
            break;
        }
        case AST_Asm: {
            ir_inst_t* inst = emit(ctx, IR_Asm);
            // raw asm text
            inst->label = strdup(node->text);
            break;
        }
        case AST_AutoDecl:
        case AST_ExtrnDecl: {
            // handled by symbol table pass, nothing to emit
            break;
        }
        default: {
            break;
        }
    }
}
int32_t lowerExpr(ir_context_t* ctx, ast_node_t* node) {
    if(!node) {
        return -1;
    }

    if(ctx->ctx->flags.verbose) {
        fprintf(stderr, "lowerExp() - node %p, \"%s\" of type \"%s\"\n", node, node->text, getAstTypeName(node->type));
        fprintf(stderr, "lowerExpr(): ctx->table->current = %p\n", ctx->table->current);
    }

    switch(node->type) {
        case AST_Number: {
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_Const);
            inst->dst = dst;
            inst->imm = node->value;
            return dst;
        }
        case AST_String: {
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_GlobalRef);
            inst->dst = dst;
            inst->label = strdup(internString(ctx, node->text));
            return dst;
        }
        case AST_Identifier: {

            if(ctx->ctx->flags.verbose) {
                fprintf(stdout, "ctx=%p\n", (void*)ctx);
                if(ctx && ctx->table) {
                    fprintf(stdout, "table=%p\n", (void*)ctx->table);
                    fprintf(stdout, "current=%p\n", (void*)ctx->table->current);
                }
                fprintf(stdout, "node->text=%p '%s'\n", (void*)node->text, node->text ? node->text : "<null>");
            }

            symbol_t* sym = lookupSymbol(ctx->table->current, node->text);
            int32_t dst = newTemp(ctx);
            if (!sym || sym->type == SYM_Extern || sym->type == SYM_Function) {
                // external or fn, use address
                ir_inst_t* inst = emit(ctx, IR_GlobalRef);
                inst->dst = dst;
                inst->label = strdup(node->text);
            } else if(sym->type == SYM_Global) {
                // global var, load value
                ir_inst_t* inst = emit(ctx, IR_GlobalLoad);
                inst->dst = dst;
                inst->label = strdup(node->text);
            } else {
                // local or param, copy from stack
                ir_inst_t* inst = emit(ctx, IR_Copy);
                inst->dst = dst;
                inst->src1 = sym->offset;
                // will be interpreted as relative to ebp or rbp
                inst->src1IsOffset = true;
            }
            return dst;
        }
        case AST_BinOp: {
            int32_t l = lowerExpr(ctx, node->left);
            int32_t r = lowerExpr(ctx, node->right);
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, binOpToIR(node->op));
            inst->dst = dst;
            inst->src1 = l;
            inst->src2 = r;
            return dst;
        }
        case AST_UnaryOp: {
            int32_t src = lowerExpr(ctx, node->left);
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, unaryOpToIR(node->op));
            inst->dst = dst;
            inst->src1 = src;
            return dst;
        }
        case AST_Assign: {
            int32_t rhs = lowerExpr(ctx, node->right);
            // lvalue: figure out where to store
            if (node->left->type == AST_Deref) {
                int32_t addr = lowerExpr(ctx, node->left->left);
                ir_inst_t* inst = emit(ctx, IR_Store);
                inst->dst = addr;
                inst->src1 = rhs;
            } else if (node->left->type == AST_Index) {
                int32_t addr = lowerIndexAddr(ctx, node->left);
                ir_inst_t* inst = emit(ctx, IR_Store);
                inst->dst = addr;
                inst->src1 = rhs;
            } else {
                // simple variable
                symbol_t* sym = lookupSymbol(ctx->table->current, node->left->text);

                if(ctx->ctx->flags.verbose) {
                    fprintf(stdout, "assign lvalue '%s': sym=%p type=%d offset=%d\n",
                        node->left->text,
                        (void*)sym,
                        sym ? (int32_t)sym->type : -1,
                        sym ? sym->offset : 0
                    );
                }
                
                if(sym == NULL) {
                    addError(
                        ctx->ctx,
                        "[%s] Undefined symbol '%s'\n",
                        __COMPONENT__,
                        node->left->text ? node->left->text : "NULL"
                    );
                    return -1;
                }
                if(sym->type == SYM_Global) {
                    ir_inst_t* inst = emit(ctx, IR_GlobalStore);
                    inst->src1 = rhs;
                    inst->label = strdup(node->left->text);
                } else {
                    ir_inst_t* inst = emit(ctx, IR_Copy);
                    inst->dst = sym->offset;
                    inst->src1 = rhs;
                    inst->dstIsOffset = true;
                }
            }
            return rhs; // assignment expression evaluates to rhs
        }
        case AST_Call: {
            // emit IR_Param for each argument left to right
            for (uint32_t i = 0u; i < node->numChildren; i++) {
                int32_t arg = lowerExpr(ctx, node->children[i]);
                ir_inst_t* p = emit(ctx, IR_Param);
                p->src1 = arg;
            }
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_Call);
            inst->dst = dst;
            if(node->left != NULL && node->left->text != NULL) {
                inst->label = strdup(node->left->text);
            } else {
                addError(
                    ctx->ctx,
                    "[%s] IR_Call has no callee name\n",
                    __COMPONENT__
                );
                inst->label = strdup("ERRORLABEL");
            }
            inst->numArgs = node->numChildren;
            return dst;
        }
        case AST_Index: {
            // rvalue: load the value at base[index]
            int32_t addr = lowerIndexAddr(ctx, node);
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_Load);
            inst->dst = dst;
            inst->src1 = addr;
            return dst;
        }
        case AST_Deref: {
            int32_t src = lowerExpr(ctx, node->left);
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_Load);
            inst->dst = dst;
            inst->src1 = src;
            return dst;
        }
        case AST_AddrOf: {
            int32_t dst = newTemp(ctx);
            ir_inst_t* inst = emit(ctx, IR_AddrOf);
            inst->dst = dst;
            inst->label = strdup(node->left->text);
            return dst;
        }
        case AST_PostfixOp: {
            // load current value
            symbol_t* sym = lookupSymbol(ctx->table->current, node->left->text);
            int32_t dst = newTemp(ctx);
            if(!sym || (sym->type != SYM_Local && sym->type != SYM_Param)) {
                return dst;
            }
            // read current value into dst
            ir_inst_t* load = emit(ctx, IR_Copy);
            load->dst = dst;
            load->src1 = sym->offset;
            load->src1IsOffset = true;
            
            int32_t one = newTemp(ctx);
            ir_inst_t* constOne = emit(ctx, IR_Const);
            constOne->dst = one;
            constOne->imm = strcmp(node->op, "++") == 0 ? 1 : -1;
            int32_t incremented = newTemp(ctx);
            ir_inst_t* add = emit(ctx, IR_Add);
            add->dst = incremented;
            add->src1 = dst;
            add->src2 = one;
            
            ir_inst_t* store = emit(ctx, IR_Copy);
            store->dst = sym->offset;
            store->dstIsOffset = true;
            store->src1 = incremented;
            return dst;  // postfix returns old value
        }
        default: {
            break;
        }
    }
    return -1;
}
int32_t lowerIndexAddr(ir_context_t* ctx, ast_node_t* node) {
    int32_t base  = lowerExpr(ctx, node->left);
    int32_t index = lowerExpr(ctx, node->right);
    int32_t dst   = newTemp(ctx);
    ir_inst_t* i  = emit(ctx, IR_Index);
    i->dst  = dst;
    i->src1 = base;
    i->src2 = index;
    return dst;
}
char* labelStr(uint32_t id) {
    char buf[32u];
    snprintf(buf, sizeof(buf), ".L%d", id);
    return strdup(buf);
}
char* internString(ir_context_t* ctx, char const* content) {
    // check cache
    for(uint32_t i = 0u; i < ctx->numStrings; ++i) {
        if(strcmp(content, ctx->strings[i].content) == 0){
            return ctx->strings[i].label;
        }
    }

    char buf[32u];
    snprintf(buf, sizeof(buf), "STR%u", ctx->numStrings);

    ctx->strings = realloc(ctx->strings, sizeof(string_entry_t) * (ctx->numStrings + 1));
    ctx->strings[ctx->numStrings].label = strdup(buf);
    ctx->strings[ctx->numStrings].content = strdup(content);
    return ctx->strings[ctx->numStrings++].label;
}