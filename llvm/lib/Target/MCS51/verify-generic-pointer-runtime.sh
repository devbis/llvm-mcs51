#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "$SOURCE_ROOT/../llvm-mcs51-build" && pwd)

CLANG=${MCS51_CLANG:-$BUILD_ROOT/bin/clang}
LLD=${MCS51_LLD:-$BUILD_ROOT/bin/ld.lld}
OBJCOPY=${MCS51_OBJCOPY:-$BUILD_ROOT/bin/llvm-objcopy}
SIM=${MCS51_SIM:-s51}
TMP=${TMPDIR:-/tmp}
WORK=$(mktemp -d "$TMP/mcs51-gptr.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

"$CLANG" -target mcs51 -O2 -ffunction-sections -fdata-sections \
  -c "$SCRIPT_DIR/mcs51-runtime.c" -o "$WORK/runtime.o"
"$CLANG" -target mcs51 -O2 -ffreestanding -fno-builtin \
  -c "$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs/generic-pointer-runtime.c" \
  -o "$WORK/test.o"
"$LLD" -m elf32-mcs51 -T "$SCRIPT_DIR/test/mcs51-flat-sim.ld" \
  --gc-sections --no-check-sections -o "$WORK/test.elf" \
  "$WORK/test.o" "$WORK/runtime.o"
"$OBJCOPY" -O ihex "$WORK/test.elf" "$WORK/test.hex"

printf '%s\n' 'break xram w 0x100' 'run' 'dx 0x100 0x100' 'quit' |
  "$SIM" -q -c - "$WORK/test.hex" >"$WORK/sim.log"

if ! grep -q "Event break" "$WORK/sim.log" || \
   ! grep -Eq '0x0100 +00' "$WORK/sim.log"; then
  cat "$WORK/sim.log" >&2
  echo "generic-pointer runtime test failed" >&2
  exit 1
fi

echo "Generic-pointer runtime test passed" \
  "(CODE/XDATA/PDATA/DATA/IDATA and 64-bit round-trip)."
