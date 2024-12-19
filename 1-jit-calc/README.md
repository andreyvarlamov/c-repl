### Stage 1: Hack together a "JIT" REPL executor using clang tools

To use:

1. Create a user code c file (example: math.c, crazy.c), with your library functions.
2. Run "jit-calc compile file.c". That code will be compiled to LLVM IR (.ll file for debugging).
3. Run "jit-calc execute". That starts a REPL loop. You can then run any C expression, as long as the result can be assigned to an int.
4. Run "jit-calc clean" to clean all intermediate files (_generated folder).

For now, these are the limitations:

- Your library functions can return and accept any built-in type, but the expression has to be assignable to an int variable.
- REPL has no memory, each execution of an expression is its own process :(
- Your library functions have to have { on the same line, because it uses regex for extracting function declarations.
- It's crazy hacky. Check the source lol.
