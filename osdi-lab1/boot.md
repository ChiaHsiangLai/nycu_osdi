# boot.S 筆記

對應 [boot.S](boot.S) 的說明文件。

## 背景

rpi3 開機後，GPU 會「同時」啟動 4 顆 CPU 核心，全部從同一個位址(0x80000)
開始執行同一份程式碼。我們要做三件事，才能安全地跳進 C 的世界：

1. 篩選核心：只留 core 0 繼續，其他核心睡死。
2. 設定 stack pointer：C 函式呼叫都依賴 stack，沒設好之前不能呼叫任何 C 函式。
3. 清空 .bss：C 語言規定「沒初始值的全域變數」開機時要是 0，
   這件事平常是作業系統幫你做的，bare-metal 下要自己清。

## `_start`

讀出目前是哪顆核心：`mpidr_el1` 是 ARM 內建的系統暫存器，
裡面記錄了核心編號等資訊，取最低 2 個 bit 就是 0~3 的核心編號。

只有 core 0（x1 == 0）才會跳去 `master`。

## `proc_hang`

非 0 號核心，安全地睡死在這裡：`wfe` 等待事件、`b` 跳回去繼續等，
形成不會亂跑的無窮迴圈。

## `master`

### 第一步：設定 stack pointer

`_stack_top` 定義在 linker.ld 裡，值是 0x80000。
stack 往低位址成長、我們的程式碼往高位址成長，兩者方向相反不會互踩。

為什麼要先 `ldr` 到 x1 再 `mov` 給 sp：
ARM64 指令長度固定 4 bytes，塞不下一個完整的 64-bit 位址，
組譯器會把這個位址值偷偷放在程式碼附近的 literal pool，
`ldr x1, =xxx` 其實是「去 literal pool 把值讀出來」，
而這個讀取的動作不能直接對 sp 做，所以要先經過 x1 中轉。

### 第二步：清空 .bss

`__bss_start` / `__bss_end` 是 linker.ld 算好、定義的兩個 symbol，
標出 BSS 這塊記憶體的起訖位址。

`bss_clear_loop`：
- x1 >= x2 代表清完了，跳出迴圈 (`b.ge bss_clear_done`)
- `xzr` 永遠是 0；`str xzr, [x1], #8` 寫入 8 bytes 的 0，然後 x1 += 8（post-index）

### 第三步：跳進 C 的世界

`bl` = branch and link，跳去 `main` 之前，會把「返回位址」記在 x30(link register)，
這樣 `main` 執行完理論上可以跳回來（正確、通用的函式呼叫寫法）。

`main()` 是無窮迴圈，理論上不會返回。
萬一真的返回了，也不要讓 CPU 亂跑，導向已經寫好的安全睡眠迴圈 (`proc_hang`)。
