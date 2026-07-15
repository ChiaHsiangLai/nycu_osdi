#include "utils.h"
#include "mmio.h"

#define PM_PASSWORD 0x5a000000u
#define PM_RSTC     (MMIO_BASE + 0x0010001c)
#define PM_WDOG     (MMIO_BASE + 0x00100024)

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

unsigned long read_cntfrq(void) {
    unsigned long val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

unsigned long read_cntpct(void) {
    unsigned long val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

// 這個函式會把 val 轉成十進位字串，然後一個字元一個字元呼叫 putc_fn() 把它印出來
void print_udec(unsigned long val, void (*putc_fn)(char)) {
    char digits[20];
    int n = 0;

    if (val == 0) {
        putc_fn('0');
        return;
    }

    while (val > 0) {
        digits[n++] = (char)('0' + (val % 10));
        val /= 10;
    }

    while (n > 0) {
        putc_fn(digits[--n]);
    }
}

void reset(int tick) {
    mmio_write(PM_RSTC, PM_PASSWORD | 0x20);                // 設成 full reset 模式
    mmio_write(PM_WDOG, PM_PASSWORD | (unsigned int)tick);  // 設定看門狗倒數的 tick 數
}