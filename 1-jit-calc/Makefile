execute: bin
	clang -std=c99 -Wall -Wextra -Werror execute.c -o bin/execute

bin:
	mkdir bin

math:
	clang -S -emit-llvm math.c -o bin/math.ll

generated:
	clang -S -emit-llvm generated.c -o bin/generated.ll

run:
	llvm-link -S bin/math.ll bin/generated.ll -o bin/combined.ll
	lli bin/combined.ll
	@echo
