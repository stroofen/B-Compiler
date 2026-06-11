#include <string.h>
#include <stdlib.h>
#include "compile.h"
#include "target.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <direct.h>
#elif defined(__linux__)
#include <sys/stat.h>
#endif

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_REVISION 1

static void help(char const* const executable);
static bool init(int argc, char* argv[], compiler_context_t* compiler);
static bool pushFile(char const* const filePath, compiler_context_t* compiler);
static bool collectFiles(char const* const path, compiler_context_t* compiler);

int main(int argc, char* argv[]) {
    compiler_context_t compiler;
    memset(&compiler, 0, sizeof(compiler));

    if(init(argc, argv, &compiler) == false || compiler.flags.help) {
        help(*argv);
        return 0;
    }
    
    fprintf(
        stdout,
        "Compiling %d file%s...\n",
        compiler.flags.numFiles,
        compiler.flags.numFiles > 1
        ? "s"
        : ""
    );

    if(compile(&compiler) == false) {
        fprintf(
            stderr,
            "Compilation process failed with %d errors and %d warnings.\n",
            compiler.errc,
            compiler.warnc
        );
    }

    return 0;
}

void help(char const* const executable) {
    fprintf(
        stdout,
        "B Compiler V%d.%d.%d - build date %s %s\n"
        "Usage: %s [options]\n"
        "-h / --help            | Print help information\n"
        "-x / --architecture    | Target architecture (\"64\" or \"86\")\n"
        "-os / --platform       | Target platform (\"windows\" or \"linux\")\n"
        "-v / --verbose         | Verbose outputs/compiler messages\n"
        "-sw / --suppress       | Suppress warnings\n"
        "-wc / --warnings       | Warning cap (-1 for none)\n"
        "-ec / --errors         | Error cap (-1 for none)\n"
        "-na / --noasm          | Skip assembling procedure\n"
        "-nl / --nolink         | Skip linking procedure\n",
        VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION,
        __DATE__, __TIME__,
        executable
    );
}

bool init(int argc, char* argv[], compiler_context_t* compiler) {
    // Default state
    memset(compiler, 0, sizeof(compiler));
    compiler->flags.errorCap = -1;
    compiler->flags.warningCap = -1;

    if(argc <= 1) {
        compiler->flags.help = true;
        return false;
    }

    bool success = true;

    for(uint32_t i = 1u; i < (uint32_t)argc; ++i) {

        char const* const arg = argv[i];
        
        // Args without a preceding '-' are treated as inputs
        // An input can be either a file or directory. Directories
        // are recursively scanned
        if(arg[0] != '-') {
            if(!collectFiles(arg, compiler)) {
                success = false;
                break;
            }
            continue;
        }

        if(
            strcmp(arg, "-h") == 0 ||
            strcmp(arg, "--help") == 0
        ) {

            compiler->flags.help = true;
            break;

        } else if(
            strcmp(arg, "-x") == 0 ||
            strcmp(arg, "--architecture") == 0
        ) {

            if((int32_t)i >= argc - 1) {
                fprintf(stdout, "Missing parameter for argument\"%s\"\n", arg);
                success = false;
                break;
            }
            char const* const arch = argv[++i];
            if(strcmp(arch, "64") == 0) {
                compiler->flags.targetX64 = true;
            } else if(strcmp(arch, "86") == 0) {
                compiler->flags.targetX64 = false;
            } else {
                fprintf(stdout, "Unrecognized architecture: \"%s\"\n", arch);
                success = false;
                break;
            }

        } else if(
            strcmp("-os", arg) == 0 ||
            strcmp("--platform", arg) == 0
        ) {
            
            if((int32_t)i >= argc - 1) {
                fprintf(stdout, "Missing parameter for argument\"%s\"\n", arg);
                success = false;
                break;
            }
            char const* const os = argv[++i];
            if(strcmp(os, "windows") == 0) {
                compiler->flags.windows = true;
            } else if(strcmp(os, "linux") == 0) {
                compiler->flags.windows = false;
            } else {
                fprintf(stdout, "Unrecognized platform: \"%s\"\n", os);
                success = false;
                break;
            }

        } else if(
            strcmp("-sl", arg) == 0 ||
            strcmp("--stdlib", arg) == 0
        ) {
        
            if((int32_t)i >= argc - 1) {
                fprintf(stdout, "Missing parameter for argument \"%s\"\n", arg);
                success = false;
                break;
            }

            compiler->flags.stdlib = argv[++i];

        } else if(
            strcmp("-sw", arg) == 0 ||
            strcmp("--suppress", arg) == 0
        ) {

            compiler->flags.suppressWarnings = true;

        } else if(
            strcmp("-wc", arg) == 0 ||
            strcmp("--warnings", arg) == 0
        ) {

            if((int32_t)i >= argc - 1) {
                fprintf(stdout, "Missing parameter for argument \"%s\"\n", arg);
                success = false;
                break;
            }
            
            compiler->flags.warningCap = atoi(argv[++i]);
        } else if(
            strcmp("-ec", arg) == 0 ||
            strcmp("--errors", arg) == 0
        ) {

            if((int32_t)i >= argc - 1) {
                fprintf(stdout, "Missing parameter for argument \"%s\"\n", arg);
                success = false;
                break;
            }
            
            compiler->flags.errorCap = atoi(argv[++i]);

        } else if(
            strcmp("-na", arg) == 0 ||
            strcmp("--noasm", arg) == 0
        ) {
        
            compiler->flags.noAsm = true;

        } else if(
            strcmp("-nl", arg) == 0 ||
            strcmp("--nolink", arg) == 0
        ) {
        
            compiler->flags.noLink = true;

        } else if(
            strcmp("-v", arg) == 0 ||
            strcmp("--verbose", arg) == 0
        ) {
        
            compiler->flags.verbose = true;

        } else {

            fprintf(stdout, "Unrecognized argument: %s\n", arg);
            success = false;
            break;

        }

    }

    if(success) {
        fprintf(stdout, "Initializing target info...\n");
        initTargetData(&compiler->target, compiler);
    } else {
        fprintf(stderr, "Initialization failed\n");
    }

    return success;
}

bool pushFile(char const* const filePath, compiler_context_t* compiler) {
    size_t newSize = (compiler->flags.numFiles + 1) * sizeof(char*);
    char** newFiles = realloc(compiler->flags.files, newSize);
    if(newFiles == NULL) {
        fputs("Out of memory growing file list.\n", stderr);
        return false;
    }

    compiler->flags.files = newFiles;
    
    compiler->flags.files[compiler->flags.numFiles] = strdup(filePath);
    if(!compiler->flags.files[compiler->flags.numFiles]) {
        fputs("Out of memory duplicating file path.\n", stderr);
        return false;
    }

    ++compiler->flags.numFiles;
    return true;
}

bool collectFiles(char const* const path, compiler_context_t* compiler) {
    static char const* const sExtension = ".b";

    #if defined(_WIN32)

    // Append "\*" to path for FindFirstFile
    size_t const pathLen = strlen(path);
    
    // Build the search pattern "path\*" to query the path's attributes
    // +3 for '\', '*', and NUL
    char* searchPath = malloc(pathLen + 3);
    if(!searchPath) {
        fputs("Out of memory building search path\n", stderr);
        return false;
    }
    memcpy(searchPath, path, pathLen);
    searchPath[pathLen]     = '\\';
    searchPath[pathLen + 1] = '*';
    searchPath[pathLen + 2] = '\0';

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    free(searchPath);

    if(hFind == INVALID_HANDLE_VALUE) {
        DWORD const err = GetLastError();
        if(err == ERROR_FILE_NOT_FOUND) {
            // Not a directory: check if path itself is a .b file
            size_t const extLen = strlen(sExtension);
            if(pathLen >= extLen && strcmp(path + pathLen - extLen, sExtension) == 0) {
                if(compiler->flags.verbose) {
                    printf("Adding file: %s\n", path);
                }
                return pushFile(path, compiler);
            }
            return true;
        }
        fprintf(
            stderr,
            "FindFirstFileA() failed for path '%s' (error %lu)\n",
            path,
            err
        );
        return false;
    }

    // FindFirstFile succeeded, so path is a directory: recurse into it
    bool ok = true;
    do {
        // Skip "." and ".."
        if(strcmp(findData.cFileName, ".")  == 0 ||
           strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        size_t const childLen = strlen(findData.cFileName);
        // +2 for '\' separator and NUL
        char* child = malloc(pathLen + childLen + 2);
        if(!child) {
            fprintf(stderr, "Out of memory building path\n");
            ok = false;
            break;
        }
        memcpy(child, path, pathLen);
        child[pathLen] = '\\';
        memcpy(child + pathLen + 1, findData.cFileName, childLen + 1);

        if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ok = collectFiles(child, compiler);
        } else {
            // Regular file - check extension
            if(childLen >= 2 &&
               strcmp(child + pathLen + 1 + childLen - 2, sExtension) == 0) {
                if(compiler->flags.verbose) {
                    printf("Adding file: %s\n", child);
                }
                ok = pushFile(child, compiler);
            }
        }
        free(child);
    } while(ok && FindNextFileA(hFind, &findData));

    FindClose(hFind);

    return ok;

    #elif defined(__linux__)

    struct stat st;
    if(stat(path, &st) != 0) {
        fprintf(stderr, "stat() failed for path '%s'\n", path);
        return false;
    }

    if (S_ISREG(st.st_mode)) {
        // Accept only files whose name ends with ".b"
        size_t const len = strlen(path);
        if (len < 2) {
            return false;
        }

        char const* const fileExt = path + len - 2;
        if(strcmp(fileExt, sExtension) != 0) {
            return false;
        }
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(path);
        if (!dir) {
            fprintf(stderr, "Cannot open directory '%s'\n", path);
            return false;
        }

        bool ok = true;
        struct dirent* entry;
        while (ok && (entry = readdir(dir)) != NULL) {

            // Skip the "." and ".." pseudo-entries
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            /* Build the child path: "parent/child" */
            size_t parent_len = strlen(path);
            size_t child_len  = strlen(entry->d_name);
            /* +2 for the '/' separator and the NUL terminator */
            char* child = malloc(parent_len + child_len + 2);
            if (!child) {
                fprintf(stderr, "Out of memory building path\n");
                ok = false;
                break;
            }
            memcpy(child, path, parent_len);
            child[parent_len] = '/';
            memcpy(child + parent_len + 1, entry->d_name, child_len + 1);

            ok = collectFiles(flags, child);
            free(child);
        }
        closedir(dir);
        return ok;
    }

    return true;

    #endif
}