#include "compile.h"
#include "passes/passes.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast_node.h"
#include "passes/scope.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wchar.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#endif

#ifndef _MAX_PATH
#define _MAX_PATH 512u
#endif

static char* const readFile(char const* const filePath) {
    FILE* f = fopen(filePath, "rb");
    if (!f) {
        perror(filePath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char* buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';

    fclose(f);

    return buf;
}

static char *getFilepathWithoutExtension(char const* filePath) {
    char const* dot = strrchr(filePath, '.');

    size_t len = dot
        ? (size_t)(dot - filePath)
        : strlen(filePath);

    char *buf = malloc(len + 1);
    if (!buf) {
        return NULL;
    }

    memcpy(buf, filePath, len);
    buf[len] = '\0';

    return buf;
}

void addError(compiler_context_t* p, char const* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if(p->flags.errorCap != -1 && p->errc++ > p->flags.errorCap) {
        fprintf(stderr, "Error count exceeds cap (%d > %d). Stopping...\n", p->errc, p->flags.errorCap);
        exit(0);
    }
}
void addWarning(compiler_context_t* p, char const* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if( p->flags.warningCap != -1 && p->warnc++ > p->flags.warningCap) {
        fprintf(stderr, "Warning count exceeds cap (%d > %d). Stopping...\n", p->warnc, p->flags.warningCap);
        exit(0);
    }
}
void addMessage(compiler_context_t* p, char const* const fmt, ...) {
    if(!p->flags.verbose) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

static bool compileFile(compiler_context_t* compiler, char const* const file);

bool compile(compiler_context_t* compiler) {
    
    bool success = true;

    for(uint32_t i = 0u; i < compiler->flags.numFiles; ++i) {
        
        success &= compileFile(compiler, compiler->flags.files[i]);
        if(!success) {
            break;
        }

    }

    return success;
}

bool compileFile(compiler_context_t* compiler, char const* const file) {
    bool success = false;

    if(file == NULL) {
        fprintf(stderr, "Invalid file passed to compiler.\n");
        goto end;
    }

    fprintf(stdout, "Reading file: \"%s\" ...\n", file);

    compiler->currentFile = file;
    compiler->fileBuffer = readFile(file);
    if(compiler->fileBuffer == NULL) {
        fprintf(stderr, "Failed to read file: \"%s\"\n", file);
        goto failed_read;
    }

    // Compilation
    {
        lexer_t lexer;
        initLexer(&lexer, compiler->fileBuffer);

        parser_t parser;
        initParser(&parser, &lexer, compiler);

        fputs("Parsing ...\n", stdout);

        ast_node_t* const nodes = parse(&parser);
        if(nodes == NULL) {
            fprintf(stderr, "Fatal error occured when parsing file: \"%s\"", file);
            goto failed_parse;
        }

        if(compiler->flags.verbose) {
            dumpNodes(stdout, nodes);
        }

        // Pass 1: Build symbol table
        symbol_table_t* const symbols = pass1(nodes, compiler);
        if(symbols == NULL) {
            fprintf(stderr, "Fatal error occured when parsing file: \"%s\"", file);
            goto failed_pass1;
        }

        // Pass 2: IR lowering
        ir_result_t* ir = pass2(nodes, symbols, compiler);
        ir_inst_t* const instrs = ir->instrs;
        uint32_t numInstrs = ir->numInstrs;
        if(instrs == NULL) {
            fprintf(stderr, "Fatal error occured when parsing file: \"%s\"\n", file);
            goto failed_pass2;
        }

        // Build output .asm file path and open file handle
        char* filePathNoExt = getFilepathWithoutExtension(file);
        if(filePathNoExt == NULL) {
            fprintf(stderr, "Failed to get base file path for file: \"%s\"\n", file);
            goto failed_base_path;
        }

        char outputAsmPath[_MAX_PATH];
        snprintf(outputAsmPath, _MAX_PATH, "%s.asm", filePathNoExt);
        FILE* outAsmFile = fopen(outputAsmPath, "w");
        if(outAsmFile == NULL) {
            fprintf(stderr, "Failed to open output file: \"%s\"\n", outputAsmPath);
            goto failed_asm_file;
        }

        // Emit global data segment .data (string literals + initialized globals)
        fprintf(outAsmFile, "    %s\n", compiler->target.segmentData);
        for(uint32_t i = 0u; i < ir->numStrings; ++i) {
            fprintf(outAsmFile, "%s: db ", ir->strings[i].label);
            char const* s = ir->strings[i].content;
            // Print string char by char
            for(uint32_t j = 0u; s[j] != '\0'; ++j) {
                if(j > 0u) {
                    fputs(", ", outAsmFile);
                }
                fprintf(outAsmFile, "%d", (int)s[j]);
            }
            // Null terminator
            fputs(", 0\n", outAsmFile);
        }
        for(uint32_t i = 0u; i < nodes->numChildren; ++i) {
            ast_node_t* node = nodes->children[i];
            if(node->type != AST_VarDef) {
                continue;
            }
            if(node->numChildren > 0u) {
                fprintf(outAsmFile, "%s: dq ", node->text);
                for(uint32_t j = 0u; j < node->numChildren; ++j) {
                    if(j > 0u) {
                        fputs(", ", outAsmFile);
                    }
                    fprintf(outAsmFile, "%lld", node->children[j]->value);
                }
                fputs(", 0\n", outAsmFile);
            } else if(node->value != 0) {
                fprintf(outAsmFile, "%s: dq %lld\n", node->text, (int64_t)node->value);
            }
            // If neither of these it goes into .bss
        }
        fputc('\n', outAsmFile);

        // Emit static data segment .bss (uninitialized)
        fprintf(outAsmFile, "    %s\n", compiler->target.segmentBss);
        for(uint32_t i = 0u; i < nodes->numChildren; ++i) {
            ast_node_t* node = nodes->children[i];
            if(node->type != AST_VarDef) {
                continue;
            }
            if(node->numChildren > 0u || node->value != 0) {
                continue;
            }
            uint32_t words = node->value > 0 ? (uint32_t)node->value : 1u;
            fprintf(outAsmFile, "%s: resq %u\n", node->text, words);
        }
        fputs("\n", outAsmFile);

        // Emit symbols (global/extern decl) in .text
        fprintf(outAsmFile, "    %s\n", compiler->target.segmentText);
        for(uint32_t i = 0u; i < symbols->global->numSymbols; ++i) {
            symbol_t* sym = &symbols->global->symbols[i];
            if(sym->type == SYM_Extern) {
                fprintf(outAsmFile, "    extern %s\n", sym->name);
            } else if(sym->type == SYM_Function) {
                fprintf(outAsmFile, "    global %s\n", sym->name);
            }
        }
        fputs("\n", outAsmFile);

        // Pass 3: Code generation
        pass3(instrs, numInstrs, symbols, compiler, outAsmFile);

        fclose(outAsmFile);
        outAsmFile = NULL;

        // Assemble
        if(!compiler->flags.noAsm) {
            if (compiler->flags.verbose) {
                fprintf(stdout, "Assembling ...\n");
            }
            char cmd[1024u];
            snprintf(
                cmd,
                sizeof(cmd),
                "nasm -f %s \"%s\" -o \"%s.o\"",
                getNasmFmtName(compiler->target.format),
                outputAsmPath,
                filePathNoExt
            );

            if (compiler->flags.verbose) {
                fprintf(stdout, "Executing \"%s\" ...\n", cmd);
            }

            if(system(cmd) != 0) {
                fprintf(stderr, "Error: Assembler failed.\n");
                goto failed_asm_link;
            }

            if(compiler->flags.verbose) {
                fputs("Assembling successful\n", stdout);
            }

        } else if (compiler->flags.verbose) {
            fprintf(stdout, "Skipping assembling stage\n");
        }

        // Link
        if(!compiler->flags.noLink) {
            if (compiler->flags.verbose) {
                fprintf(stdout, "Linking ...\n");
            }

            char cmd[1024u];

            char objPath[_MAX_PATH];
            char binPath[_MAX_PATH];
            snprintf(objPath, sizeof(objPath), "%s.o", filePathNoExt);
            snprintf(binPath, sizeof(binPath), "%s%s", filePathNoExt, compiler->target.fileSuffix);

            snprintf(cmd, sizeof(cmd), "gcc %s -o %s", objPath, binPath);

            if (compiler->flags.verbose) {
                fprintf(stdout, "Executing \"%s\" ...\n", cmd);
            }
            
            if(system(cmd) != 0) {
                fprintf(stderr, "Error: Linker failed.\n");
                goto failed_asm_link;
            }

            if(compiler->flags.verbose) {
                fputs("Linking successful\n", stdout);
            }

        } else if (compiler->flags.verbose) {
            fprintf(stdout, "Skipping linking stage\n");
        }

        success = true;

failed_asm_link:
        if(outAsmFile != NULL) {
            fclose(outAsmFile);
            outAsmFile = NULL;
        }

failed_asm_file:
        free(filePathNoExt);

failed_base_path:
        for(uint32_t i = 0u; i < numInstrs; ++i) {
            free(instrs[i].label);
        }
        free(instrs);

failed_pass2:
        freeSymbolTable(symbols);

failed_pass1:
        freeAstNode(nodes);

failed_parse:
        freeParser(&parser);
        freeLexer(&lexer);
    }

failed_read:
    free(compiler->fileBuffer);

end:
    return success;
}