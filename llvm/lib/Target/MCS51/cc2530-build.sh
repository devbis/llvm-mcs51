#!/bin/sh

set -eu

if [ "$#" -eq 0 ]; then
  echo "usage: $0 source-or-object... -o firmware.elf" >&2
  exit 2
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CLANG=${MCS51_CLANG:-clang}
OBJCOPY=${MCS51_OBJCOPY:-llvm-objcopy}
TEMP_ROOT=${TMPDIR:-/tmp}
TEMP_DIR=$(mktemp -d "$TEMP_ROOT/mcs51-cc2530.XXXXXX")
trap 'rm -rf "$TEMP_DIR"' EXIT
OUTPUT=a.out
EXPECT_OUTPUT=0

for ARG do
  if [ "$EXPECT_OUTPUT" -eq 1 ]; then
    OUTPUT=$ARG
    EXPECT_OUTPUT=0
    continue
  fi
  case "$ARG" in
  -o)
    EXPECT_OUTPUT=1
    ;;
  -o*)
    OUTPUT=${ARG#-o}
    ;;
  esac
done

if [ "$EXPECT_OUTPUT" -eq 1 ]; then
  echo "error: -o requires an output path" >&2
  exit 2
fi

"$CLANG" -target mcs51 -mcpu=cc2530 -O2 -ffreestanding -fno-builtin \
  -ffunction-sections -fdata-sections -c "$SCRIPT_DIR/mcs51-runtime.c" \
  -mllvm -verify-machineinstrs \
  -o "$TEMP_DIR/mcs51-runtime.o"

"$CLANG" -target mcs51 -mcpu=cc2530 -ffreestanding -fno-builtin \
  -ffunction-sections -fdata-sections -nostdlib \
  "$SCRIPT_DIR/cc2530_startup.s" "$@" \
  -Wl,-T,"$SCRIPT_DIR/cc2530.ld" -Wl,--no-check-sections \
  -Wl,--gc-sections "$TEMP_DIR/mcs51-runtime.o"

HEX_OUTPUT=${OUTPUT%.*}.hex
"$OBJCOPY" --output-target=ihex "$OUTPUT" "$HEX_OUTPUT"
