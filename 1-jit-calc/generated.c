#include <stdio.h>

int add(int a, int b);
int sub(int a, int b);
int mul(int a, int b);
int div(int a, int b);

int main() {
    int result = add(add(5, sub(233, 89999999)) + sub(11, 8298292892 * mul(12, 22)), 5);
    printf("%d", result);
    return 0;
}
