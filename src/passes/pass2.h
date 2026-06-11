#ifndef PASS2_H
#define PASS2_H

#include "scope.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct compiler_context_s compiler_context_t;
typedef struct ast_node_s ast_node_t;
typedef struct symbol_table_s symbol_table_t;

typedef enum {
    // Data movement
    IR_Const,           // dst = immediate value (AST_Number)
    IR_Copy,            // dst = src1 (AST_Assign)
    IR_Load,            // dst = src2 (AST_Deref)
    IR_Store,           // *dst = src1 (AST_Assign)
    IR_AddrOf,          // dst = address of symbol (AST_AddrOf)
    IR_GlobalRef,       // dst = address of global label (AST_Identifier)
    IR_GlobalLoad,
    IR_GlobalStore,

    // Arithmetic / logic
    IR_Add,
    IR_Sub,
    IR_Mul,
    IR_Div,
    IR_Mod,
    IR_Neg,
    IR_Not,
    IR_BitNot,
    IR_And,
    IR_Or,
    IR_Xor,
    IR_Shl,
    IR_Shr,

    // Comparison
    IR_Eq,
    IR_Ne,
    IR_Lt,
    IR_Gt,
    IR_Le,
    IR_Ge,

    // Array indexing
    IR_Index,           // dst = base[src1]
    IR_IndexAddr,       // dst = &base[src1]

    // Control flow
    IR_Label,           // label:
    IR_Jmp,             // goto label
    IR_Jz,              // if !src1 goto label
    IR_Jnz,             // if src1 goto label

    // Functions
    IR_Param,           // Push (AST_Call children)
    IR_Call,            // dst = call label, n args (AST_Call)
    IR_Return,          // return src1 (AST_Return)

    // Postfix
    IR_Inc,             // src1++ , dst = old value if postfix, new value if prefix
    IR_Dec,             // src1-- , dst = old value if postfix, new value if prefix

    // Inline asm
    IR_Asm              // Raw text pass-through
} ir_op_t;

typedef struct ir_inst_s {
    ir_op_t op;
    int32_t dst;
    int32_t src1;
    int32_t src2;
    int64_t imm; // for IR_Const
    char* label; // calls, jumps, globals
    uint32_t numArgs; // for IR_Call: how many IR_Param instructions precede
    // these must exist or Pass3 won't know if it's bp-relative or a temp index
    bool dstIsOffset;
    bool src1IsOffset;
    bool src2IsOffset;
} ir_inst_t;

typedef struct string_entry_s {
    char* label; // label name
    char* content; // string literal
} string_entry_t;

typedef struct ir_result_s {
    ir_inst_t* instrs;
    uint32_t numInstrs;
    string_entry_t* strings;
    uint32_t numStrings;
} ir_result_t;

// IR lowering pass: flattens nested expressions into linear instructions
ir_result_t* pass2(ast_node_t const* root, symbol_table_t* symbols, compiler_context_t* ctx);

#endif