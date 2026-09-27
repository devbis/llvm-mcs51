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
  FileCheck lld mcs51-runtime -j 10
```

## Verification

After changing the backend, build the compiler and tools from the sibling build
directory, then run the focused MCS-51 tests. From the `llvm-8051` source root:

```sh
cmake --build ../llvm-8051-build \
  --target clang llc llvm-mc llvm-objdump llvm-objcopy llvm-readobj FileCheck lld \
  mcs51-runtime -j 10

../llvm-8051-build/bin/llvm-lit -q \
  llvm/test/CodeGen/8051 \
  llvm/test/MC/MCS51 \
  lld/test/ELF/mcs51-relocations.s \
  lld/test/ELF/mcs51-data-pointer-reloc.s \
  lld/test/ELF/mcs51-11-bit-branch.s \
  lld/test/ELF/mcs51-auto-bank.s \
  llvm/test/CodeGen/8051/cc2530-firmware.c \
  llvm/test/CodeGen/8051/division-runtime.c
```

The CC2530 end-to-end test checks C compilation through Clang and LLD, followed
by Intel HEX generation. Keep new MCS-51-specific tests inside this checkout,
even when nearby test directories are symlinks to another LLVM checkout. The
Clang driver defaults MCS-51 compilation to freestanding mode and uses static
bare-metal links without hosted startup files or default system libraries.
Building `mcs51-runtime` compiles the arithmetic runtime and CC2530 startup
objects into Clang's resource directory. With those files present,
`clang -target mcs51 -mcpu=cc2530 app.c -o firmware.elf` automatically links
the runtime, startup, CC2530 linker script, and section garbage collection.
CC2530 compilation enables function and data sections so link-time garbage
collection can discard unused firmware functions and globals. Pass
`-fno-function-sections` or `-fno-data-sections` to override either default.
Use `llvm-objcopy --output-target=ihex firmware.elf firmware.hex` to emit Intel
HEX. `-nostdlib` disables this automatic CC2530 profile.

For a standalone CC2530 image, use
`llvm/lib/Target/MCS51/cc2530-build.sh source.c -o firmware.elf`. The script
links the CC2530 reset startup and MCS-51 runtime with the application, then
writes `firmware.hex` next to the ELF. `MCS51_CLANG` and `MCS51_OBJCOPY` can
select non-default LLVM tool paths.

The CC2530 common-flash/XDATA linker layout is
`llvm/lib/Target/MCS51/cc2530.ld`. Pass it to Clang/LLD with
`-Wl,-T,<path-to-cc2530.ld> -Wl,--no-check-sections`; the overlap is expected
because CODE and XDATA are separate 8051 buses. This profile covers the common
32 KiB code window and defines the CC2530 XDATA windows for 8 KiB SRAM, XREG,
memory-mapped SFRs, the information page, and the selectable 32 KiB flash-bank
window. Linker symbols also describe the DATA alias in the top 256 bytes of
SRAM. Static XDATA `.data` and `.bss` outputs are linker-checked to stay below
`0x1f00`, preserving that alias for the CPU's DATA/IDATA space. The linker
assigns explicit DATA/IDATA globals offsets `0x30` through `0x7f`; startup adds
the SRAM alias base when copying their initial values. DATA-space globals use
direct accesses, while IDATA globals support byte and word accesses through
`@R0`. Lower offsets remain available for register banks and bit-addressable
RAM, and the upper half of the alias is reserved for the hardware stack.
`__bit` globals use the bit-address range `0x00` through `0x7f`; initialized
values are loaded from CODE and merged into the corresponding bit-addressable
RAM byte during reset. Clang also provides `__sbit` and `__sfr` qualifiers for
absolute bit-addressed and SFR accesses; use volatile-qualified accesses for
hardware registers. These aliases map to the target's bit and SFR address
spaces. `__pdata` objects occupy the low 256 bytes of CC2530 XDATA, which are
accessed through the hardware-reset `MPAGE` page zero and 8-bit `@Ri` pointers;
initialized objects are copied from CODE at startup, and the linker rejects a
PDATA allocation larger than one page. The current reset stub in
`llvm/lib/Target/MCS51/cc2530_startup.s` sets the stack pointer and clears
XDATA, copies initialized `.data` from CODE to XDATA, then calls `main`. The
linker accepts manually placed `.bank1.*` through `.bank7.*` input sections
and emits them at separate physical flash load addresses while retaining the
shared `0x8000` execution VMA. The linker profile models the 256 KiB flash
variant. A function placed with
`__attribute__((section(".bankN.text")))` gets a common-area trampoline for
direct calls. The trampoline saves FMAP, selects the callee bank, calls the
function, and restores FMAP, including when the caller is itself banked.
Taking the address of a banked function yields its common-area trampoline, so
ordinary 16-bit function pointers can call banked functions indirectly; the
trampoline saves and restores FMAP around the call. Functions placed in unique
`.mcs51.autobank.<name>` sections are distributed across the seven flash banks
by LLD's size-balanced placement pass. Their calls and function pointers use
common-area trampolines whose bank number is resolved after placement. With
`-mcpu=cc2530` and function sections enabled (the CC2530 Clang profile's
default), ordinary `.text.*` function sections are also distributed across
the seven banks by LLD's size-balanced pass. `.text.main`, startup sections,
and interrupt handlers stay in common flash.
Compiler-generated calls and function pointers use common-area bank-call
trampolines; LLD redirects cross-bank direct calls and function-address
relocations to those trampolines. Applications that need predictable manual
placement can still use `.bankN.*` sections. CC2530
interrupt handlers use
`__attribute__((interrupt(N)))`, where `N` is 0 through 17. The backend saves
the interrupted register-bank-0 state and SFR registers, then returns with
`RETI`. It emits a three-byte `LJMP` vector stub; `cc2530.ld` places vector N
at `0x03 + 8*N`, keeps reset at address zero, reserves the vector table, and
rejects duplicate handlers for a vector. Interrupt handlers must reside in
common flash and cannot be called directly.

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

Current work is incomplete. The CC2530 profile supports XDATA globals,
initialized XDATA data, explicit DATA/IDATA globals, manually selected and
automatically balanced flash banks with direct and indirect-call trampolines,
and CC2530 interrupt vectors. It does not yet provide a complete runtime
library. Classic 8051 and CC2530 support must be verified
feature by feature; do not describe the target as fully supported until the
remaining ABI, instruction, memory-map, runtime, optimization, and
firmware-image gaps are implemented and tested.

Generic (`__generic`) pointers are not implemented yet. LLVM's SelectionDAG
pointer lowering currently represents pointers with a `MVT`, whose integer
pointer types are limited to the target's simple value types; declaring a
24-bit pointer in the data layout alone produces an invalid EVT during
CodeGen. A 32-bit padded pointer experiment also reached unsupported i32
comparison legalization. Do not add a `p8:24` layout or advertise generic
pointer support until the representation, three-byte ABI, address-space
conversion, and dynamic dereference paths are implemented together and
covered by codegen and firmware tests.
