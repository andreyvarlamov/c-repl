#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 3) {
	fprintf(stderr, "Usage: execute [file] [expression]\n");
	return -1;
    }

    const char *file_path = argv[1];
    const char *expression = argv[2];

    printf("%s: %s\n", file_path, expression);

    return 0;
}
