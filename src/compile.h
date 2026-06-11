#ifndef COMPILE_H
#define COMPILE_H

#include <stdio.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>
#include "target.h"

typedef struct compiler_flags_s {
    int32_t errorCap; // max errors before forcibly stopping, -1 = no limit
    int32_t warningCap; // max warnings before forcibly stopping, -1 = no limit
    uint32_t numFiles; // number of files
    char** files; // list of each file path
    char* stdlib; // path to compiled stdlib
    bool targetX64; // target 64bit architecture, 32bit otherwise
    bool suppressWarnings; // no warnings
    bool verbose; // verbose output
    bool help; // print help info
    bool windows; // target windows, linux otherwise
    bool noAsm; // don't assemble program
    bool noLink; // don't link program
} compiler_flags_t;

typedef struct compiler_context_s {
    compiler_flags_t flags; // compiler settings
    char const* currentFile; // current file (full path)
    char* fileBuffer; // current file (raw data)
    int32_t errc; // error count
    int32_t warnc; // warning count
    FILE* logFile; // file handle
    target_data_t target;
} compiler_context_t;

inline uint32_t getWordSize(compiler_context_t const* const ctx) {
    uint32_t const word64 = 8u;
    uint32_t const word32 = 4u;
    return ctx->flags.targetX64 == false ? word32 : word64;
}

void addError(compiler_context_t* p, char const* const fmt, ...);
void addWarning(compiler_context_t* p, char const* const fmt, ...);
void addMessage(compiler_context_t* p, char const* const fmt, ...);

bool compile(compiler_context_t* compiler);

#endif