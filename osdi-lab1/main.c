#include "uart.h"
#include "utils.h"

#define LINE_MAX 128

static void print_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help   - print all available commands\n");
    uart_puts("  hello  - print Hello World!\n");
}

static void print_timestamp(void) {
    unsigned long freq = read_cntfrq();
    unsigned long count = read_cntpct();

    unsigned long seconds = count / freq;
    unsigned long remainder = count % freq;
    unsigned long micros = (remainder * 1000000UL) / freq; // 避免用浮點數，改用整數運算湊出微秒

    uart_puts("timestamp: ");
    print_udec(seconds, uart_putc);
    uart_puts(".");
    print_udec(micros, uart_putc);
    uart_puts(" s (since boot)\n");
}

static void handle_command(const char *cmd) {
    if (str_eq(cmd, "")) {
        return; // 使用者直接按 Enter，什麼都不做
    } else if (str_eq(cmd, "help")) {
        print_help();
    } else if (str_eq(cmd, "hello")) {
        uart_puts("Hello World!\n");
    } else if (str_eq(cmd, "timestamp")) {
        print_timestamp();
    } else if (str_eq(cmd, "reboot")) {
        uart_puts("Rebooting... (only works on real rpi3, not QEMU)\n");
        reset(100);
    } else {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_puts("\n");
        uart_puts("Type 'help' to see available commands.\n");
    } 
}

int main(void) {
    char line[LINE_MAX];

    uart_init(); // 初始化 UART，讓我們可以透過它來輸入指令、輸出訊息
    uart_puts("\n=== NCTU OSDI Lab 1 - Hello World Shell ===\n");
    uart_puts("Type 'help' to get started.\n");

    while (1) {
        uart_puts("# ");
        uart_readline(line, LINE_MAX);
        handle_command(line);
    }

    return 0;
}