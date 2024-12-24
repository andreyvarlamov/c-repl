main:
	clang -std=c99 -g -Wall -Wextra -Werror main.c third_party/glad/src/glad.c -o bin/text-edit -lglfw -Ithird_party/glad/include

run: main
	./bin/text-edit
