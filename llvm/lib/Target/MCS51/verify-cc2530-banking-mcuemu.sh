#!/bin/sh
# Execute same-bank, cross-bank, indirect and stack-argument calls on CC2530.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "${MCS51_BUILD:-$SOURCE_ROOT/../llvm-mcs51-build}" && pwd)
CLANG=${MCS51_CLANG:-$BUILD_ROOT/bin/clang}
OBJCOPY=${MCS51_OBJCOPY:-$BUILD_ROOT/bin/llvm-objcopy}
OBJDUMP=${MCS51_OBJDUMP:-$BUILD_ROOT/bin/llvm-objdump}
MCUEMU=${MCUEMU:-mcuemu}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/cc2530-bank.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
INPUTS="$SOURCE_ROOT/llvm/test/CodeGen/8051/Inputs"
for unit in main callee; do
  "$CLANG" -target mcs51 -mcpu=cc2530 "${MCS51_TEST_OPT:--O1}" \
    -c "$INPUTS/cc2530-bank-emulator-$unit.c" -o "$WORK/$unit.o"
done
"$CLANG" -target mcs51 -mcpu=cc2530 "$WORK/main.o" "$WORK/callee.o" \
  -o "$WORK/test.elf"
"$OBJCOPY" -O ihex "$WORK/test.elf" "$WORK/test.hex"
"$OBJDUMP" -d "$WORK/test.elf" > "$WORK/disassembly.txt"
# Assert the exact linked call targets, not merely the runtime result: the
# executable must exercise the bypass and retain the unsafe trampolines.
python3 - "$WORK/disassembly.txt" <<'PY'
import re
import sys

symbols = {}
calls = {}
current = None
for line in open(sys.argv[1], encoding="utf-8"):
    label = re.match(r"^([0-9a-fA-F]+) <([^>]+)>:$", line)
    if label:
        name = label.group(2)
        if not name.startswith("."):
            current = name
            symbols[name] = int(label.group(1), 16)
            calls[current] = []
        continue
    call = re.search(r"\blcall\s+(\d+)", line)
    if call and current:
        calls[current].append(int(call.group(1)))

required = ("caller", "cross", "simple", "__mcs51_bankcall_simple",
            "__mcs51_bankcall_wide", "__mcs51_bankcall_asmf")
if any(name not in symbols for name in required):
    sys.exit("Missing expected bank-test symbols")
if symbols["simple"] not in calls["caller"]:
    sys.exit("Same-bank direct call did not bypass the trampoline")
if symbols["__mcs51_bankcall_simple"] not in calls["cross"]:
    sys.exit("Cross-bank call did not use the trampoline")
for name in ("__mcs51_bankcall_wide", "__mcs51_bankcall_asmf"):
    if symbols[name] not in calls["caller"]:
        sys.exit("Unsafe same-bank call bypassed " + name)
PY
"$MCUEMU" run --model CC2530F256 --firmware "$WORK/test.hex" \
  --steps 1000000 --dump-xdata 0x1e00:4 > "$WORK/emulator.log" 2>&1
if ! grep -Eq '^xdata 0x1e00: a5 00 01( |$)' "$WORK/emulator.log" || \
   ! grep -q '^stop: idle self-loop' "$WORK/emulator.log"; then
  cat "$WORK/emulator.log" >&2
  echo 'CC2530 bank call runtime test failed' >&2
  exit 1
fi
echo 'CC2530 same-bank, cross-bank, indirect and stack-argument calls passed.'
