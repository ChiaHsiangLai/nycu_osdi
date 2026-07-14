# Lab 1 學習筆記：Hello World（進行中，目前完成 required 1，required 2 開工）

程式碼現況存在 `osdi-lab1-progress/`（`boot.S`、`linker.ld`、`main.c`、`mmio.h`，都已加好註解），對應你自己手上正在寫的那份。

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

### 接下來（還沒做）

- 寫 `uart_init()`：設定 GPIO14/15 為 ALT5（mini UART 功能）、關閉 pull up/down、依序設定 mini UART 的暫存器（AUX_ENABLES、AUX_MU_IER_REG、AUX_MU_LCR_REG、AUX_MU_BAUD_REG...）。
- 寫 `uart_putc`/`uart_getc`：靠讀寫 `AUX_MU_LSR_REG` 的旗標位元，判斷可不可以送/收資料。
- 寫一個能讀一行輸入的簡易 shell，支援 `help`/`hello` 指令（required 3）。
