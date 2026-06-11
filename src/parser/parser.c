#include "parser.h"
#include "../ast/ast_node.h"
#include "../compile.h"
#include <stdlib.h>
#include <string.h>

#define __COMPONENT__ "Parser"

// Token "navigation"

static token_type_t curType(parser_t* p) {
    return p->current.type;
}
static char const* const curText(parser_t* p) {
    static char const* const sEmptyStr = "";
    return p->current.text ? p->current.text : sEmptyStr;
}
static int32_t curLine(parser_t* p) {
    return lexerLine(p->lexer);
}
static int32_t curCol(parser_t* p) {
    return lexerCol(p->lexer);
}
// Consume current token and load the next one
// Caller must free the returned token using freeToken().
static token_t advance(parser_t* p) {
    token_t prev = p->current;
    if(p->hasPeek) {
        p->current = p->peek;
        p->hasPeek = false;
    } else {
        p->current = lexerNext(p->lexer);
    }
    return prev;
}
static token_t* peekToken(parser_t* p) {
    if(!p->hasPeek) {
        p->peek = lexerNext(p->lexer);
        p->hasPeek = true;
    }
    return &p->peek;
}


// Helper functions for parsing

// Consume current token if it matches the expected token type.
// Caller must free the returned token using freeToken().
static token_t expect(parser_t* p, token_type_t type) {
    if(curType(p) != type) {
        addError(
            p->ctx,
            "[%s] : Expected %s, got %s ('%s')\n",
            __COMPONENT__,
            getTokenName(type),
            getTokenName(curType(p)),
            curText(p)
        );
    }
    return advance(p);
}

// Checks without consuming
static bool checkKeyword(parser_t* p, char const* const keyword) {
    return curType(p) == TOK_Keyword && strcmp(curText(p), keyword) == 0;
}

// Checks without consuming
static bool checkOperator(parser_t* p, char const* const operation) {
    return curType(p) == TOK_Operator && strcmp(curText(p), operation) == 0;
}

// Consume a semicolon
static void expectSemicolon(parser_t* p) {
    if(curType(p) == TOK_Semicolon) {
        token_t t = advance(p);
        freeToken(&t);
    } else {
        addError(
            p->ctx,
            "[%s] : Expected ';', got %s ('%s')\n",
            __COMPONENT__,
            getTokenName(curType(p)),
            curText(p)
        );
    }
}

static bool isAssignOp(char const* const op) {
    if(!op) {
        return false;
    }
    return strcmp(op, "=") == 0;
}

static bool isLabel(parser_t* p) {
    if(curType(p) != TOK_Identifier) {
        return false;
    }
    token_t* next = peekToken(p);
    if(!next) {
        return false;
    }
    if(next->type != TOK_Operator) {
        return false;
    }
    return strcmp(next->text ? next->text : "", ":") == 0;
}


// Internal parser functionality

// Forward declarations
static ast_node_t* parseBinaryOp(parser_t* p, ast_node_t*(*parseNext)(parser_t*), char const* const* opstrs, size_t opstrsSize);
static ast_node_t* parseAutoDecl(parser_t* p);
static ast_node_t* parseExtrnDecl(parser_t* p);
static ast_node_t* parseBlock(parser_t* p);
static ast_node_t* parseStatement(parser_t* p);
static ast_node_t* parseExpr(parser_t* p);
static ast_node_t* parseAssign(parser_t* p);
static ast_node_t* parseConditional(parser_t* p);
static ast_node_t* parseLogOr(parser_t* p);
static ast_node_t* parseLogAnd(parser_t* p);
static ast_node_t* parseBitOr(parser_t* p);
static ast_node_t* parseBitXor(parser_t* p);
static ast_node_t* parseBitAnd(parser_t* p);
static ast_node_t* parseEquality(parser_t* p);
static ast_node_t* parseRelational(parser_t* p);
static ast_node_t* parseShift(parser_t* p);
static ast_node_t* parseAdditive(parser_t* p);
static ast_node_t* parseMultiply(parser_t* p);
static ast_node_t* parseUnary(parser_t* p);
static ast_node_t* parsePostfix(parser_t* p);
static ast_node_t* parsePrimary(parser_t* p);

// Template for left-associative binary operators
ast_node_t* parseBinaryOp(parser_t* p, ast_node_t*(*parseNext)(parser_t*), char const* const* opstrs, size_t opstrsSize) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);
    ast_node_t* left = parseNext(p);
    size_t const opCount = opstrsSize / sizeof(char const*);
    while(1) {
        if(curType(p) != TOK_Operator) {
            break;
        }
        char const* cur = curText(p);
        bool match = false;
        for(int32_t i = 0; i < (int32_t)opCount; ++i) {
            if(strcmp(cur, opstrs[i]) == 0) {
                match = true;
                break;
            }
        }
        if(!match) {
            break;
        }
        token_t token = advance(p);
        ast_node_t* right = parseNext(p);
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_BinOp, line, col);
        node->op = token.text;
        token.text = NULL;
        freeToken(&token);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;                           
}

// auto name [size], ... ;
ast_node_t* parseAutoDecl(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    // Consume 'auto'
    token_t keyword = advance(p);
    freeToken(&keyword);

    ast_node_t* decl = malloc(sizeof(ast_node_t));
    initAstNode(decl, AST_AutoDecl, line, col);
    while(curType(p) != TOK_Semicolon && curType(p) != TOK_Eof) {

        if(curType(p) != TOK_Identifier) {
            addError(
                p->ctx,
                "[%s] : Expected identifier in auto, got %s ('%s')\n",
                __COMPONENT__,
                getTokenName(curType(p)),
                curText(p)
            );
            token_t skip = advance(p);
            freeToken(&skip);
            continue;
        }

        token_t name = advance(p);
        ast_node_t* var = malloc(sizeof(ast_node_t));
        initAstNode(var, AST_VarDef, line, col);
        var->text = name.text;
        name.text = NULL;
        freeToken(&name);

        // Optional array size
        if(curType(p) == TOK_SquareBracketOpen) {
            token_t temp = advance(p);
            freeToken(&temp);

            if(curType(p) == TOK_Number) {
                var->value = p->current.number;
                temp = advance(p);
                freeToken(&temp);
            }

            temp = expect(p, TOK_SquareBracketClose);
            freeToken(&temp);
        }

        // Scalar initializer after name or after []
        if(curType(p) == TOK_Number) {
            ast_node_t* ival = malloc(sizeof(ast_node_t));
            initAstNode(ival, AST_Number, line, col);
            ival->value = p->current.number;
            nodeAddChild(var, ival);
            token_t temp = advance(p);
            freeToken(&temp);
        }

        nodeAddChild(decl, var);
        if(curType(p) != TOK_Comma) {
            break;
        }
        
        token_t temp = advance(p);
        freeToken(&temp);
    }

    expectSemicolon(p);
    return decl;
}

ast_node_t* parseExtrnDecl(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    // Consume 'extrn'
    token_t keyword = advance(p);
    freeToken(&keyword);

    ast_node_t* decl = malloc(sizeof(ast_node_t));
    initAstNode(decl, AST_ExtrnDecl, line, col);
    while(curType(p) != TOK_Semicolon && curType(p) != TOK_Eof) {

        if(curType(p) != TOK_Identifier) {
            addError(
                p->ctx,
                "[%s] : Expected identifier in extrn, got %s ('%s')\n",
                __COMPONENT__,
                getTokenName(curType(p)),
                curText(p)
            );
            token_t temp = advance(p);
            freeToken(&temp);
            continue;
        }

        token_t name = advance(p);
        
        ast_node_t* identifier = malloc(sizeof(ast_node_t));
        initAstNode(identifier, AST_Identifier, line, col);
        identifier->text = name.text;
        name.text = NULL;
        freeToken(&name);
        
        nodeAddChild(decl, identifier);

        if(curType(p) != TOK_Comma) {
            break;
        }

        token_t temp = advance(p);
        freeToken(&temp);
    }

    expectSemicolon(p);
    return decl;
}

ast_node_t* parseBlock(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    // Comsune '{'
    token_t temp = advance(p);
    freeToken(&temp);

    ast_node_t* block = malloc(sizeof(ast_node_t));
    initAstNode(block, AST_Block, line, col);
    while(curType(p) != TOK_CurlyBracketClose && curType(p) != TOK_Eof) {
        ast_node_t* statement = parseStatement(p);
        if(statement) {
            nodeAddChild(block, statement);
        }
    }
    // Consume '}'
    temp = expect(p, TOK_CurlyBracketClose);
    freeToken(&temp);

    return block;
}

ast_node_t* parseStatement(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    if(checkKeyword(p, "auto")) {
        return parseAutoDecl(p);
    }
    if(checkKeyword(p, "extrn")) {
        return parseExtrnDecl(p);
    }
    if(checkKeyword(p, "if")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_If, line, col);

        // Consume '('
        token_t leftP = expect(p, TOK_ParenthesisOpen);
        freeToken(&leftP);

        node->left = parseExpr(p);

        // Consume ')'
        token_t rightP = expect(p, TOK_ParenthesisClose);
        freeToken(&rightP);

        node->right = parseStatement(p);

        if(checkKeyword(p, "else")) {
            // Consume 'else'
            token_t temp = advance(p);
            freeToken(&temp);

            node->extra = parseStatement(p);
        }

        return node;
    }
    if(checkKeyword(p, "while")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_While, line, col);

        // Consume '('
        token_t leftP = expect(p, TOK_ParenthesisOpen);
        freeToken(&leftP);

        node->left = parseExpr(p);

        // Consume ')'
        token_t rightP = expect(p, TOK_ParenthesisClose);
        freeToken(&rightP);

        node->right = parseStatement(p);
        return node;
    }
    if(checkKeyword(p, "switch")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Switch, line, col);

        // Consume '('
        token_t leftP = expect(p, TOK_ParenthesisOpen);
        freeToken(&leftP);

        node->left = parseExpr(p);

        // Consume ')'
        token_t rightP = expect(p, TOK_ParenthesisClose);
        freeToken(&rightP);

        node->right = parseStatement(p);
        return node;
    }
    if(checkKeyword(p, "case")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Case, line, col);

        if(curType(p) == TOK_Number) {
            node->value = p->current.number;
            token_t temp = advance(p);
            freeToken(&temp);
        } else {
            addError(
                p->ctx,
                "[%s] : Expected constant/string literal after 'case', got %s ('%s') , line %d col %d\n",
                __COMPONENT__,
                getTokenName(curType(p)),
                curText(p),
                line,
                col
            );
        }

        // Consume ':'
        if(checkOperator(p, ":")) {
            token_t temp = advance(p);
            freeToken(&temp);
        } else {
            addError(
                p->ctx,
                "[%s] : Expected ':' after constant , line %d col %d\n",
                __COMPONENT__,
                line,
                col
            );
        }

        return node;
    }
    if(checkKeyword(p, "goto")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Goto, line, col);

        node->left = parseExpr(p);
        expectSemicolon(p);
        return node;
    }
    if(checkKeyword(p, "return")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Return, line, col);

        if(curType(p) == TOK_ParenthesisOpen) {
            token_t leftP = advance(p);
            freeToken(&leftP);
            node->left = parseExpr(p);
            token_t rightP = advance(p);
            freeToken(&rightP);
        } else if(curType(p) != TOK_Semicolon) {
            node->left = parseExpr(p);
        }
        expectSemicolon(p);
        return node;
    }
    if(checkKeyword(p, "break")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Break, line, col);

        expectSemicolon(p);
        return node;
    }
    if(checkKeyword(p, "asm")) {
        token_t keyword = advance(p);
        freeToken(&keyword);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Asm, line, col);
        
        if(curType(p) == TOK_AsmBlock) {
            token_t block = advance(p);
            node->text = block.text;
            block.text = NULL;
            freeToken(&block);
        } else {
            addError(
                p->ctx,
                "[%s] : Expected asm block, got '%s' ('%s')\n",
                __COMPONENT__,
                getTokenName(curType(p)),
                curText(p)
            );
        }
        return node;
    }

    // Block
    if(curType(p) == TOK_CurlyBracketOpen) {
        return parseBlock(p);
    }

    // Label
    // identifier ':' statement
    if(isLabel(p)) {
        token_t name = advance(p);
        token_t colon = advance(p);
        freeToken(&colon);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Label, line, col);
        node->text = name.text;
        name.text = NULL;
        freeToken(&name);

        node->right = parseStatement(p);
        return node;
    }

    // Expression statement
    ast_node_t* node = malloc(sizeof(ast_node_t));
    initAstNode(node, AST_ExprStmt, line, col);
    node->left = parseExpr(p);
    expectSemicolon(p);
    return node;
}

ast_node_t* parseExpr(parser_t* p) {
    return parseAssign(p);
}

ast_node_t* parseAssign(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    ast_node_t* lhs = parseConditional(p);

    if(curType(p) == TOK_Operator && isAssignOp(curText(p))) {
        token_t operatorToken = advance(p);

        ast_node_t* rhs = parseAssign(p);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Assign, line, col);
        node->op = operatorToken.text;
        operatorToken.text = NULL;
        freeToken(&operatorToken);

        node->left = lhs;
        node->right = rhs;
        return node;
    }
    return lhs;
}

ast_node_t* parseConditional(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    ast_node_t* cond = parseLogOr(p);

    if(checkOperator(p, "?")) {
        token_t temp = advance(p);
        freeToken(&temp);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Conditional, line, col);
        node->left = cond;
        node->right = parseExpr(p);

        // Consume ':'
        token_t separator = expect(p, TOK_Operator);
        freeToken(&separator);

        node->extra = parseConditional(p);
        return node;
    }
    return cond;
}

ast_node_t* parseLogOr(parser_t* p) {
    static char const* const sOps[] = { "||" };
    return parseBinaryOp(p, &parseLogAnd, sOps, sizeof(sOps));
}

ast_node_t* parseLogAnd(parser_t* p) {
    static char const* const sOps[] = { "&&" };
    return parseBinaryOp(p, &parseBitOr, sOps, sizeof(sOps));
}

ast_node_t* parseBitOr(parser_t* p) {
    static char const* const sOps[] = { "|" };
    return parseBinaryOp(p, &parseBitXor, sOps, sizeof(sOps));
}

ast_node_t* parseBitXor(parser_t* p) {
    static char const* const sOps[] = { "^" };
    return parseBinaryOp(p, &parseBitAnd, sOps, sizeof(sOps));
}

ast_node_t* parseBitAnd(parser_t* p) {
    static char const* const sOps[] = { "&" };
    return parseBinaryOp(p, &parseEquality, sOps, sizeof(sOps));
}

ast_node_t* parseEquality(parser_t* p) {
    static char const* const sOps[] = { "==", "!=" };
    return parseBinaryOp(p, &parseRelational, sOps, sizeof(sOps));
}

ast_node_t* parseRelational(parser_t* p) {
    static char const* const sOps[] = { "<", ">", "<=", ">=" };
    return parseBinaryOp(p, &parseShift, sOps, sizeof(sOps));
}

ast_node_t* parseShift(parser_t* p) {
    static char const* const sOps[] = { "<<", ">>" };
    return parseBinaryOp(p, &parseAdditive, sOps, sizeof(sOps));
}

ast_node_t* parseAdditive(parser_t* p) {
    static char const* const sOps[] = { "+", "-" };
    return parseBinaryOp(p, &parseMultiply, sOps, sizeof(sOps));
}

ast_node_t* parseMultiply(parser_t* p) {
    static char const* const sOps[] = { "*", "/", "%" };
    return parseBinaryOp(p, &parseUnary, sOps, sizeof(sOps));
}

// Unary prefix:
// - ! ~ * & ++ --
ast_node_t* parseUnary(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    if(curType(p) != TOK_Operator) {
        return parsePostfix(p);
    }

    char const* const op = curText(p);
    if(
        strcmp(op, "-") == 0 ||
        strcmp(op, "!") == 0 ||
        strcmp(op, "~") == 0 ||
        strcmp(op, "+") == 0
    ) {
        token_t token = advance(p);
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_UnaryOp, line, col);

        node->op = token.text;
        token.text = NULL;
        freeToken(&token);

        node->left = parseUnary(p);
        return node;
    }
    if(strcmp(op, "*") == 0) {
        token_t token = advance(p);
        freeToken(&token);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Deref, line, col);
        node->left = parseUnary(p);
        return node;
    }
    if(strcmp(op, "&") == 0) {
        token_t token = advance(p);
        freeToken(&token);

        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_AddrOf, line, col);
        node->left = parseUnary(p);
        return node;
    }
    if(strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
        // These are not supported.
        addError(
            p->ctx,
            "[%s] '++' and '--' are invalid operators in the B language.\n",
            __COMPONENT__
        );
        /*
        token_t token = advance(p);
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_UnaryOp, line, col);
        node->op = token.text;
        token.text = NULL;
        freeToken(&token);
        node->left = parseUnary(p);
        return node;
        */
    }
    return parsePostfix(p);
}

// Postfix:
// (...) [...] ++ --
ast_node_t* parsePostfix(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    ast_node_t* base = parsePrimary(p);

    while(1) {
        // Function call
        // expr ( args )
        if(curType(p) == TOK_ParenthesisOpen) {
            // Consume '('
            token_t temp = advance(p);
            freeToken(&temp);

            ast_node_t* call = malloc(sizeof(ast_node_t));
            initAstNode(call, AST_Call, line, col);
            call->left = base;

            while(curType(p) != TOK_ParenthesisClose && curType(p) != TOK_Eof) {
                nodeAddChild(call, parseExpr(p));

                if(curType(p) != TOK_Comma) {
                    break;
                }

                temp = advance(p);
                freeToken(&temp);
            }

            // Consume ')'
            temp = expect(p, TOK_ParenthesisClose);
            freeToken(&temp);
            base = call;
            continue;
        }

        // Array index
        // expr [ expr ]
        if(curType(p) == TOK_SquareBracketOpen) {
            // Consume '['
            token_t temp = advance(p);
            freeToken(&temp);

            ast_node_t* index = malloc(sizeof(ast_node_t));
            initAstNode(index, AST_Index, line, col);
            index->left = base;
            index->right = parseExpr(p);

            // Consume ']'
            temp = expect(p, TOK_SquareBracketClose);
            freeToken(&temp);

            base = index;
            continue;
        }

        // Postfix ++ , --
        if(
            curType(p) == TOK_Operator &&
            (strcmp(curText(p), "++") == 0 || strcmp(curText(p), "--") == 0)
        ) {
            token_t op = advance(p);

            ast_node_t* node = malloc(sizeof(ast_node_t));
            initAstNode(node, AST_PostfixOp, line, col);
            node->op = op.text;
            op.text = NULL;
            freeToken(&op);

            node->left = base;
            base = node;
            continue;
        }

        break;
    }
    return base;
}

ast_node_t* parsePrimary(parser_t* p) {
    int32_t line = curLine(p);
    int32_t col = curCol(p);

    // Parenthesized expression
    if(curType(p) == TOK_ParenthesisOpen) {
        token_t temp = advance(p);
        freeToken(&temp);

        ast_node_t* inner = parseExpr(p);

        token_t close = expect(p, TOK_ParenthesisClose);
        freeToken(&close);

        return inner;
    }

    // Numeric literal
    if(curType(p) == TOK_Number) {
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Number, line, col);
        node->value = p->current.number;
        if(p->current.text != NULL) {
            node->text = strdup(p->current.text);
        }

        token_t temp = advance(p);
        freeToken(&temp);

        return node;
    }

    // String literal
    if(curType(p) == TOK_String) {
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_String, line, col);

        token_t temp = advance(p);
        node->text = temp.text;
        temp.text = NULL;
        freeToken(&temp);

        return node;
    }

    // Identifier
    if(curType(p) == TOK_Identifier) {
        ast_node_t* node = malloc(sizeof(ast_node_t));
        initAstNode(node, AST_Identifier, line, col);

        token_t temp = advance(p);
        node->text = temp.text;
        temp.text = NULL;
        freeToken(&temp);

        return node;
    }

    // Unexpected token
    addError(
        p->ctx,
        "[%s] : Unexpected token %s ('%s') in expression , line %d col %d\n",
        __COMPONENT__,
        getTokenName(curType(p)),
        curText(p),
        line,
        col
    );

    // TODO:
    // Return dummy object instead to keep tree "valid"?
    return NULL;
}


// Top level definitions/helpers

// Definition
// vector       :   name '[' [number] ']' [ ival (',' ival)* ] ';'
// variable     :   name [ ival (',' ival)* ] ';'
// function     :   name '(' [namelist] ')' statement
static ast_node_t* parseDefinition(parser_t* p) {
    if(p->ctx->flags.verbose) {    
        fprintf(stdout, "parseDefinition: curType=%s curText=%s line=%d\n",
            getTokenName(curType(p)),
            curText(p),
            curLine(p)
        );
    }

    int32_t line = curLine(p);
    int32_t col = curCol(p);

    // Top level declarations

    if(checkKeyword(p, "extrn")) {
        return parseExtrnDecl(p);
    }
    if(checkKeyword(p, "auto")){
        return parseAutoDecl(p);
    }

    // Other definitions start with an identifier
    if(curType(p) != TOK_Identifier) {
        addError(
            p->ctx,
            "[%s] : Expected identifier at top level, got %s ('%s') , line %d col %d\n",
            __COMPONENT__,
            getTokenName(curType(p)),
            curText(p),
            line,
            col
        );

        // Skip to semicolon as an attempt to recover.
        // Obviously any further compilation is useless from a functional
        // standpoint, but it's nice to get all (syntax) errors delivered
        // at once instead of exiting at the first one
        uint32_t skipped = 0u;
        while(curType(p) != TOK_Semicolon && curType(p) != TOK_Eof) {
            token_t skip = advance(p);
            freeToken(&skip);
            ++skipped;
        }
        
        if(curType(p) == TOK_Semicolon) {
            addMessage(
                p->ctx,
                "[%s] : Skipped %d tokens to next semicolon. Now at line %d, col %d.\n",
                __COMPONENT__,
                skipped,
                curLine(p),
                curCol(p)
            );
            
            // Consume semicolon too
            token_t t = advance(p);
            freeToken(&t);
        }

        return NULL;
    }

    token_t name = advance(p);

    // Function definition:
    // name '(' ... ')'
    if(curType(p) == TOK_ParenthesisOpen) {
        // Consume '('
        token_t temp = advance(p);
        freeToken(&temp);

        ast_node_t* func = malloc(sizeof(ast_node_t));
        initAstNode(func, AST_FuncDef, line, col);
        // Swap ptrs and zero this one out so freeToken() doesn't touch it
        func->text = name.text;
        name.text = NULL;
        freeToken(&name);

        // Parameters
        while(curType(p) != TOK_ParenthesisClose && curType(p) != TOK_Eof) {

            if(curType(p) == TOK_Identifier) {

                token_t param = advance(p);
                
                func->paramNames = realloc(
                    func->paramNames,
                    sizeof(char*) * (func->numParams + 1)
                );
                // Swap ptrs here too to avoid needless allocations
                func->paramNames[func->numParams++] = param.text;
                param.text = NULL;

                freeToken(&param);

                // Consume eventual comma arg delimiter
                if(curType(p) == TOK_Comma) {
                    token_t comma = advance(p);
                    freeToken(&comma);
                }

            } else {

                addError(
                    p->ctx,
                    "[%s] : Expected identifier in argument list, got %s ('%s') , line %d col %d\n",
                    __COMPONENT__,
                    getTokenName(curType(p)),
                    curText(p),
                    line,
                    col
                );

                token_t skip = advance(p);
                freeToken(&skip);

            }

        }

        temp = expect(p, TOK_ParenthesisClose);
        freeToken(&temp);

        // Function body
        func->left = parseStatement(p);
        return func;
    }

    // Variable def
    ast_node_t* var = malloc(sizeof(ast_node_t));
    initAstNode(var, AST_VarDef, line, col);
    var->text = name.text; // Same trick here as before
    name.text = NULL;
    var->value = 1; // default as single value
    freeToken(&name);

    // Optional vector size:
    // name '[' [number] ']'
    if(curType(p) == TOK_SquareBracketOpen) {
        
        token_t temp = advance(p);
        freeToken(&temp);

        if(curType(p) == TOK_Number) {
            var->value = p->current.number;
            temp = advance(p);
            freeToken(&temp);
        } else {
            var->value = 0; // unspecified size
        }

        temp = expect(p, TOK_SquareBracketClose);
        freeToken(&temp);
    }

    // Optional initializer list:
    while(curType(p) != TOK_Semicolon && curType(p) != TOK_Eof) {
        ast_node_t* ival = parsePrimary(p);
        nodeAddChild(var, ival);
        if(curType(p) == TOK_Comma) {
            token_t t = advance(p);
            freeToken(&t);
        } else {
            break;
        }
    }

    expectSemicolon(p);
    return var;
}


// Public API

void initParser(parser_t* p, lexer_t* lexer, compiler_context_t* ctx) {
    if(p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
    p->lexer = lexer;
    p->current = lexerNext(lexer);
    p->hasPeek = false;
    p->ctx = ctx;
}

void freeParser(parser_t* p) {
    if(p == NULL) {
        return;
    }
    freeToken(&p->current);
    if(p->hasPeek) {
        freeToken(&p->peek);
    }
}

// Parse full program
ast_node_t* parse(parser_t* p) {
    if(p == NULL) {
        return NULL;
    }

    ast_node_t* program = malloc(sizeof(ast_node_t));
    if(program == NULL) {
        fputs("Failed to allocate root node.\n", stderr);
        return NULL;
    }
    initAstNode(program, AST_ProgramRoot, 1, 1);

    while(curType(p) != TOK_Eof) {
        ast_node_t* def = parseDefinition(p);
        if(def != NULL) {
            nodeAddChild(program, def);
        }
    }

    return program;
}