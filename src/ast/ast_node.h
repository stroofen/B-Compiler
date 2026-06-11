#ifndef AST_NODE_H
#define AST_NODE_H

#include <stdint.h>
#include <stdio.h>

typedef struct ast_node_s ast_node_t;

typedef enum {
    AST_Invalid,
    
    // Definitions
    AST_ProgramRoot,        // root, list of definitions
    AST_FuncDef,            // name ( params ) body
    AST_VarDef,             // name [ '[' size ']' ] [= ival,...] ;

    // Statements
    AST_Block,              // { stmt* }
    AST_AutoDecl,           // auto name [size], ...;
    AST_ExtrnDecl,          // extrn name, ...;
    AST_If,                 // if (cond) then [else]
    AST_While,              // while (cond) body
    AST_Switch,             // switch (expr) body
    AST_Case,               // case const:
    AST_Label,              // name:
    AST_Goto,               // goto expr;
    AST_Return,             // return [expr];
    AST_Break,              // break;
    AST_ExprStmt,           // expr;
    AST_Asm,                // asm { raw text assembly content }
    AST_Empty,              // ;

    // Expressions
    AST_Number,             // Integer literal
    AST_String,             // String literal
    AST_Identifier,         // name
    AST_Assign,             // lval = expr (and operator variants)
    AST_BinOp,              // expr op expr
    AST_UnaryOp,            // op expr (eg. ~x)
    AST_PostfixOp,          // expr op (eg. x++)
    AST_Call,               // expr ( args )
    AST_Index,              // expr [ expr ]
    AST_Conditional,        // cond ? then : else
    AST_AddrOf,             // & lval
    AST_Deref               // * expr
} ast_type_t;

typedef struct ast_node_s {
    ast_type_t type;

    // Name/text payload for identifiers, labels, string literals, asm body
    char* text;
    // Operator string for binary/unary/assign nodes
    char* op;

    // Integer value
    int64_t value;

    // Primary child ptrs
    ast_node_t* left; // Condition, base, operand, callee
    ast_node_t* right; // then-branch, index, RHS
    ast_node_t* extra; // else-branch, else of ternary

    ast_node_t** children;
    uint32_t numChildren;

    // Parameter name list for function definitions
    char** paramNames;
    uint32_t numParams;

    // Source location
    int32_t line;
    int32_t col;
} ast_node_t;

void initAstNode(ast_node_t* p, ast_type_t type, int32_t line, int32_t col);
void freeAstNode(ast_node_t* p);
void nodeAddChild(ast_node_t* p, ast_node_t* const child);

char const* const getAstTypeName(ast_type_t type);

/* This is extremely ugly */
void dumpNodes(FILE* stream, ast_node_t const* node);

#endif