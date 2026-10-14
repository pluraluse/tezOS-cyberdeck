#!/bin/bash
# Real, reproducible build for the STM32F401CCU6 bootstrap firmware --
# builds the SAME already-tested wire protocol + session-assembly
# layers used by the real F412 target, linked against a real,
# minimal startup file and linker script for this specific bootstrap
# chip. Produces a real, flashable .bin image.
#
# Deliberately does NOT build or link the RNG safety layer
# (tezos_signer_rng_core.c/tezos_signer_rng.c) -- the STM32F401 does
# not have that hardware peripheral at all. See main.c's own header
# comment for the full reasoning.
#
# Run from this directory (firmware/signer/stm32f401_bootstrap/).
set -e

PROTOCOL_DIR=../protocol
CMSIS_DIR=../../../third_party/cmsis_stm32f4/Include
CFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32F401xC -Wall -Wextra -std=c11 -I$CMSIS_DIR -I$PROTOCOL_DIR"

mkdir -p build

arm-none-eabi-gcc $CFLAGS -c main.c -o build/main.o
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
    -c startup_stm32f401xc.s -o build/startup.o
arm-none-eabi-gcc $CFLAGS -c $PROTOCOL_DIR/tezos_signer_apdu.c -o build/apdu.o
arm-none-eabi-gcc $CFLAGS -c $PROTOCOL_DIR/tezos_signer_session.c -o build/session.o

arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
    -T linker.ld --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=build/output.map \
    -o build/tezos_signer_f401_bootstrap.elf \
    build/startup.o build/main.o build/apdu.o build/session.o

arm-none-eabi-objcopy -O binary build/tezos_signer_f401_bootstrap.elf build/tezos_signer_f401_bootstrap.bin

echo ""
echo "Built: build/tezos_signer_f401_bootstrap.bin"
arm-none-eabi-size build/tezos_signer_f401_bootstrap.elf
echo ""
echo "To flash via DFU (BOOT0 held during reset/power-on to enter the"
echo "system bootloader first):"
echo "  dfu-util -a 0 -s 0x08000000:leave -D build/tezos_signer_f401_bootstrap.bin"
