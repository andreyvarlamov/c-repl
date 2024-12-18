jit-calc: bin
	clang -std=c99 -Wall -Wextra -Werror jit_calc.c -o bin/jit-calc

bin:
	mkdir bin

user_code:
	clang -S -emit-llvm user_code.c -o bin/user_code.ll

generated:
	clang -S -emit-llvm generated.c -o bin/generated.ll

run:
	llvm-link -S bin/user_code.ll bin/generated.ll -o bin/combined.ll
	lli bin/combined.ll
	@echo

clean-gen:
	rm bin/combined.ll bin/generated.ll bin/user_code.ll generated.c
