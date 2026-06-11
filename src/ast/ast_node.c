#include "ast_node.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

static char const* const gAstNames[] = {
    [AST_Invalid]       = "Invalid",
    [AST_ProgramRoot]   = "Program root",
    [AST_FuncDef]       = "Function definition",
    [AST_VarDef]        = "Variable definition",
    [AST_Block]         = "Code block",
    [AST_AutoDecl]      = "Declaration (auto)",
    [AST_ExtrnDecl]     = "Declaration (extrn)",
    [AST_If]            = "If",
    [AST_While]         = "While",
    [AST_Switch]        = "Switch",
    [AST_Case]          = "Case",
    [AST_Label]         = "Label",
    [AST_Goto]          = "Goto",
    [AST_Return]        = "Return",
    [AST_Break]         = "Break",
    [AST_ExprStmt]      = "Expression statement",
    [AST_Asm]           = "Assembly block",
    [AST_Empty]         = "Empty expression",
    [AST_Number]        = "Numeric literal",
    [AST_String]        = "String literal",
    [AST_Identifier]    = "Identifier",
    [AST_Assign]        = "Assignment",
    [AST_BinOp]         = "Binary operation",
    [AST_UnaryOp]       = "Unary operation",
    [AST_PostfixOp]     = "Postfix operation",
    [AST_Call]          = "Function call",
    [AST_Index]         = "Index",
    [AST_Conditional]   = "Conditional (inline-if)",
    [AST_AddrOf]        = "Address of",
    [AST_Deref]         = "Dereference"
};

void initAstNode(ast_node_t* p, ast_type_t type, int32_t line, int32_t col) {
    if(p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
    p->type = type;
    p->line = line;
    p->col = col;
}

void freeAstNode(ast_node_t* p) {
    if(p == NULL) {
        return;
    }

    free(p->text);
    free(p->op);

    freeAstNode(p->left);
    freeAstNode(p->right);
    freeAstNode(p->extra);

    for(uint32_t i = 0u; i < p->numChildren; ++i) {
        freeAstNode(p->children[i]);
    }
    free(p->children);
    p->children = NULL;
    p->numChildren = 0u;

    for(uint32_t i = 0u; i < p->numParams; ++i) {
        free(p->paramNames[i]);
    }
    free(p->paramNames);
    p->paramNames = NULL;
    p->numParams = 0u;

    free(p);
}

void nodeAddChild(ast_node_t* p, ast_node_t* const child) {
    // Unsure if it's worth preallocating more memory,
    // just grown linearly unless it becomes a problem
    p->children = realloc(p->children, sizeof(ast_node_t*) * (p->numChildren + 1));
    p->children[p->numChildren++] = child;
}

char const* const getAstTypeName(ast_type_t type) {
    if (type < sizeof(gAstNames) / sizeof(gAstNames[0])){
        return gAstNames[type];
    }
    return "<unknown>";
}

static void indent(FILE* stream, int32_t level) {
    static char const* const sIndentPerLevel = "  ";
    while (level--) { fputs(sIndentPerLevel, stream); }
}

void dumpNodesRecursively(FILE* stream, ast_node_t const* node, int32_t level) {
    indent(stream, level);

    if (!node) {
        fputs("<null>", stream);
        return;
    }

    fprintf(stream, "[ %s ]", getAstTypeName(node->type));

    if(node->text != NULL && *node->text) {
        fprintf(stream, "text = '%s'", node->text);
    }
    if(node->op != NULL && *node->op) {
        fprintf(stream, "op = '%s'", node->op);
    }

    switch(node->type) {
        case AST_Number:
        case AST_VarDef:
        case AST_Case:
            fprintf(stream, "value = %lld", node->value);
            break;
        default:
            break;
    }

    // Parameters, should only be set for AST_FuncDef types
    if(node->numParams > 0) {
        fputs("Params:\n", stream);
        for(uint32_t i = 0u; i < node->numParams; ++i) {
            indent(stream, level + 1);
            fprintf(stream, "%d - %s\n", i, node->paramNames[i]);
        }
    }
    
    // Named children
    if(node->left) {
        indent(stream, level + 1);
        fputs("left:\n", stream);
        dumpNodesRecursively(stream, node->left, level + 2);
        fputc('\n', stream);
    }
    if(node->right) {
        indent(stream, level + 1);
        fputs("right:\n", stream);
        dumpNodesRecursively(stream, node->right, level + 2);
        fputc('\n', stream);
    }
    if(node->extra) {
        indent(stream, level + 1);
        fputs("extra:\n", stream);
        dumpNodesRecursively(stream, node->extra, level + 2);
        fputc('\n', stream);
    }

    // Variadic
    for(uint32_t i = 0u; i < node->numChildren; ++i) {
        indent(stream, level + 1);
        fprintf(stream, "child[%d]:\n", (int32_t)i);
        dumpNodesRecursively(stream, node->children[i], level + 2);
        fputc('\n', stream);
    }
}

/* This is extremely ugly */
void dumpNodes(FILE* stream, ast_node_t const* node) {
    fprintf(stream, "Full AST tree - %s at %p:\n", node == NULL ? "<none>" : getAstTypeName(node->type), node);
    dumpNodesRecursively(stream, node, 0);
}