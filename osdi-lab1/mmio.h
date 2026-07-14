// rpi3 的周邊裝置（UART、GPIO、Timer...）都是透過 MMIO
// (Memory-Mapped I/O) 存取：讀寫特定的記憶體位址，
// 實際上是在跟硬體暫存器溝通，不是在讀寫一般的 RAM。
#ifndef MMIO_H
#define MMIO_H
// include guard：避免同一個檔案被多個 .c 重複 #include 時，內容重複定義而報錯。

// Pi 3 的周邊硬體暫存器，實際位址都是從這裡開始往後排列的一段區域。
#define MMIO_BASE 0x3F000000UL

// volatile 很重要：
// 沒有它，編譯器可能會覺得「這個位址我剛寫過值了，讀出來的東西我早就知道」，
// 把讀寫「最佳化」掉。但硬體暫存器的值會自己變（例如某個旗標位元），
// 每一次存取都必須真的發生，不能被省略。
static inline void mmio_write(unsigned long reg, unsigned int val) {
    *(volatile unsigned int *)reg = val;
}

static inline unsigned int mmio_read(unsigned long reg) {
    return *(volatile unsigned int *)reg;
}

// 陽春的忙碑等待。之後設定 GPIO 時，中間有些步驟需要留一點時間讓硬體反應。
static inline void delay_cycles(unsigned long count) {
    while (count--) {
        asm volatile("nop");
    }
}

#endif
