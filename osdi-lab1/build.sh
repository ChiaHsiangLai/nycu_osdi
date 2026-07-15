#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CROSS=aarch64-elf
CFLAGS="-ffreestanding -nostdlib -mgeneral-regs-only -Wall -Wextra"

echo "== 組譯 =="
${CROSS}-gcc ${CFLAGS} -c boot.S  -o boot.o
${CROSS}-gcc ${CFLAGS} -c main.c  -o main.o
${CROSS}-gcc ${CFLAGS} -c uart.c  -o uart.o
${CROSS}-gcc ${CFLAGS} -c utils.c -o utils.o

echo "== 連結 =="
${CROSS}-ld -T linker.ld -o kernel8.elf boot.o main.o uart.o utils.o

echo "== 轉成 raw binary =="
${CROSS}-objcopy -O binary kernel8.elf kernel8.img

echo "== 完成 =="
${CROSS}-readelf -h kernel8.elf | grep "Entry point"
ls -la kernel8.img

if [[ "${1:-}" == "run" ]]; then
  echo "== 用 QEMU 執行（Ctrl+A 再按 X 離開）=="
  qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial null -serial stdio -display none
fi