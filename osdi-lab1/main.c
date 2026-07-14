#include "uart.h"
#include "utils.h"

#define LINE_MAX 128

static void print_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help   - print all available commands\n");
    uart_puts("  hello  - print Hello World!\n");
}

static void handle_command(const char *cmd) {
    if (str_eq(cmd, "")) {
        return; // 使用者直接按 Enter，什麼都不做
    } else if (str_eq(cmd, "help")) {
        print_help();
    } else if (str_eq(cmd, "hello")) {
        uart_puts("Hello World!\n");
    } else {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_puts("\n");
        uart_puts("Type 'help' to see available commands.\n");
    }
}

int main(void) {
    char line[LINE_MAX];

    uart_init();
    uart_puts("\n=== NCTU OSDI Lab 1 - Hello World Shell ===\n");
    uart_puts("Type 'help' to get started.\n");

    while (1) {
        uart_puts("# ");
        uart_readline(line, LINE_MAX);
        handle_command(line);
    }

    return 0;
}