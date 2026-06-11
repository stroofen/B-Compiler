#include "pass3.h"
#include "pass2.h"
#include "pass1.h"
#include "../compile.h"
#include <string.h>

#define __COMPONENT__ "Compiler Pass 3"

#define NUM_ARG_REGS 6u

typedef struct cpu_regs_s {
    char bp[4u]; // rbp / ebp
    char ax[4u]; // rax / eax
    char sp[4u]; // rsp / esp
    char bx[4u]; // rbx / ebx
    char dx[4u]; // rdx / edx
} cpu_regs_t;

typedef struct emit_state_s {
    uint32_t localSize;
} emit_state_t;

static uint32_t localScopeSize(symbol_table_t const* table, char const* const funcName);
static int32_t tempSlot(int32_t temp, uint32_t localSize);
static int32_t resolveOp(int32_t val, bool isOffset, uint32_t localSize);
static uint32_t computeFrameSize(
    ir_inst_t const* instrs,
    uint32_t start,
    uint32_t numInstrs,
    symbol_table_t const* symbols
);
static void emitInstr(
    ir_inst_t const* inst,
    symbol_table_t const* symbols,
    cpu_regs_t const* regs, 
    ir_inst_t const* instrs,
    uint32_t numInstrs,
    uint32_t index,
    compiler_context_t* ctx,
    emit_state_t* state,
    FILE* out
);

void pass3(ir_inst_t const* instrs, uint32_t const numInstrs, symbol_table_t const* symbols, compiler_context_t* ctx, FILE* out) {
    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Executing...\n",
            __COMPONENT__
        );
    }

    cpu_regs_t regs;
    memset(&regs, 0, sizeof(regs));
    if(ctx->flags.targetX64) {
        strcpy(regs.bp, "rbp");
        strcpy(regs.ax, "rax");
        strcpy(regs.sp, "rsp");
        strcpy(regs.bx, "rbx");
        strcpy(regs.dx, "rdx");
    } else {
        strcpy(regs.bp, "ebp");
        strcpy(regs.ax, "eax");
        strcpy(regs.sp, "esp");
        strcpy(regs.bx, "ebx");
        strcpy(regs.dx, "edx");
    }

    emit_state_t state;
    state.localSize = 0u;

    for(uint32_t i = 0u; i < numInstrs; ++i) {
        emitInstr(&instrs[i], symbols, &regs, instrs, numInstrs, i, ctx, &state, out);
    }

    if(ctx->flags.verbose) {
        fprintf(
            stdout,
            "[%s] : Finished\n",
            __COMPONENT__
        );
    }
}

uint32_t localScopeSize(symbol_table_t const* table, char const* const funcName) {
    symbol_t* sym = lookupSymbol(table->global, funcName);
    
    if(sym == NULL || sym->scope == NULL) {
        return 0;
    }
    scope_t* scope = sym->scope;
    int32_t lowest = 0;

    for(uint32_t i = 0u; i < scope->numSymbols; ++i) {
        if(scope->symbols[i].offset < lowest) {
            lowest = scope->symbols[i].offset;
        }
    }

    // Offset is always negative
    return (uint32_t)(lowest * -1);
}

// Rounds to negative multiple of 8 (starting at 1)
int32_t tempSlot(int32_t temp, uint32_t localSize) {
    return -((int32_t)localSize + (temp + 1) * 8);
}

int32_t resolveOp(int32_t val, bool isOffset, uint32_t localSize) {
    if(isOffset){
        return val; // rbp rel
    }
    return tempSlot(val, localSize);
}

uint32_t computeFrameSize(
    ir_inst_t const* instrs,
    uint32_t start,
    uint32_t numInstrs,
    symbol_table_t const* symbols
) {
    uint32_t maxTemp = 0u;
    
    for(uint32_t i = start + 1u; i < numInstrs; ++i) {
        ir_inst_t const* inst = &instrs[i];
        // Next function entry
        if(inst->op == IR_Label && (inst->label == NULL || inst->label[0] != '.')) {
            break;
        }
        if((int32_t)maxTemp < inst->dst) {
            maxTemp = inst->dst;
        }
        if((int32_t)maxTemp < inst->src1) {
            maxTemp = inst->src1;
        }
        if((int32_t)maxTemp < inst->src2) {
            maxTemp = inst->src2;
        }
    }

    // Each temporary needs one word, add to each local from the symbol table
    uint32_t localSize = localScopeSize(symbols, instrs[start].label);
    uint32_t tempSize = (maxTemp + 1) * 8;
    uint32_t total = localSize + tempSize;

    // Round up to 16 byte alignment
    return (total + 15) & ~15;
}

void emitInstr(
    ir_inst_t const* inst,
    symbol_table_t const* symbols,
    cpu_regs_t const* regs,
    ir_inst_t const* instrs,
    uint32_t numInstrs,
    uint32_t index,
    compiler_context_t* ctx,
    emit_state_t* state,
    FILE* out
) {
    fprintf(
        stdout,
        "emitInstr: op=%d dst=%d src1=%d src2=%d label=%s\n",
        inst->op, inst->dst, inst->src1, inst->src2,
        inst->label ? inst->label : "<null>"
    );

    static char const* const sInstAdd = "add";
    static char const* const sInstSub = "sub";
    static char const* const sInstMul = "imul";
    
    static char const* const sDword = "dword";
    static char const* const sQword = "qword";

    switch(inst->op) {
        case IR_Label: {
            fprintf(out, "%s:\n", inst->label);

            if(inst->label[0] != '.') {
                state->localSize = localScopeSize(symbols, inst->label);
                // Function prologue
                uint32_t frameSize = 0u;
                frameSize += computeFrameSize(instrs, index, numInstrs, symbols);
                frameSize += ctx->target.shadowSpace;
                fprintf(out, "    push %s\n", regs->bp);
                fprintf(out, "    mov %s, %s\n", regs->bp, regs->sp);
                fprintf(out, "    sub %s, %u\n", regs->sp, frameSize);

                // Spill parameters from registers to their stack slots
                symbol_t* funcSym = lookupSymbol(symbols->global, inst->label);
                if(funcSym && funcSym->scope) {
                    uint32_t paramIdx = 0u;
                    for(uint32_t i = 0u; i < funcSym->scope->numSymbols; ++i) {
                        symbol_t* sym = &funcSym->scope->symbols[i];
                        if(sym->type != SYM_Param) {
                            continue;
                        }
                        if(paramIdx < ctx->target.numArgRegisters) {
                            fprintf(out, "    mov [%s%+d], %s\n",
                                regs->bp,
                                sym->offset,
                                ctx->target.argRegisters[paramIdx]);
                        }
                        paramIdx++;
                    }
                }
            }

            break;
        }
        case IR_Const: {
            fprintf(out, "    mov qword [%s%+d], %lld\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), (int64_t)inst->imm);
            break;
        }
        case IR_Return: {
            if(inst->src1 >= 0) {
                fprintf(out,"    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            }
            fprintf(out,"    mov %s, %s\n", regs->sp, regs->bp);
            fprintf(out,"    pop %s\n", regs->bp);
            fputs("    ret\n", out);
            break;
        }
        case IR_Copy: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Load: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov %s, [%s]\n", regs->ax, regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Store: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov %s, [%s%+d]\n", regs->bx, regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize));
            fprintf(out, "    mov [%s], %s\n", regs->bx, regs->ax);
            break;
        }
        case IR_AddrOf: {
            symbol_t* sym = lookupSymbol(symbols->current, inst->label);
            if(sym != NULL && (sym->type == SYM_Local || sym->type == SYM_Param)) {
                fprintf(out, "    lea %s, [%s%+d]\n", regs->ax, regs->bp, sym->offset);
            } else {
                fprintf(out, "    lea %s, [rel %s]\n", regs->ax, inst->label);
            }
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_GlobalRef: {
            fprintf(out, "    lea %s, [rel %s]\n", regs->ax, inst->label);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Index: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov %s, [%s%+d]\n", regs->bx, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fprintf(out, "    imul %s, %d\n", regs->bx, (int32_t)getWordSize(ctx));
            fprintf(out, "    add %s, %s\n", regs->ax, regs->bx);
            fprintf(out, "    mov %s, [%s]\n", regs->ax, regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Add:
        case IR_Sub:
        case IR_Mul: {
            char const* const instStr   = inst->op == IR_Add
                                        ? sInstAdd
                                        : inst->op == IR_Sub
                                        ? sInstSub
                                        : sInstMul;
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    %s %s, [%s%+d]\n", instStr, regs->ax, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Div:
        case IR_Mod: {
            char const* const wordStr   = ctx->flags.targetX64
                                        ? sQword
                                        : sDword;
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fputs("    cqo\n", out);
            fprintf(out, "    idiv %s [%s%+d]\n", wordStr, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            if(inst->op == IR_Div) {
                fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            } else {
                fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->dx);
            }
            break;
        }
        case IR_Neg: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    neg %s\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_And:
        case IR_Or:
        case IR_Xor: {
            char const* const opStr =
                inst->op == IR_And ? "and" :
                inst->op == IR_Or  ? "or"  : "xor";
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    %s %s, [%s%+d]\n", opStr, regs->ax, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_BitNot: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    not %s\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Shl: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov %s, [%s%+d]\n", regs->bx, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fputs("    mov cl, bl\n", out);
            fprintf(out, "    shl %s, cl\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Shr: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    mov %s, [%s%+d]\n", regs->bx, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fprintf(out, "    mov cl, bl\n");
            fprintf(out, "    sar %s, cl\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Not: {
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    test %s, %s\n", regs->ax, regs->ax);
            fprintf(out, "    sete al\n");
            fprintf(out, "    movzx %s, al\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Eq:
        case IR_Ne:
        case IR_Lt:
        case IR_Gt:
        case IR_Le:
        case IR_Ge: {
            char const* const opStr =
                inst->op == IR_Eq ? "sete" :
                inst->op == IR_Ne ? "setne" :
                inst->op == IR_Lt ? "setl" :
                inst->op == IR_Gt ? "setg" :
                inst->op == IR_Le ? "setle" :
                "setge";
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    cmp %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src2, inst->src2IsOffset, state->localSize));
            fprintf(out, "    %s al\n", opStr);
            fprintf(out, "    movzx %s, al\n", regs->ax);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_Jmp: {
            fprintf(out, "    jmp %s\n", inst->label);
            break;
        }
        case IR_Jz:
        case IR_Jnz: {
            char const* const opStr = inst->op == IR_Jz
                                    ? "jz"
                                    : "jnz";
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, resolveOp(inst->src1, inst->src1IsOffset, state->localSize));
            fprintf(out, "    test %s, %s\n", regs->ax, regs->ax);
            fprintf(out, "    %s %s\n", opStr, inst->label);
            break;
        }
        case IR_Param: {
            // This is handled in IR_Call
            break;
        }
        case IR_Call: {
            /*
            uint32_t const numArgRegs = ctx->target.numArgRegisters;
            uint32_t const paramBase = index - inst->numArgs;

            // Shadow space (Windows ABI support)
            //if(ctx->target.shadowSpace > 0u) {
            //    fprintf(out, "    sub %s, %u\n", regs->sp, ctx->target.shadowSpace);
            //}

            // Walk instrs to find previous IR_Param inst
            for(uint32_t i = 0u; i < inst->numArgs && i < numArgRegs; ++i) {
                ir_inst_t const* param = &instrs[paramBase + i];
                fprintf(out, "    mov %s, [%s%+d]\n", ctx->target.argRegisters[i], regs->bp, tempSlot(param->src1));
            }
            // Overflowing args (> numArgRegs) go onto stack (right to left)
            if(inst->numArgs > numArgRegs) {
                for(uint32_t i = inst->numArgs - 1u; i >= numArgRegs; --i) {
                    ir_inst_t const* param = &instrs[paramBase + i];
                    fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, tempSlot(param->src1));
                    fprintf(out, "    push %s\n", regs->ax);
                }
            }

            fprintf(out, "    call %s\n", inst->label);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, tempSlot(inst->dst), regs->ax);

            // Clean any args that were pushed onto the stack
            uint32_t stackClean = 0;//ctx->target.shadowSpace
            if (inst->numArgs > numArgRegs) {
                // Compute full param size on stack and add it all at once
                stackClean += (inst->numArgs - numArgRegs) * getWordSize(ctx);
            }
            if (stackClean > 0u) {
                fprintf(out, "    add %s, %u\n", regs->sp, stackClean);
            }
            */
           uint32_t const numArgRegs = ctx->target.numArgRegisters;
    
            // collect IR_Param instructions by scanning backwards
            uint32_t paramIndices[32];  // assuming max 32 args
            uint32_t paramCount = 0u;
            
            for(int32_t j = (int32_t)index - 1; 
                j >= 0 && paramCount < inst->numArgs; 
                --j) {
                if(instrs[j].op == IR_Param) {
                    // store in reverse, we'll reverse again below
                    paramIndices[paramCount++] = (uint32_t)j;
                }
            }
            
            // paramIndices is now in reverse order, so iterate backwards
            // to get them left-to-right
            for(uint32_t i = 0u; i < paramCount && i < numArgRegs; ++i) {
                uint32_t pi = paramIndices[paramCount - 1u - i];
                fprintf(out, "    mov %s, [%s%+d]\n",
                    ctx->target.argRegisters[i],
                    regs->bp,
                    resolveOp(instrs[pi].src1, instrs[pi].src1IsOffset, state->localSize));
            }
            
            // stack args right to left
            if(paramCount > numArgRegs) {
                for(uint32_t i = paramCount - 1u; i >= numArgRegs; --i) {
                    uint32_t pi = paramIndices[paramCount - 1u - i];
                    fprintf(out, "    mov %s, [%s%+d]\n",
                        regs->ax, regs->bp, resolveOp(instrs[pi].src1, inst->src1IsOffset, state->localSize));
                    fprintf(out, "    push %s\n", regs->ax);
                }
            }
            
            fprintf(out, "    call %s\n", inst->label);
            fprintf(out, "    mov [%s%+d], %s\n",
                regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            
            if(inst->numArgs > numArgRegs) {
                uint32_t stackClean = (inst->numArgs - numArgRegs) * getWordSize(ctx);
                fprintf(out, "    add %s, %u\n", regs->sp, stackClean);
            }
            break;
        }
        case IR_Asm: {
            fprintf(out, "%s\n", inst->label);
            break;
        }
        case IR_GlobalLoad: {
            fprintf(out, "    mov %s, [rel %s]\n", regs->ax, inst->label);
            fprintf(out, "    mov [%s%+d], %s\n", regs->bp, resolveOp(inst->dst, inst->dstIsOffset, state->localSize), regs->ax);
            break;
        }
        case IR_GlobalStore: {
            int32_t src = resolveOp(inst->src1, inst->src1IsOffset, state->localSize);
            fprintf(out, "    mov %s, [%s%+d]\n", regs->ax, regs->bp, src);
            fprintf(out, "    mov [rel %s], %s\n", inst->label, regs->ax);
            break;
        }
        default: {
            addError(
                ctx,
                "[%s] : Unrecognized instruction: ptr='%p', op='%d', dst='%d', src1='%d', src2='%d', imm='%lld', label='%s', numArgs='%u'\n",
                __COMPONENT__,
                inst,
                inst->op,
                inst->dst,
                inst->src1,
                inst->src2,
                inst->imm,
                inst->label,
                inst->numArgs
            );
            break;
        }
    }
}