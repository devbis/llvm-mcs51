#!/bin/sh
# Regression reproducer: an ordinary pointer to an IDATA local escapes to
# separately compiled code. This currently fails because default-space
# accesses do not reach the stack alias. Expected bytes: a5 42 69.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "${MCS51_BUILD:-$SOURCE_ROOT/../llvm-mcs51-build}" && pwd)
CLANG=${MCS51_CLANG:-$BUILD_ROOT/bin/clang}
OBJCOPY=${MCS51_OBJCOPY:-$BUILD_ROOT/bin/llvm-objcopy}
MCUEMU=${MCUEMU:-mcuemu}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/cc2530-stack-pointer.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
INPUTS="$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs"
"$CLANG" -target mcs51 -mcpu=cc2530 "${MCS51_TEST_OPT:--O1}" \
  -c "$INPUTS/cc2530-stack-pointer-callee.c" -o "$WORK/callee.o"
for mode in control ordinary; do
  if [ "$mode" = control ]; then
    define=-DMANUAL_STACK_ALIAS
  else
    define=-UMANUAL_STACK_ALIAS
  fi
  "$CLANG" -target mcs51 -mcpu=cc2530 "${MCS51_TEST_OPT:--O1}" "$define" \
    -c "$INPUTS/cc2530-stack-pointer-main.c" -o "$WORK/main.o"
  "$CLANG" -target mcs51 -mcpu=cc2530 "$WORK/main.o" "$WORK/callee.o" \
    -o "$WORK/test.elf"
  "$OBJCOPY" -O ihex "$WORK/test.elf" "$WORK/test.hex"
  "$MCUEMU" run --model CC2530F256 --firmware "$WORK/test.hex" \
    --steps 1000000 --dump-xdata 0x1e00:4 > "$WORK/emulator.log" 2>&1
  if ! grep -Eq '^xdata 0x1e00: a5 42 69( |$)' "$WORK/emulator.log" || \
     ! grep -q '^stop: idle self-loop' "$WORK/emulator.log"; then
    cat "$WORK/emulator.log" >&2
    echo "CC2530 stack pointer test failed ($mode)" >&2
    exit 1
  fi
  echo "CC2530 stack pointer test passed ($mode)."
done
echo 'CC2530 ordinary pointer to stack object passed.'
