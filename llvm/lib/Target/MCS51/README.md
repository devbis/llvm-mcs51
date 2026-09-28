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
