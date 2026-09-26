# LLVM MCS-51 Project Guide

## Project goals

This branch develops a complete LLVM toolchain for classic 8051/MCS-51
microcontrollers, with a CC2530 device profile. The intended toolchain covers
Clang C compilation, LLVM code generation, MC assembly and disassembly, ELF
linking with LLD, and firmware image generation such as Intel HEX.

The backend should produce compact, efficient code suitable for MCU firmware,
with code quality approaching IAR and Keil. Treat SDCC as a source of hardware
and ABI examples only; it is not the code-quality target.

## Build directory

Keep build products outside the source tree, in the sibling directory
`../llvm-8051-build`. Configure from this checkout's root when needed:

```sh
cmake -G Ninja -S . -B ../llvm-8051-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD=MCS51
```

Build the compiler and core code-generation tools with:

```sh
cmake --build ../llvm-8051-build \
  --target clang llc llvm-mc llvm-objdump llvm-objcopy ld.lld -j 10
```

## Verification

Run the MCS-51 CodeGen, MC, and LLD relocation tests with:

```sh
../llvm-8051-build/bin/llvm-lit -q \
  llvm/test/CodeGen/8051 \
  llvm/test/MC/MCS51 \
  lld/test/ELF/mcs51-relocations.s
```

The CC2530 end-to-end test is
`llvm/test/CodeGen/8051/cc2530-firmware.c`. It checks C compilation through
Clang and LLD, followed by Intel HEX generation. Keep new MCS-51-specific tests
inside this checkout, even when nearby test directories are symlinks to another
LLVM checkout.

The initial CC2530 common-flash/XDATA linker layout is
`llvm/lib/Target/MCS51/cc2530.ld`. Pass it to Clang/LLD with
`-Wl,-T,<path-to-cc2530.ld> -Wl,--no-check-sections`; the overlap is expected
because CODE and XDATA are separate 8051 buses. This profile covers the common
32 KiB code window and 8 KiB XDATA RAM only. It does not yet implement code
banks or startup copying of initialized `.data` from flash to XDATA.

## Completion criteria

The project is complete only when all of the following work together and are
verified:

* Clang can compile representative C firmware for MCS-51 and CC2530 without
  backend crashes or unsupported, silently miscompiled constructs.
* The ABI, address spaces, ordinary globals, stack objects, interrupt/vector
  use, and the full CC2530 memory map have correct implementations and tests.
* CC2530 flash banking and RAM placement are represented by a usable linker
  profile; startup and required runtime support are available for firmware.
* LLVM's assembler, disassembler, LLD, and object/image tools can produce and
  validate deployable firmware images, including Intel HEX.
* Optimization tests and representative firmware comparisons show compact,
  correct output approaching IAR/Keil quality, rather than merely proving that
  the target is registered or that a minimal image links.

Current work is incomplete. Ordinary unqualified globals are assigned to
XDATA, and the initial linker profile maps zero-initialized globals there, but
initialized data startup is missing. The full CC2530 banked-memory model and
startup/runtime support also need implementation and end-to-end verification.
Do not describe the target as fully supported until those gaps are closed.
