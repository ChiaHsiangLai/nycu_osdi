# Lab 0 學習筆記：環境設置 / 第一支 bare-metal 程式

## 開機時到底發生什麼事

平常寫 C 程式，`main()` 執行前作業系統已經把一堆事情做好：載入程式、設好 stack、初始化全域變數。Raspberry Pi 開機完全沒有這些。GPU 只做兩件事：把 `kernel8.img` 整個檔案原封不動搬進記憶體位址 `0x80000`，然後讓 CPU 跳去那裡開始執行——不管那裡是什麼，直接當成指令執行。這就是「bare metal」：沒有 stack、沒有初始化、什麼都沒有，第一行指令就要對。

## 我們寫的第一支程式（`boot.S`）

```asm
.section ".text"
.global _start
_start:
  wfe
  b _start
```

逐行意義：

- `.section ".text"`：組譯出來的內容分門別類放進不同「段」，`.text` 專門放程式碼。之後還會遇到 `.data`（有初始值的全域變數）、`.bss`（沒初始值的全域變數）。
- `.global _start`：預設一個標籤只有這個檔案自己看得到；連結器之後需要從外部看到 `_start`（當作程式進入點），所以要公開它。
- `_start:`：標籤，幫某個位址取名字，本身不是指令、不會被執行。
- `wfe`：wait for event，CPU 進入省電待機，等事件把它叫醒。
- `b _start`：跳回 `_start`，形成安全的無窮迴圈——這支程式的目的只是「開機後停在一個可預期的狀態」，用來驗證整條工具鏈是通的。

## 連結腳本（`linker.ld`）：為什麼需要它

寫一般應用程式時感覺不到 linker 的存在，是因為它用預設腳本幫你決定記憶體佈局。Bare-metal 環境下，我們知道 GPU 會把 `kernel8.img` 放在 `0x80000`，所以必須自己告訴 linker「程式碼要從 `0x80000` 開始」，不然程式裡所有跳躍指令的目標位址都會算錯。

```
SECTIONS
{
  . = 0x80000;
  .text : { *(.text) }
}
```

- `.` 是連結腳本裡的「目前位置指標」。
- `. = 0x80000;` 把位置指標設成 `0x80000`，之後放的東西都從這裡開始算。
- `.text : { *(.text) }` 把所有輸入檔案裡屬於 `.text` 的內容收集起來放在這裡。

## 建置流程：組譯 → 連結 → 轉檔

三個步驟，各自對應一個工具：

```bash
# 1. 組譯：組合語言文字 -> 機器碼（半成品 .o，位址還沒定案）
aarch64-elf-gcc -c boot.S -o boot.o

# 2. 連結：用 linker.ld 決定最終位址佈局 -> 完整的 .elf
aarch64-elf-ld -T linker.ld -o kernel8.elf boot.o

# 3. 轉檔：ELF 裡有除錯資訊等額外資料，GPU 只認得純機器碼 -> raw binary
aarch64-elf-objcopy -O binary kernel8.elf kernel8.img
```

驗證方式：

- `aarch64-elf-readelf -h kernel8.elf | grep "Entry point"` → 確認 entry point 是 `0x80000`。
- `aarch64-elf-objdump -d kernel8.elf` → 反組譯確認位址跟指令都對（`wfe` + `b 80000 <_start>`）。
- `ls -la kernel8.img` → 兩條指令各 4 bytes，檔案應該剛好 **8 bytes**。

## 用 QEMU 驗證開機

```bash
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm
```

- `-M raspi3b`：模擬一台 Raspberry Pi 3（新版 QEMU 用 `raspi3b`，舊版可能是 `raspi3`）。
- `-kernel kernel8.img`：QEMU 扮演 GPU 的角色，把這個檔案搬進模擬記憶體的 `0x80000` 並跳過去執行。
- `-display none`：不開圖形視窗。
- `-d in_asm`：每執行一條指令就印出反組譯結果，讓我們親眼確認程式真的在跑。

### 實際跑出來的結果，怎麼解讀

1. **位址 `0x0` 開始的一段**：不是我們寫的，是 QEMU 內建模擬「開機韌體」在做的事——清暫存器、把 kernel 該執行的位址讀進暫存器、跳過去。等同真實 Pi 上 GPU 韌體的角色。
2. **位址 `0x80000` 那段**：就是我們自己寫的程式，跟原始碼完全一致（`wfe` + `b 0x80000`）。看到這段代表整條工具鏈（打字 → 組譯 → 連結 → 轉檔 → QEMU 開機執行）全部正確。
3. **位址 `0x300` 反覆出現的一段**：QEMU 模擬「其他 3 顆 CPU 核心」的行為，跟我們的程式無關——它們去檢查一個固定位址，如果還是 0 就 `wfe` 睡覺、醒來再檢查，無窮迴圈（所以要 `Ctrl+C` 手動停止，這是正常現象，不是當機）。這也預告了 Lab 1 要自己做的事：手動判斷「我是哪顆核心」，非 0 號核心自己睡死。

## 用到的工具清單

| 指令 | 用途 |
| --- | --- |
| `aarch64-elf-gcc` | 交叉編譯器：組譯 `.S`/編譯 `.c` |
| `aarch64-elf-ld` | 連結器：依 linker script 決定記憶體佈局 |
| `aarch64-elf-objcopy` | 把 ELF 轉成 raw binary |
| `aarch64-elf-objdump` | 反組譯，檢查機器碼對不對 |
| `aarch64-elf-readelf` | 檢查 ELF 檔頭資訊（entry point 等） |
| `qemu-system-aarch64` | 模擬 Pi 3，載入 `kernel8.img` 開機執行 |

## 下一步：Lab 1

會在 `boot.S` 的基礎上，加上「篩選核心」（只留 core 0）、「清空 BSS」、「設定 stack pointer」，接著開始寫 C 程式：初始化 mini UART、實作一個能讀輸入、支援 `help`/`hello` 指令的簡易 shell。
