#include "target.h"
#include "compile.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>

static char const* const getOsName(target_type_t type);
static char* getArgRegs(char const** regs, uint32_t numRegs);

void initTargetData(target_data_t* p, compiler_context_t* ctx) {
    if(p == NULL) {
        return;
    }
    if(ctx == NULL) {
        return;
    }
    
    p->segmentText = "section .text";
    p->segmentData = "section .data";
    p->segmentBss = "section .bss";

    if(ctx->flags.windows) {
        p->os = TARGET_Windows;
        p->shadowSpace = 32u;
        p->fileSuffix = ".exe";

        if(ctx->flags.targetX64) {
            p->format = NASM_WIN64;

            p->numArgRegisters = 4u;
            p->argRegisters = malloc(p->numArgRegisters * sizeof(char*));
            p->argRegisters[0u] = strdup("rcx");
            p->argRegisters[1u] = strdup("rdx");
            p->argRegisters[2u] = strdup("r8");
            p->argRegisters[3u] = strdup("r9");

        } else {
            p->format = NASM_WIN32;
            // x86 uses __decl, which puts all args on the stack
            p->numArgRegisters = 0u;
            p->argRegisters = NULL;
        }

    } else {
        p->os = TARGET_Linux;
        p->shadowSpace = 0u;
        p->fileSuffix = "";

        if(ctx->flags.targetX64) {
            p->format = NASMN_ELF64;

            p->numArgRegisters = 6u;
            p->argRegisters = malloc(p->numArgRegisters * sizeof(char*));
            p->argRegisters[0] = strdup("rdi");
            p->argRegisters[1] = strdup("rsi");
            p->argRegisters[2] = strdup("rdx");
            p->argRegisters[3] = strdup("rcx");
            p->argRegisters[4] = strdup("r8");
            p->argRegisters[5] = strdup("r9");

        } else {
            p->format = NASMN_ELF32;
        }

    }


    char* argRegs = getArgRegs(p->argRegisters, p->numArgRegisters);

    fprintf(
        stdout,
        "Target data:\n"
        "OS: %s\n"
        "NASM format: %s\n"
        "Shadow space: %d bytes\n"
        "Registers available for arguments: %s\n"
        "Text segment: %s\n"
        "Data segment: %s\n"
        "BSS segment: %s\n"
        "Executable file extension: %s\n",
        getOsName(p->os),
        getNasmFmtName(p->format),
        p->shadowSpace,
        argRegs,
        p->segmentText,
        p->segmentData,
        p->segmentBss,
        p->fileSuffix
    );

    free(argRegs);

}

void freeTargetData(target_data_t* p) {
    if(p == NULL) {
        return;
    }
    if(p->argRegisters != NULL && p->numArgRegisters != 0u) {
        for(uint32_t i = 0u; i < p->numArgRegisters; ++i) {
            free(p->argRegisters[i]);
        }
        free(p->argRegisters);
    }
    memset(p, 0, sizeof(*p));
}

char const* const getOsName(target_type_t type) {
    switch(type) {
        case TARGET_Windows: return "Windows";
        case TARGET_Linux: return "Linux";
        default: return "Unknown";
    }
}

char const* const getNasmFmtName(nasm_fmt_t type) {
    switch(type) {
        case NASM_WIN32: return "win32";
        case NASM_WIN64: return "win64";
        case NASMN_ELF32: return "elf32";
        case NASMN_ELF64: return "elf64";
        default: return "unknown";
    }
}

// Suboptimally written but it's only ran once on startup when verbose-mode is active
char* getArgRegs(char const** regs, uint32_t numRegs) {
    if(numRegs == 0u) {
        // Unneeded allocation but we do this so we can
        // unconditionally free the string later
        return strdup("[ ]");
    }

    uint32_t bufSize = 4u; // "[ " + " ]"

    for(uint32_t i = 0u; i < numRegs; ++i) {
        bufSize += strlen(regs[i]);
        if(i < numRegs - 1u) {
            bufSize += 3u; // " , "
        }
    }

    // +1 for null byte
    char* const buf = malloc(bufSize + 1u);
    
    char* p = buf;
    
    *p++ = '[';
    *p++ = ' ';
    for(uint32_t i = 0u; i < numRegs; ++i) {
        size_t len = strlen(regs[i]);
        memcpy(p, regs[i], len);

        p += len;

        // " , "
        if(i < numRegs - 1u) {
            *p++ = ' ';
            *p++ = ',';
            *p++ = ' ';
        }
    }
    *p++ = ' ';
    *p++ = ']';
    *p = '\0';

    return buf;
}