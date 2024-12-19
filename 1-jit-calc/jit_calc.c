#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <regex.h>
#include <unistd.h>

#define MAX_FUNCTIONS 128

void usage_and_error();
void validate_args(int argc, char **argv);

void mode_compile(const char *file_name);
void mode_execute();
void mode_clean();

void copy_file(const char *src, const char *dest);

void eval_expression(const char *expression);
char *read_whole_file(const char *file_name);
char **extract_function_declarations(const char *input, int *out_function_count);
void generate_executing_code(const char *file_name, char **function_declarations, int function_count, const char *expression);

void compile(const char *file_name, const char *compiled_file_name);
void run_lli(const char *user_code_file, const char *generated_file, const char *linked_file_name);

int main(int argc, char **argv) {
    validate_args(argc, argv);

    const char *mode = argv[1];
    if (strcmp(mode, "compile") == 0) {
	const char *file_name = argv[2];
	mode_compile(file_name);
    } else if (strcmp(mode, "execute") == 0) {
	mode_execute();
    } else if (strcmp(mode, "clean") == 0) {
	mode_clean();
    }

    return 0;
}

void usage_and_error() {
    fprintf(stderr, ("Usage: jit-calc compile file\n"
		     "       jit-calc execute\n"
		     "       jit-calc clean\n"));
    exit(1);
}

void validate_args(int argc, char **argv) {
    if (argc < 2) {
	usage_and_error();
    }

    const char *mode = argv[1];

    if (strcmp(mode, "compile") == 0) {
	if (argc != 3) {
	    usage_and_error();
	}
    } else if (strcmp(mode, "execute") == 0 || strcmp(mode, "clean") == 0) {
	if (argc != 2) {
	    usage_and_error();
	}
    }
}

void mode_compile(const char *file_name) {
    if (mkdir("_generated", 0755) != 0) {
	mode_clean();
	if (mkdir("_generated", 0755) != 0) {
	    perror("Failed to make _generated dir.");
	    exit(1);
	}
    }

    copy_file(file_name, "_generated/user_code.c");

    compile("_generated/user_code.c", "_generated/user_code.ll");
}

static char stdin_buffer[1024 * 1024];

void mode_execute() {
    while(true) {
	printf(">> ");
	if (fgets(stdin_buffer, sizeof(stdin_buffer), stdin) == NULL) {
	    break;
	}

	size_t len = strlen(stdin_buffer);
	if (len > 0 && stdin_buffer[len] == '\n') {
	    stdin_buffer[len] = '\0';
	}

	eval_expression(stdin_buffer);
    }
}

void mode_clean() {
    if (unlink("_generated/generated.c") != 0) {
	perror("Failed to remove _generated/generated.c");
    }
    if (unlink("_generated/user_code.c") != 0) {
	perror("Failed to remove _generated/user_code.c");
    }
    if (unlink("_generated/generated.ll") != 0) {
	perror("Failed to remove _generated/generated.ll");
    }
    if (unlink("_generated/user_code.ll") != 0) {
	perror("Failed to remove _generated/user_code.ll");
    }
    if (unlink("_generated/combined.ll") != 0) {
	perror("Failed to remove _generated/combined.ll");
    }

    if (rmdir("_generated") != 0) {
	perror("Failed to remove _generated");
	exit(1);
    }

    printf("INFO: Cleaned generated files.\n");
}


void copy_file(const char *src, const char *dest) {
    char command[256];
    snprintf(command, sizeof(command), "cp %s %s", src, dest);
    if (system(command) != 0) {
	perror("Copy failed");
	exit(1);
    }
}

void eval_expression(const char *expression) {
    char *file_contents = read_whole_file("_generated/user_code.c");

    int function_count = 0;
    char **function_declarations = extract_function_declarations(file_contents, &function_count);

    generate_executing_code("_generated/generated.c", function_declarations, function_count, expression);

    compile("_generated/generated.c", "_generated/generated.ll");
    run_lli("_generated/generated.ll", "_generated/user_code.ll", "_generated/combined.ll");

    for (int i = 0; i < function_count; i++) {
	free(function_declarations[i]);
    }
    free(function_declarations);
    free(file_contents);
}

char *read_whole_file(const char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
	fprintf(stderr, "ERROR: Failed to open file %s\n", file_name);
	exit(1);
    }
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    char *file_contents = malloc(file_size + 1);
    if (file_contents == NULL) {
	fprintf(stderr, "ERROR: Failed to allocate buffer for file %s\n", file_name);
	fclose(file);
	exit(1);
    }
    fread(file_contents, 1, file_size, file);
    file_contents[file_size] = '\0';
    fclose(file);

    return file_contents;
}

char **extract_function_declarations(const char *input, int *out_function_count) {
    /* printf("\nExtracting function declarations...\n"); */
    regex_t regex_state;
    regmatch_t match[2];

    const char *pattern = "([a-zA-Z_][a-zA-Z0-9_\\s\\*]+\\s+[a-zA-Z_][a-zA-Z0-9_]*\\s*\\([^)]*\\))\\s*\\{";

    if (regcomp(&regex_state, pattern, REG_EXTENDED | REG_NEWLINE) != 0) {
	fprintf(stderr, "ERROR: Failed to compile regex: %s\n", pattern);
	exit(1);
    }

    char **functions = malloc(MAX_FUNCTIONS * sizeof(char *));
    if (functions == NULL) {
	fprintf(stderr, "ERROR: Failed to alloc for function declaration array\n");
	exit(1);
    }

    int function_count = 0;

    const char *cursor = input;
    while (regexec(&regex_state, cursor, 2, match, REG_NOTBOL | REG_NOTEOL) == 0) {
	int match_start = match[1].rm_so;
	int match_end = match[1].rm_eo;
	int match_length = match_end - match_start;

	functions[function_count] = malloc(match_length + 1);
	if (functions[function_count] == NULL) {
	    fprintf(stderr, "ERROR: Failed to alloc for function declaration\n");
	    for (int i = 0; i < function_count; i++) {
		free(functions[i]);
	    }
	    free(functions);
	    exit(1);
	}

	functions[function_count] = strncpy(functions[function_count], cursor + match_start, match_length);
	functions[function_count][match_length] = '\0';

	/* printf("INFO: Found: %s\n", functions[function_count]); */

	cursor += match_end;

	function_count++;

	if (*cursor == '\0') {
	    break;
	}
    }

    /* printf("INFO: %d functions found.\n", function_count); */
    regfree(&regex_state);

    *out_function_count = function_count;
    return functions;
}

void generate_executing_code(const char *file_name, char **function_declarations, int function_count, const char *expression) {
    /* printf("INFO: Generating executing code...\n"); */
    FILE *file = fopen(file_name, "w");
    if (file == NULL) {
	fprintf(stderr, "ERROR: Failed to open %s for writing.", file_name);
	exit(1);
    }

    fprintf(file, "#include <stdio.h>\n\n");
    for (int i = 0; i < function_count; i++) {
	fprintf(file, "%s;\n", function_declarations[i]);
    }
    fprintf(file, "\nint main() {\n");
    fprintf(file, "    int result = %s;\n", expression);
    fprintf(file, "    printf(\"%%d\", result);\n");
    fprintf(file, "    return 0;\n");
    fprintf(file, "}\n");

    fclose(file);

    /* printf("INFO: Done!\n"); */
}

void compile(const char *file_name, const char *compiled_file_name) {
    char command[256];
    snprintf(command, sizeof(command), "clang -S -emit-llvm %s -o %s", file_name, compiled_file_name);
    int ret = system(command);
    if (ret != 0) {
	fprintf(stderr, "\"%s\" failed with error code %d\n", command, ret);
	exit(1);
    }
}

void run_lli(const char *user_code_file, const char *generated_file, const char *linked_file_name) {
    char command[256];
    snprintf(command, sizeof(command), "llvm-link -S %s %s -o %s", user_code_file, generated_file, linked_file_name);
    int ret = system(command);
    if (ret != 0) {
	fprintf(stderr, "\"%s\" failed with error code %d\n", command, ret);
	exit(1);
    }
    snprintf(command, sizeof(command), "lli %s", linked_file_name);
    ret = system(command);
    if (ret != 0) {
	fprintf(stderr, "\"%s\" failed with error code %d\n", command, ret);
	exit(1);
    }
    printf("\n");
}
