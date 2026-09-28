# MCS-51 frame overlay

The experimental module pass can move eligible fixed-size local objects into
one static XDATA arena and reuse function ranges when their functions cannot
be active at the same time. Enable it with `llc -mcs51-overlay`, or through
Clang with `-mllvm -mcs51-overlay`. It is disabled by default.

The first implementation is deliberately conservative. It handles entry-block
allocas whose addresses do not escape, and excludes recursive functions,
interrupt handlers and their callees, externally visible functions, and
functions whose addresses are taken. Indirect calls disable range sharing
between eligible functions. Spill slots and unsafe or dynamic allocas remain
on the hardware stack. Overlay bytes are emitted in `.bss.mcs51.overlay`, so
the normal MCS-51 XDATA linker placement and startup clearing apply.

Cross-translation-unit sharing requires regular full LTO (`-flto`) so the
pass sees one call graph. The current MCS-51 Clang driver cannot pass LLVM
bitcode to its linker, so this mode is not yet available for ordinary linked
firmware builds. Without LTO, only internal-linkage functions are considered.
This does not implement the complete IAR/Keil model: spill slots remain on the
hardware stack, and linker placement across separately produced objects is
not implemented.
