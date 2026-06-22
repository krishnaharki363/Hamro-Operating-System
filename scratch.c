#include <stdio.h>

static int my_strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

int main() {
    printf("Result: %d\n", my_strcmp("snake", "snake"));
    return 0;
}
