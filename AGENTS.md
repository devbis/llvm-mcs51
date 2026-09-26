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
  --target clang llc llvm-mc llvm-objdump llvm-objcopy llvm-readobj \
  FileCheck ld.lld -j 10
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
32 KiB code window and defines the CC2530 XDATA windows for 8 KiB SRAM, XREG,
memory-mapped SFRs, the information page, and the selectable 32 KiB flash-bank
window. Linker symbols also describe the DATA alias in the top 256 bytes of
SRAM. Ordinary `.data` and `.bss` allocation remains limited to SRAM. The
current reset stub in
`llvm/lib/Target/MCS51/cc2530_startup.s` sets the stack pointer and clears
XDATA, copies initialized `.data` from CODE to XDATA, then calls `main`. The
profile does not yet package physical bank images or implement code-bank
switching calls.

## Final target and completion criteria

The final deliverable is a buildable LLVM branch that can compile, optimize,
link, and emit deployable firmware for classic MCS-51/8051 devices and the
CC2530. Building the listed tools and passing the focused test suite are the
minimum per-change checks; they do not by themselves mean the architecture is
fully supported.

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

For a final readiness check, build all listed tools, run the MCS-51 CodeGen,
MC, and LLD relocation tests above, and run the CC2530 end-to-end firmware test.
Add focused regression tests for each newly supported ABI, instruction,
address-space, linker, startup, and image-generation feature. Inspect generated
assembly and linked images for representative firmware to confirm both
correctness and code size.

Current work is incomplete. Ordinary unqualified globals are assigned to
XDATA, and the initial linker profile maps zero-initialized globals there.
The full CC2530 banked-memory model, broader startup/runtime support, and
end-to-end verification of those features remain to be implemented. Do not
describe the target as fully supported until those gaps are closed.
