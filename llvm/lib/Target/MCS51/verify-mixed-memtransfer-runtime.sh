#!/bin/sh
# Execute mixed IDATA/XDATA memcpy and memmove regressions in uCsim.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "${MCS51_BUILD:-$SOURCE_ROOT/../llvm-mcs51-build}" && pwd)
CLANG=${MCS51_CLANG:-$BUILD_ROOT/bin/clang}
LLC=${MCS51_LLC:-$BUILD_ROOT/bin/llc}
LLD=${MCS51_LLD:-$BUILD_ROOT/bin/ld.lld}
OBJCOPY=${MCS51_OBJCOPY:-$BUILD_ROOT/bin/llvm-objcopy}
SIM=${MCS51_SIM:-s51}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/mcs51-mixed.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
"$CLANG" -target mcs51 -O2 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections \
  -c "$SCRIPT_DIR/mcs51-runtime.c" -o "$WORK/runtime.o"
"$CLANG" -target mcs51 -O1 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections \
  -c "$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs/mixed-memtransfer-runtime.c" -o "$WORK/test.o"
"$LLC" -mtriple=mcs51 "${MCS51_TEST_OPT:--O1}" -verify-machineinstrs -filetype=obj \
  "$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs/mixed-memtransfer-runtime.ll" -o "$WORK/mixed.o"
"$LLD" -m elf32-mcs51 -T "$SCRIPT_DIR/test/mcs51-flat-sim.ld" \
  --gc-sections --no-check-sections -o "$WORK/test.elf" \
  "$WORK/test.o" "$WORK/mixed.o" "$WORK/runtime.o"
"$OBJCOPY" -O ihex "$WORK/test.elf" "$WORK/test.hex"
# Keep the hardware stack above the imaginary registers at 0x30..0x47.
printf '%s\n' 'set memory sfr 0x81 0x7f' 'break xram w 0x100' 'step 1000000' 'dx 0x100 0x100' 'quit' |
  "$SIM" -t C52 -q -c - "$WORK/test.hex" >"$WORK/sim.log" 2>&1 || true
if ! grep -q 'Event break' "$WORK/sim.log" || ! grep -Eq '0x0100 +00' "$WORK/sim.log"; then
  cat "$WORK/sim.log" >&2
  echo 'Mixed memory transfer runtime test failed' >&2
  exit 1
fi
echo 'Mixed IDATA/XDATA memcpy and memmove runtime test passed.'
