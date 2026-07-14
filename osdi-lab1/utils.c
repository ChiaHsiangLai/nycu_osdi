#include "utils.h"

int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b; // 兩邊要同時走到結尾的 '\0' 才算真的相等
}