#ifndef UART_H
#define UART_H

void uart_init(void);

void uart_putc(char c);
char uart_getc(void);

void uart_puts(const char *s);
int uart_readline(char *buf, int max_len);

#endif