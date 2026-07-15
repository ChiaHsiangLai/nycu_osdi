# Lab 1 學習筆記：Hello World（required 1~3 全部完成，已用 QEMU 實測成功）

程式碼最終版存在 `osdi-lab1-progress/`（`boot.S`、`linker.ld`、`main.c`、`uart.c`/`uart.h`、`utils.c`/`utils.h`、`mmio.h`，都已加好註解），對應你自己手上寫的那份，在沙盒裡重新編譯驗證過（entry point `0x80000`、無編譯警告）。

## required 1：基本初始化（已完成並驗證）

### 為什麼要篩選核心

rpi3 開機後，GPU 會「同時」啟動 4 顆 CPU 核心，全部從 `0x80000` 開始執行同一份程式碼。如果放任不管，4 顆核心會同時搶著初始化、搶著跑後面的 shell，資料會亂掉。

用 `mpidr_el1` 這個 ARM 內建系統暫存器讀出核心編號，只留 core 0：

```asm
mrs     x1, mpidr_el1   // 讀系統暫存器：mrs = move from system register
and     x1, x1, #3      // 只留最低 2 bits，就是 0~3 的核心編號
cbz     x1, master      // x1 == 0（core 0）才跳去 master

proc_hang:
  wfe                   // 其他核心：等待事件（省電睡眠）
  b proc_hang           // 睡醒了也不做事，繼續睡，安全的無窮迴圈
```

驗證方式：反組譯確認 `cbz` 的跳躍目標正確指向 `master`，`proc_hang` 是自我循環。

### 為什麼要先設定 stack pointer

C 語言的函式呼叫（區域變數、返回位址、暫存器備份）全部依賴 stack。`sp`（stack pointer 暫存器）開機時是垃圾值，呼叫任何 C 函式之前，一定要先給它一個安全、確定沒人用的位址。

```asm
ldr x1, =_stack_top
mov sp, x1
```

- `_stack_top` 定義在 `linker.ld`，值是 `0x80000`。
- `ldr x1, =xxx` 這個「等號」寫法是組譯器的偽指令：ARM64 指令長度固定 4 bytes，塞不下完整的 64-bit 位址，組譯器會把這個數值放在程式碼附近一塊叫 **literal pool** 的資料區，`ldr` 其實是「去 literal pool 把值讀出來」。反組譯時你會看到這類指令旁邊多出幾行 `.word` 資料，那就是 literal pool。
- 不能直接 `ldr sp, =xxx`，這種查表讀取的動作不能直接對 `sp` 做，要先經過一般暫存器 `x1` 中轉。
- Stack 往低位址成長，我們的程式碼往高位址成長，兩者從 `0x80000` 往相反方向長，不會互踩。

### 為什麼要清 BSS，怎麼清

C 語言規定「沒給初始值的全域變數」開機時要是 0（例如 `int counter;`）。這件事平常是作業系統的載入器幫你做的，bare-metal 沒人做，要自己清。

```asm
ldr x1, =__bss_start
ldr x2, =__bss_end
bss_clear_loop:
  cmp x1, x2
  b.ge bss_clear_done      // x1 >= x2 代表清完了
  str xzr, [x1], #8        // xzr 永遠是 0；寫 8 bytes 的 0，然後 x1 += 8（post-index）
  b bss_clear_loop
bss_clear_done:
```

`__bss_start`/`__bss_end` 是 `linker.ld` 算好定義的 symbol，標出 BSS 的起訖位址（見下方連結腳本章節）。

驗證：目前程式還沒有任何 C 全域變數，實際跑出來 `__bss_start == __bss_end == 0x80050`（一路增加程式碼後變成 `0x80088`），代表迴圈第一次比較就正確判斷「範圍是空的，不用清」，不是 bug。

### 連結腳本（linker.ld）

linker script 是一張「記憶體佈局藍圖」，核心機制是**位置指標 `.`**：

- 手動設定：`. = 0x80000;`
- 自動前進：定義一個段（例如 `.text : { *(.text) }`）之後，`.` 會自動跳過剛放進去的內容大小
- 讀出存成 symbol：`__bss_start = .;`

```
SECTIONS
{
  . = 0x80000;
  .text : { *(.text) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  . = ALIGN(8);
  __bss_start = .;
  .bss : { *(.bss*) *(COMMON) }
  __bss_end = .;
}
_stack_top = 0x80000;
```

佈局圖：

```
0x80000  +-------------+
         |   .text     |  程式碼
         +-------------+
         |   .rodata   |  唯讀資料（字串常數等）
         +-------------+
         |   .data     |  有初始值的全域變數
         +-------------+ <- 補齊到 8 的倍數
__bss_start -----------+
         |   .bss      |  沒初始值的全域變數（要被清零）
__bss_end -------------+
```

補充兩個常見疑問：

- **`.text`/`.rodata`/`.data`/`.bss` 不是語言保留字**，是編譯器/組譯器輸出 `.o` 檔時的慣例命名（用 `objdump -h a.o` 可以親眼看到），linker script 裡的 `*(.text)` 是在跟這些名字做字串比對；左邊 `.text : { ... }` 這個輸出段名字理論上可以取別的，只是沿用同樣名字方便閱讀、跟其他工具相容。
- **linker script 只支援 `/* ... */` 這種 C 風格註解，不支援 `//`**——這點在正式的 `linker.ld` 裡要注意（我自己在準備這份筆記的範例檔時就踩到這個雷，`ld` 會把 `//` 後面的中文字元逐個 byte 當成語法錯誤報出來）。

### 從組合語言跳進 C

```asm
bss_clear_done:
  bl main
  b proc_hang
```

- `bl` = branch and link：跳去 `main` 之前，先把「下一行位址」記在 `x30`（link register），這樣 `main` 理論上執行完可以跳回來，是正確、通用的函式呼叫寫法。
- `b proc_hang`：`main` 實際上是無窮迴圈不會返回，但萬一真的返回了，導向安全的睡眠迴圈，不讓 CPU 亂飛。

編譯 C 檔案要多兩個參數：

```bash
aarch64-elf-gcc -ffreestanding -nostdlib -c boot.S -o boot.o
aarch64-elf-gcc -ffreestanding -nostdlib -c main.c -o main.o
aarch64-elf-ld -T linker.ld -o kernel8.elf boot.o main.o
```

- `-ffreestanding`：不能假設有作業系統、有標準函式庫。
- `-nostdlib`：不要連結任何標準函式庫（沒有 libc）。

驗證：反組譯確認 `bl` 跳去的位址跟 `main` 這個 symbol 的位址一致；`main` 目前只有 `while(1) {}`，編譯器直接最佳化成 `b <main>` 原地跳圈。

## required 2：Mini UART（進行中）

### MMIO（Memory-Mapped I/O）概念

Pi 的周邊硬體（UART、GPIO、Timer...）透過讀寫特定記憶體位址來控制，不是普通的 RAM 讀寫：寫入某個位址等於扳動硬體開關，讀取某個位址拿到的是硬體目前的狀態（例如某個旗標位元）。

```c
#define MMIO_BASE 0x3F000000UL

static inline void mmio_write(unsigned long reg, unsigned int val) {
    *(volatile unsigned int *)reg = val;
}

static inline unsigned int mmio_read(unsigned long reg) {
    return *(volatile unsigned int *)reg;
}
```

`volatile` 的作用：沒有它，編譯器可能覺得「這個位址我剛寫過了，讀出來的值我早就知道」而把讀寫最佳化掉；但硬體暫存器的值會自己變，每次存取都必須真的發生。

`#ifndef MMIO_H` / `#define MMIO_H` / `#endif` 是 include guard，避免同一個 header 被多個 `.c` 檔重複引入時內容重複定義而報錯。

### GPIO 設定（ALT5）與 mini UART 暫存器初始化

`uart_init()` 分兩段：先把 GPIO14/15 切成 ALT5（mini UART 功能），再依序設定 mini UART 本體的暫存器。

**GPIO 部分**：`GPFSEL1` 這個暫存器一次管好幾根 GPIO 腳位（每根佔 3 個 bit），所以要用 **read-modify-write（讀-改-寫）** 模式：先讀出目前完整的 32-bit 內容，只修改我們關心的那幾個 bit，其他 bit 保持原樣，最後才整個寫回去——如果沒有先讀就直接蓋，會把其他沒打算動的腳位設定也弄壞。

```c
unsigned int selector = mmio_read(GPFSEL1);
selector &= ~(7u << 12);   // 清空 GPIO14 的 3 個 bit（先蓋成 0）
selector |= (2u << 12);    // 填入 ALT5 編碼 (0b010)
selector &= ~(7u << 15);   // GPIO15 同理
selector |= (2u << 15);
mmio_write(GPFSEL1, selector); // 只有這一行才是真正動到硬體
```

接著關閉這兩根腳位原本的 pull up/down 電阻設定（改成 alternate function 後不需要）：官方文件規定的兩段式流程——寫入控制值(`GPPUD`)、等待、寫入要套用到哪幾根腳位的遮罩(`GPPUDCLK0`)、等待、清空 `GPPUDCLK0` 完成鎖存。

**mini UART 本體**：依序設定 `AUX_ENABLES`（開電源）、`AUX_MU_IER_REG`（關中斷）、`AUX_MU_CNTL_REG`（設定期間先關收發）、`AUX_MU_LCR_REG`（8-bit 資料長度）、`AUX_MU_MCR_REG`（不用流量控制）、`AUX_MU_BAUD_REG`（傳輸速率）、`AUX_MU_IIR_REG`（不用 FIFO）、最後再開收發功能。

`AUX_MU_BAUD_REG` 設成 `270` 不是隨便選的：

```
baud rate = 系統時脈 / (8 × (AUX_MU_BAUD_REG + 1))
115200    = 250000000 / (8 × (270 + 1))
```

開機後系統時脈是 250MHz，反推要湊出業界常用的 115200 baud，算出來就是 270。這個 115200 之後在電腦端連線（QEMU 或終端機軟體）也要設定一致。

驗證：反組譯逐行核對每個暫存器位址（例如 `AUX_ENABLES` = `0x3F215004`）跟寫入的數值，全部跟原始碼一一對上，位元遮罩（`0xffff8fff`、`0xfffc7fff` 等）也都跟 `~(7u << 12)` 這類運算式算出來的結果一致。

### `uart_putc`/`uart_getc`：忙碑等待（busy-wait / polling）

```c
void uart_putc(char c) {
    while (!(mmio_read(AUX_MU_LSR_REG) & (1u << 5))) { } // bit5 = 傳輸暫存器空了嗎
    mmio_write(AUX_MU_IO_REG, (unsigned int)c);
}

char uart_getc(void) {
    while (!(mmio_read(AUX_MU_LSR_REG) & 1u)) { }        // bit0 = 有新資料嗎
    return (char)(mmio_read(AUX_MU_IO_REG) & 0xFF);
}
```

`AUX_MU_LSR_REG`（Line Status Register）的旗標位元隨時反映硬體狀態。`while(!(...))` 這種「一直問硬體準備好了沒」的寫法叫忙碑等待——陽春但直觀，Lab 3 學中斷後會有更有效率的做法。

**資料實際去哪裡了**：`uart_putc` 寫進 `AUX_MU_IO_REG`，這個動作讓 mini UART 硬體把資料透過 TX 接腳（GPIO14）送出去；真實 Pi 上經 USB-TTL 線送到電腦的序列埠，QEMU 則是攔截這個位址的寫入，轉送到 `-serial` 參數指定的目的地（我們用 `-serial null -serial stdio`，接到執行 QEMU 的 Terminal 視窗）。`uart_getc` 方向相反：讀同一個暫存器，硬體已經先把外部傳進來的 byte 準備好在那裡。

### `uart_readline`：組合成一整行

```c
int uart_readline(char *buf, int max_len) {
    int i = 0;
    while (1) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {          // Enter：結束這一行
            uart_putc('\r'); uart_putc('\n');
            break;
        }
        if (c == 0x7F || c == 0x08) {          // Backspace：刪掉上一個字元
            if (i > 0) { i--; uart_puts("\b \b"); }
            continue;
        }
        if (i < max_len - 1) {
            buf[i++] = c;
            uart_putc(c);                       // echo：讓使用者看到自己打的字
        }
    }
    buf[i] = '\0';                              // 補上字串結尾記號
    return i;
}
```

寫這支函式時踩過的坑，記錄一下：

- **不能用 `c == '\0'` 判斷「輸入結束」**：`'\0'` 是我們事後才加上去的字串結尾記號，鍵盤打不出這個字元，使用者按 Enter 送出的是 `'\r'`/`'\n'`，要判斷這個才對。
- **不能拿指標直接跟長度比大小**（例如 `buf < maxlen`）：`buf` 是位址、`maxlen` 是數量，單位不同，比較沒有意義。要另外維護一個索引 `i`，用 `buf[i++] = c` 存取，`buf` 這個指標本身不要移動，不然函式結束後呼叫端拿到的指標就跑到字串尾端去了。
- **忘記補 `'\0'`**：字串陣列裡沒有結尾記號的話，之後 `str_eq` 之類的函式無法知道字串在哪裡結束，會讀到不可預期的內容。

### `str_eq`：自己刻的字串比對

bare-metal 沒有 libc，`strcmp` 用不了：

```c
int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b; // 兩邊要「同時」走到結尾的 '\0' 才算真的相等
}
```

最後那行容易忽略：如果只用「有沒有走到某一邊的結尾」判斷，會把類似 `"hell"` 跟 `"hello"` 這種一個是另一個前綴的情況誤判成相等。要兩邊同時是 `'\0'` 才算數。

### 整合成 shell（required 3，已用 QEMU 實測成功）

`main.c` 用 read-eval loop 的結構：印提示字元 `# ` → 讀一行 → 跟已知指令比對 → 執行 → 重複。

```c
int main(void) {
    char line[LINE_MAX];   // 區域變數，配置在 stack 上
    uart_init();
    uart_puts("\n=== NCTU OSDI Lab 1 - Hello World Shell ===\n");
    while (1) {
        uart_puts("# ");
        uart_readline(line, LINE_MAX);
        handle_command(line);
    }
}
```

`char line[LINE_MAX]` 這個區域陣列會被放上 stack——這也回頭印證了 Lab 1 一開始為什麼要先在 `boot.S` 設好 `sp` 才能呼叫 `main`：沒有正確的 stack，這種陣列的空間根本無從分配。

**實際跑出來的結果**（用 `qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial null -serial stdio -display none`）：

```
=== NCTU OSDI Lab 1 - Hello World Shell ===
Type 'help' to get started.
# help
Available commands:
  help   - print all available commands
  hello  - print Hello World!
# hello
Hello World!
#
```

跟課程 required 3 的要求（`help`/`hello` 兩個指令）完全吻合。從最開始一行 `wfe` 的無窮迴圈，到現在能互動的 shell，required 1~3 全部完成並實測驗證。

## 還沒做的部分（elective，非必要）

- `timestamp` 指令：讀 `CNTFRQ_EL0`（計時器頻率）跟 `CNTPCT_EL0`（目前計數），算出開機後經過的時間。
- `reboot` 指令：透過 PM_RSTC/PM_WDOG 暫存器觸發重開機（只在真實 rpi3 上有效，QEMU 不支援）。

## 下一步

Lab 2：Bootloader——學怎麼用 mailbox 跟 GPU 要硬體資訊，以及寫一個能透過 UART 直接上傳新 kernel image 的 bootloader。
