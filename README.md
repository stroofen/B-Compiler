## Compiler for the B programming language

### Tribute to the legend 👑 Ken Thompson 👑


### Features
- ELF32/64 + Win32/64 support
- Lexer + parser
- Abstract syntax tree (AST) creation
- Intermediate language (IL) emission
- Several compiler flags (read flags section)

### Extensions
*While the point of the language is to be extremely minimal, it does get a bit overkill due to the advancements of languages the past 56+ years. Hence, it was decided to extend the language to make it slightly more practical.*
- Inline assembly blocks
- Strings are byte arrays instead of word arrays, for the sake of interop with C functions
- Compound assignments
- Else-clause for if-statements

### Compiler flags
| Name | Description | Arguments | Default | Required |
|------|-------------|-----------|---------|----------|
| `-h` / `--help` | Display help information | `None` | `0` | `False` |
| `-x` / `--architecture` | Target architecture | `86` / `64` | `NULL` | `True` |
| `-os` / `--platform` | Target platform | `windows` / `linux` | `NULL` | `True` |
| `-v` / `--verbose` | Print detailed information during compilation | `None` | `False` | `False` |
| `-sw` / `--suppress` | Suppress warnings | `None` | `False` | `False` |
| `-wc` / `--warnings` | Maximum warning count before forcibly exiting | A number, -1 for no cap | `-1` | `False` |
| `-ec` / `--errors` | Maximum error count before forcibly exiting | A number, -1 for no cap | `-1` | `False` |
| `-na` / `--noasm` | Skip assembling stage | `None` | `False` | `False` |
| `-nl` / `--nolink` | Skip linking stage | `None` | `False` | `False` |

### Examples
```
N 10;

main() {
    extrn N;
    extrn fib;
    extrn printf;
    
    auto result;
    result = fib(N);
    printf("Fibonacci of 10 = %d", result);
}

fib(x) {
    auto a, b, i, sum;
    a = 0;
    b = 1;
    i = 1;
    sum = 0;

    while(i++ < x) {
        sum = a + b;
        a = b;
        b = sum;
    }

    return sum;
}
```

### Caveats / known issues
- Compiler will blindly process syntactically invalid code (such as prefix ++/-- operations) without giving a parsing error
- There is no standard library (yet?), instead C interop is upheld by violating some of B's rules (strings are byte arrays and not word arrays)
- Very lightly tested in Windows, not at all tested on Linux
- Errors may break any further compilation, this can be handled by setting the max error flag to 1