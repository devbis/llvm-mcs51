#!/bin/sh
# Executes the 64-bit division and remainder helpers in uCsim.
# Needs uCsim's s51 on PATH or in MCS51_SIM.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "${MCS51_BUILD:-$SOURCE_ROOT/../llvm-mcs51-build}" && pwd)

CLANG=${MCS51_CLANG:-$BUILD_ROOT/bin/clang}
LLD=${MCS51_LLD:-$BUILD_ROOT/bin/ld.lld}
OBJCOPY=${MCS51_OBJCOPY:-$BUILD_ROOT/bin/llvm-objcopy}
SIM=${MCS51_SIM:-s51}
OPT_LEVEL=${MCS51_OPT_LEVEL:-2}
TMP=${TMPDIR:-/tmp}
WORK=$(mktemp -d "$TMP/mcs51-div64.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

if ! command -v "$SIM" >/dev/null 2>&1; then
  echo "uCsim executable not found: $SIM" >&2
  exit 2
fi

"$CLANG" -target mcs51 -O"$OPT_LEVEL" -ffreestanding -fno-builtin -ffunction-sections \
  -fdata-sections -c "$SCRIPT_DIR/mcs51-runtime.c" -o "$WORK/runtime.o"
"$CLANG" -target mcs51 -O"$OPT_LEVEL" -ffreestanding -fno-builtin -ffunction-sections \
  -c "$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs/division64-runtime.c" \
  -o "$WORK/test.o"
"$LLD" -m elf32-mcs51 -T "$SCRIPT_DIR/test/mcs51-flat-sim.ld" \
  --gc-sections --no-check-sections -o "$WORK/test.elf" \
  "$WORK/test.o" "$WORK/runtime.o"
"$OBJCOPY" -O ihex "$WORK/test.elf" "$WORK/test.hex"

printf '%s\n' 'break xram w 0x100' 'run' 'dx 0x100 0x110' 'quit' |
  "$SIM" -q -c - "$WORK/test.hex" >"$WORK/sim.log" 2>&1 || true

if ! grep -q "Event break" "$WORK/sim.log" || \
   ! grep -Eq '0x0100 +00' "$WORK/sim.log"; then
  cat "$WORK/sim.log" >&2
  echo "64-bit division runtime test failed" >&2
  exit 1
fi

echo "64-bit division and remainder runtime test passed."
