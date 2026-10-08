# MCS-51 frame overlay

The experimental module pass can move eligible fixed-size local objects into
one static arena and reuse function ranges when their functions cannot be
active at the same time. Enable it with `llc -mcs51-overlay`, or through Clang
with `-mllvm -mcs51-overlay`. It is disabled by default. The hidden
`-mcs51-overlay-space` option accepts `auto`, `data`, `idata`, or `xdata`.
`auto` uses direct DATA when every candidate is a scalar with direct accesses.
If any candidate needs indexed access, it puts all candidates in one IDATA
arena so direct-only objects can share offsets with non-interfering arrays.
When that arena does not fit the configured internal budget, it falls back to
XDATA. The internal budget defaults to 80 bytes and can be changed with
`-mcs51-overlay-internal-limit`.

The first implementation is deliberately conservative. It handles entry-block
allocas whose addresses do not escape, and excludes recursive functions,
interrupt handlers and their callees, externally visible functions, and
functions whose addresses are taken. Indirect calls disable range sharing
between eligible functions. Spill slots and unsafe or dynamic allocas remain
on the hardware stack. XDATA arenas are emitted in `.bss.mcs51.overlay`; DATA
and IDATA arenas use the target's normal address-space sections. DATA and IDATA
are treated as one capacity pool because their low addresses alias. The CC2530
profile's linker script is the final check for internal-RAM overflow and
XDATA/IDATA alias conflicts.

Cross-translation-unit sharing requires regular full LTO (`-flto`) so the
pass sees one call graph. The current MCS-51 Clang driver cannot pass LLVM
bitcode to its linker, so this mode is not yet available for ordinary linked
firmware builds. Without LTO, only internal-linkage functions are considered.
This does not implement the complete IAR/Keil model: spill slots remain on the
hardware stack, and linker placement across separately produced objects is not
implemented. The internal-space estimate only sees globals in the current
LLVM module; non-LTO builds with additional DATA/IDATA globals in other objects
must set the internal budget conservatively or select XDATA explicitly.

# MCS-51 pointer representation

Each MCS-51 memory space is a separate LLVM address space. Clang
(`clang/lib/Basic/Targets/MCS51.h`) and the target DataLayout
(`MCS51TargetMachine.cpp`) use the same pointer widths, and the backend lowers
every pointer to exactly that many bytes. All pointers have byte alignment.

| Qualifier   | AS | Width | DataLayout | Address bits                   |
|-------------|----|-------|------------|--------------------------------|
| (none)      | 0  | 2     | `p:16:8`   | XDATA, or IDATA for locals     |
| `__data`    | 1  | 1     | `p1:8:8`   | direct internal RAM 0x00-0x7F  |
| `__idata`   | 2  | 1     | `p2:8:8`   | indirect internal RAM 0x00-0xFF|
| `__pdata`   | 3  | 1     | `p3:8:8`   | offset in the MPAGE XDATA page |
| `__xdata`   | 4  | 2     | `p4:16:8`  | external RAM 0x0000-0xFFFF     |
| `__code`    | 5  | 2     | `p5:16:8`  | code memory 0x0000-0xFFFF      |
| `__bit`     | 6  | 1     | `p6:8:8`   | bit address (no pointers)      |
| `__sfr`     | 7  | 1     | `p7:8:8`   | SFR address (no pointers)      |
| `__generic` | 8  | 4     | `p8:32:8`  | tagged, see below              |

Because DATA, IDATA and PDATA pointers are one byte wide, Clang rejects
objects larger than 255 bytes in those spaces.

# Global storage and initialized XDATA

An unqualified global, including a `const` global, uses the default XDATA
address space. `const` prevents C writes through the object's type; it does
not move the object to CODE. Use `__code` (or `CODE`) to select CODE. String
literals retain their historical CODE backing; when a literal is converted to
an XDATA pointer, Clang emits an initialized XDATA copy, while direct constant
reads use CODE. Clang emits default-space initialized globals in
`.mcs51.xdata.init[.<name>]` and zero-initialized globals in
`.mcs51.xdata.bss[.<name>]`. Explicit CODE objects use
`.mcs51.code.rodata[.<name>]`. These section names retain the bus identity
through section splitting and link-time optimization.

The CC2530 linker script and the CA51F253L3 profile place initialized XDATA
sections at XDATA run addresses with CODE load images. Their reset startup
copies the load range to XDATA before calling `main`; the XDATA BSS range uses
the existing clear path. LLD rejects initialized XDATA sections unless they
are part of the startup-managed `.data` output range and the startup boundary
symbols are defined.

Compiler-generated global-address relocations carry CODE or XDATA identity.
The assembly spelling `#xlo8(symbol)`, `#xhi8(symbol)`, `#clo8(symbol)`,
`#chi8(symbol)`, `#xdata16(symbol)`, and `#code16(symbol)` preserves this
metadata when generated assembly is assembled separately. LLD checks the
relocation's space against the target input section, even when CODE and XDATA
have the same numeric address, and reports both sections on a mismatch.
Generic hand-written `lo8`, `hi8`, and `mov dptr, #symbol` expressions remain
untyped; use the tagged forms when the address space is known.

A generic pointer is four bytes in memory order: address low, address high,
tag, and a zero padding byte. As an `i32` this is
`address | tag << 16`. The tag selects the access instruction in the runtime
helpers `__mcs51_gptr_read_byte`/`__mcs51_gptr_write_byte`:

| Source space        | Tag  | Address bytes            | Access          |
|---------------------|------|--------------------------|-----------------|
| XDATA, plain global | 0x00 | 16-bit address           | `movx @dptr`    |
| DATA, IDATA, locals | 0x40 | 8-bit address, high = 0  | `mov @r0`       |
| PDATA               | 0x60 | 8-bit offset, high = 0   | `movx @r0`      |
| CODE                | 0x80 | 16-bit address           | `movc @a+dptr`  |

Conversions:

* To generic: zero-extend the address to 16 bits and add the tag. A plain
  (AS0) pointer gets tag 0x40 when it is derived from a local stack object
  (whose address is an IDATA address), otherwise tag 0x00.
* From generic: take the low 16 bits (XDATA, CODE, plain) or the low 8 bits
  (DATA, IDATA, PDATA). The tag is not checked, as in other MCS-51
  compilers; converting to the wrong space is undefined.
* BIT and SFR addresses are not indirectly addressable and cannot be
  converted to a generic pointer; the backend reports an error.
* Null is address 0 in every space and all four bytes zero for generic. A
  non-generic null converts to generic null (the tag is only added for a
  non-zero address), so null comparisons are preserved. As a consequence, a
  real object at address 0 of its space compares equal to null.

In static initializers the converted address is emitted as its own 1- or
2-byte relocated field followed by constant tag and padding bytes, because
there is no 32-bit data relocation; `(u8 __generic *)d + 2` for a DATA array
`d` becomes `.byte d+2, 0, 0x40, 0`. Clang does not constant-fold a
conversion of an integer-valued pointer between spaces of different pointer
widths, since it is not an identity on the integer: such an expression is
lowered at run time in code, and is not accepted as a static initializer when
pointer arithmetic is applied to it.

Equality compares all 32 bits; pointer arithmetic changes only the 16 address
bits. Generic pointers are passed and returned in four bytes like any other
32-bit value.

Examples (bytes in memory order):

| C expression                                   | Bytes         |
|------------------------------------------------|---------------|
| `(u8 __generic *)(u8 __data *)0x30`            | `30 00 40 00` |
| `(u8 __generic *)(u8 __idata *)0x90`           | `90 00 40 00` |
| `(u8 __generic *)(u8 __pdata *)0x12`           | `12 00 60 00` |
| `(u8 __generic *)(u8 __xdata *)0x1234`         | `34 12 00 00` |
| `(u8 __generic *)(const u8 __code *)0x8000`    | `00 80 80 00` |
| `(u8 __generic *)(u8 __data *)0`               | `00 00 00 00` |
| `(u8 __generic *)(u8 __data *)0x30 + 3`        | `33 00 40 00` |

# Runtime simulator checks

`verify-div64-runtime.sh` builds the MCS-51 runtime and a flat test image,
then runs it in uCsim. It checks unsigned 64-bit quotient and remainder
helpers and signed 64-bit remainder against fixed expected values. Set
`MCS51_SIM` to the `s51` executable when it is not on `PATH`.
