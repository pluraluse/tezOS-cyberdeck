/*
 * Real linker script for STM32F401CCU6 (Black Pill, 256KB flash /
 * 64KB RAM) -- the bootstrap chip. NOT for the STM32F412 (the real,
 * final signer target) -- that chip has different, larger flash/RAM
 * and needs its own linker script once real F412 hardware exists.
 * Deliberately kept in a clearly separate directory
 * (stm32f401_bootstrap/) from the F412 code, so there's no risk of
 * confusing "the chip used to bootstrap the toolchain/protocol" with
 * "the chip the real RNG safety layer and eventual signing logic are
 * built for" -- see docs/build-notes/signer-protocol-build-notes.md
 * for why that distinction matters.
 *
 * Standard STM32 bare-metal linker script structure (matches what
 * ST's own CubeMX generates, and what real bare-metal STM32 projects
 * commonly use) -- not invented from scratch.
 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 64K
}

_Min_Heap_Size  = 0x200;
_Min_Stack_Size = 0x400;

ENTRY(Reset_Handler)

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
