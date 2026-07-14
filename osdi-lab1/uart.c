#include "mmio.h"
#include "uart.h"

#define GPFSEL1   (MMIO_BASE + 0x00200004) // 32 bits, 表示腳位 GPIO10~19 的功能選擇
#define GPPUD     (MMIO_BASE + 0x00200094)
#define GPPUDCLK0 (MMIO_BASE + 0x00200098)

#define AUX_ENABLES     (MMIO_BASE + 0x00215004)
#define AUX_MU_IO_REG   (MMIO_BASE + 0x00215040)
#define AUX_MU_IER_REG  (MMIO_BASE + 0x00215044)
#define AUX_MU_IIR_REG  (MMIO_BASE + 0x00215048)
#define AUX_MU_LCR_REG  (MMIO_BASE + 0x0021504C)
#define AUX_MU_MCR_REG  (MMIO_BASE + 0x00215050)
#define AUX_MU_LSR_REG  (MMIO_BASE + 0x00215054)
#define AUX_MU_CNTL_REG (MMIO_BASE + 0x00215060)
#define AUX_MU_BAUD_REG (MMIO_BASE + 0x00215068)

#define AUX_MU_LSR_REG (MMIO_BASE + 0x00215054)

void uart_init(void)
{
    // 1. 把 GPIO14/15 切成 ALT5（mini UART）
    unsigned int selector = mmio_read(GPFSEL1);
    selector &= ~(7u << 12);   // 先把 GPIO14 對應的 3 個 bit 清成 0
    selector |= (2u << 12);    // 再填入 ALT5 的編碼 (0b010)
    selector &= ~(7u << 15);   // GPIO15 同樣先清 0
    selector |= (2u << 15);    // 再填入 ALT5
    mmio_write(GPFSEL1, selector);

    // 2. 因為改成 alternate function 了，要關掉這兩根腳位原本的
    //    pull up/down 電阻設定，不然電位會被拉住、訊號讀不準。
    mmio_write(GPPUD, 0);
    delay_cycles(150);
    mmio_write(GPPUDCLK0, (1u << 14) | (1u << 15));
    delay_cycles(150);
    mmio_write(GPPUDCLK0, 0);

    // 3. 依序初始化 mini UART 本體（順序是官方文件規定的，照著做）
    mmio_write(AUX_ENABLES, 1);       // 打開 mini UART 電源開關，暫存器才能被存取
    mmio_write(AUX_MU_IER_REG, 0);    // 先關中斷，這個 Lab 用不到
    mmio_write(AUX_MU_CNTL_REG, 0);   // 設定期間先關閉收發功能
    mmio_write(AUX_MU_LCR_REG, 3);    // 設定資料長度為 8 bit
    mmio_write(AUX_MU_MCR_REG, 0);    // 不需要硬體流量控制
    mmio_write(AUX_MU_BAUD_REG, 270); // 設定傳輸速率(baud rate)
    mmio_write(AUX_MU_IIR_REG, 6);    // 不使用 FIFO 緩衝
    mmio_write(AUX_MU_CNTL_REG, 3);   // 打開收發功能，設定完成
}

void uart_putc(char c)
{
    // busy wait：LSR 第 5 個 bit 是 0 就代表「還沒空出來」，一直檢查到它變 1 為止 (輪詢法)
    while (!(mmio_read(AUX_MU_LSR_REG) & (1u << 5)))
    {
        // 忙碑等待：LSR 第 5 個 bit 是 0 就代表「還沒空出來」，一直檢查到它變 1 為止
    }
    mmio_write(AUX_MU_IO_REG, (unsigned int)c);
}

char uart_getc(void)
{
    // busy wait：LSR 第 0 個 bit 是 0 就代表「還沒有新資料」，一直檢查到它變 1 為止 (輪詢法)
    while (!(mmio_read(AUX_MU_LSR_REG) & 1u))
    {
        // 忙碑等待：LSR 第 0 個 bit 是 0 就代表「還沒有新資料」
    }
    return (char)(mmio_read(AUX_MU_IO_REG) & 0xFF);
}

void uart_puts(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
        {
            uart_putc('\r'); // 大多數終端機軟體需要 \r\n 才會正確換行、游標歸到最左邊
        }
        uart_putc(*s++);
    }
}

int uart_readline(char *buf, int max_len) {
    int i = 0;

    while (1) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            break;
        }

        // backspace（不同終端機可能送 0x7F 或 0x08）
        if (c == 0x7F || c == 0x08) {
            if (i > 0) {
                i--;
                uart_puts("\b \b"); // 畫面上把上一個字元「視覺上」蓋掉
            }
            continue;
        }

        if (i < max_len - 1) {
            buf[i++] = c;
            uart_putc(c); // echo：把使用者剛打的字元送回螢幕
        }
    }

    buf[i] = '\0';
    return i;
}