#ifndef TARGET_H
#define TARGET_H

#include <stdint.h>

typedef struct compiler_context_s compiler_context_t;

typedef enum {
    TARGET_Windows,
    TARGET_Linux
} target_type_t;

typedef enum {
    NASM_WIN32,
    NASMN_ELF32,
    NASM_WIN64,
    NASMN_ELF64,
} nasm_fmt_t;

typedef struct target_data_s {
    target_type_t os;
    // Platform-specific
    uint32_t shadowSpace;
    char** argRegisters;
    uint32_t numArgRegisters;
    // Assembly segments
    char const* segmentText;
    char const* segmentData;
    char const* segmentBss;
    // assembler output
    nasm_fmt_t format;
    char const* fileSuffix;
} target_data_t;

void initTargetData(target_data_t* p, compiler_context_t* ctx);
void freeTargetData(target_data_t* p);

char const* const getNasmFmtName(nasm_fmt_t type);

#endif