#include "walk_nodes.h"
#include "../ast/ast_node.h"
#include <stdlib.h>

void walkNodes(ast_node_t* node, void* ctx, visit_fn_t preVisit, visit_fn_t postVisit) {
    if(node == NULL) {
        return;
    }

    if(preVisit) {
        preVisit(node, ctx);
    }

    switch (node->type) {
        case AST_FuncDef: {
            // Body
            walkNodes(node->left, ctx, preVisit, postVisit);
            break;
        }
        case AST_ProgramRoot:
        case AST_Block: {
            for (uint32_t i = 0u; i < node->numChildren; i++) {
                walkNodes(node->children[i], ctx, preVisit, postVisit);
            }
            break;
        }
        case AST_If: {
             // condition
            walkNodes(node->left,  ctx, preVisit, postVisit);
            // then
            walkNodes(node->right, ctx, preVisit, postVisit);
            // else (may be NULL)
            walkNodes(node->extra, ctx, preVisit, postVisit);
            break;
        }
        case AST_While:
        case AST_Switch: {
            // condition/expr
            walkNodes(node->left,  ctx, preVisit, postVisit);
            // Body
            walkNodes(node->right, ctx, preVisit, postVisit);
            break;
        }
        case AST_ExprStmt:
        case AST_Return:
        case AST_Goto: {
            walkNodes(node->left, ctx, preVisit, postVisit);
            break;
        }
        case AST_BinOp:
        case AST_Assign:
        case AST_Index: {
            walkNodes(node->left,  ctx, preVisit, postVisit);
            walkNodes(node->right, ctx, preVisit, postVisit);
            break;
        }
        case AST_UnaryOp:
        case AST_PostfixOp:
        case AST_Deref:
        case AST_AddrOf: {
            walkNodes(node->left, ctx, preVisit, postVisit);
            break;
        }
        case AST_Call: {
            // Callee
            walkNodes(node->left, ctx, preVisit, postVisit);
            for (uint32_t i = 0u; i < node->numChildren; i++) {
                walkNodes(node->children[i], ctx, preVisit, postVisit);
            }
            break;
        }
        case AST_Number:
        case AST_String:
        case AST_Identifier:
        case AST_Asm:
        case AST_Empty:
        case AST_Break: {
            break;
        }
        case AST_AutoDecl:
        case AST_ExtrnDecl: {
        // Declarations
            for (uint32_t i = 0u; i < node->numChildren; i++) {
                walkNodes(node->children[i], ctx, preVisit, postVisit);
            }
            break;
        }
        case AST_Label: {
            // The labelled statement
            walkNodes(node->right, ctx, preVisit, postVisit);
            break;
        }
        default: {
            break;
        }
    }

    if(postVisit) {
        postVisit(node, ctx);
    }
}