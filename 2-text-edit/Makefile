main:
	clang -std=c99 -g -Wall -Wextra -Werror main.c third_party/glad/src/glad.c -o bin/text-edit -lglfw -lm -Ithird_party/glad/include -Ithird_party

run: main
	./bin/text-edit
