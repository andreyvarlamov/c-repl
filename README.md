# C REPL

Goal: a text editor that can a) run C code from the edited buffer live, as REPL. b) modify its own configuration, etc. with its own exposed C API and user functions written in its edited buffer. Think emacs, but in C.

Prototyping in stages:

- Stage 1: Hack together REPL-like execution of user C code using llvm toolchain.
- Stage 2: Simple text editor
- Stage 3: Editor with REPL
- Stage 4: REPL with TCC
- Stage 5: True JIT in the editor
- Stage X: Switch to back to clang/LLVM and use stuff like ORC JIT?
