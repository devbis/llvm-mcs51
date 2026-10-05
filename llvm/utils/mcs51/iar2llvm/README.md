# IAR UBROF to LLVM MCS-51 objects

This tool is an in-tree prototype for converting IAR 8051 libraries used by
Z-Stack into ELF objects accepted by this LLVM MCS-51 linker. IAR `.r51`
members use UBROF 11. XLIB can produce a readable listing, which the current
converter uses as input for its first supported subset.

Run it from `llvm/utils/mcs51`:

```sh
python3 -m iar2llvm inspect /path/to/Router-Pro.lib
python3 -m iar2llvm inspect --json /path/to/Router-Pro.lib
python3 -m iar2llvm extract /path/to/Router-Pro.lib --out-dir /tmp/router-pro-r51
```

Extraction preserves the full UBROF object header and records each member's
input byte range and SHA-256 digest in JSON output. UBROF record decoding is
delegated to IAR XLIB for now. In the `iar8051` container, run the interactive
XLIB command and copy its listing to the host:

```text
docker exec -it iar8051 wine /opt/iar/embedded_workbench/8051/bin/xlib.exe
LIST-OBJECT-CODE /workspace/zstack/Projects/zstack/Libraries/TI2530DB/bin/Router-Pro.lib /tmp/Router-Pro.lst
EXIT
docker exec iar8051 cat /tmp/Router-Pro.lst > /private/tmp/Router-Pro.lst
```

Then run the prototype converter from `llvm/utils/mcs51`:

```sh
python3 -m iar2llvm convert-library-listing /private/tmp/Router-Pro.lst \
  --out /tmp/Router-Pro.clang.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

This scans the listing, converts each module with public `BANKED_CODE`
or `RCODE` functions, and combines their ELF objects into one archive. It
defaults to XLIB's `LAN=00` variant when a library contains duplicate module
names. Repeat `--symbol NAME` to limit conversion to modules that define
specific public symbols, such as runtime helpers. It reports unresolved IAR
symbols that need another converted library or IAR runtime support. The
module-specific commands below are useful for inspecting a smaller slice:

```sh
python3 -m iar2llvm convert-listing /private/tmp/Router-Pro.lst \
  --module cGP_stub --symbol gp_macReq \
  --out /tmp/libzstack-data.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

The zero-initialized data converter accepts selected public objects in IAR
`XDATA_Z` segments. It emits `.bss.*` ELF sections and packages them into an
archive with `llvm-ar`; this subset can resolve ordinary MCS-51 XDATA
references in Clang objects. It rejects emitted relocations.

Absolute constants in IAR `CONST/XDATA_ROM_C` segments can also be converted
when they contain only literal bytes and have no relocations. For example, the
four scalar constants in `cGP_stub`:

```sh
python3 -m iar2llvm convert-const-listing /private/tmp/Router-Pro.lst \
  --module cGP_stub \
  --symbol gpTxOffset --symbol gpRxOffset \
  --symbol gpMaxTxOffsetVariation --symbol gpTxDuration \
  --out /tmp/libzstack-const.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

The resulting objects are emitted into ELF `.rodata.*` sections in CODE space.
The converter rejects constant tables that contain address relocations; for
example, `nwkMsgProcs` remains unsupported until those IAR relocation records
are decoded.

Literal initializers for public `XDATA_I` objects can be converted from the
paired `XDATA_ID` load-image segment into ELF `.data.*` sections. For example,
the router module's one-byte beacon payload size and two-byte saved address:

```sh
python3 -m iar2llvm convert-init-listing /private/tmp/Router-Pro.lst \
  --module nwk \
  --symbol nwk_beaconPayloadSize --symbol _savedRejoinRspAddr \
  --out /tmp/libzstack-init.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

The generated `.data.*` objects use XDATA VMAs and are intended for Clang's
CC2530 startup copy from CODE. The converter supports 16-bit pointers to public
`CODE_C` objects; other relocations and unsupported IAR initialization
metadata are rejected.

`convert-code-listing` can select one public `BANKED_CODE` or `RCODE` function
with `--symbol`, collect public `BANKED_CODE` functions with
`--all-public-banked`, or collect public banked and common code with
`--all-public-code`. The all-public forms also keep referenced code segments,
relays, literal private
constants, all public `CODE_C` and literal `CONST/XDATA_ROM_C` objects, and
public `XDATA_Z`/literal `XDATA_I` objects in the same ELF member. Public
constant and data symbols are exported so other converted modules can
reference them. Literal initializers and 16-bit relocations to public
`CODE_C` objects are supported. Other unsupported public data initializers
are reported and remain unresolved. Each IAR banked code segment gets its own
label within the module's `.mcs51.autobank.*` section; this preserves short
branches between IAR code segments while the module remains one placement unit
for CC2530 bank assignment. `CODE_C` tables are exported in read-only
common-flash sections. For example, all
currently supported code in `cGP_stub`:

```sh
python3 -m iar2llvm convert-code-listing /private/tmp/Router-Pro.lst \
  --module cGP_stub --all-public-banked \
  --out /tmp/libzstack-cgp-code.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

This remains a partial record decoder. In the selected Z-Stack RouterEB
libraries, module-wide conversion can still stop on unsupported records or
unresolved symbol forms. The converter reports the first unsupported record
and does not write a partial archive. The generated objects preserve the IAR
machine code and supported relocations, but do not adapt IAR calling
conventions or runtime symbols to Clang's ABI.

For the first executable slice, produce an XLIB listing of `ecc.r51` and
convert one public banked function together with its relay and referenced code
segments:

```sh
python3 -m iar2llvm convert-code-listing /private/tmp/iar2llvm-ecc.lst \
  --module eccapi --symbol ZSE_ECDSASign \
  --out /tmp/libiar-ecc.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

The code subset preserves machine bytes and translates supported `SJMP`,
`LCALL`/`LJMP`, DPTR-address, 8/16-bit data, high/low-byte, IAR `?VB` virtual
bit-address, and 24-bit banked-code relocations. `R_8051_24` writes
address-low, address-high, and the final flash bank number after CC2530
automatic bank placement. `R_8051_BIT8` converts the IAR virtual-bit byte
address and bit offset into the 8051 bit address. Symbols with IAR-only
punctuation are exported under stable `__iar_sym_<hex-name>` ELF names; JSON
output lists those mappings and unresolved IAR runtime names.

This does not adapt an IAR function body to the standard Clang calling
convention. `-mabi=iar` opts into the current scalar register ABI bring-up;
stack-passed arguments, aggregate arguments and returns, and IAR's XDATA
reentrant frames are not implemented yet. The converted relay still depends
on IAR bank-switching runtime routines, and banked functions use IAR's 24-bit
return-address convention. This archive is therefore a linker-input
milestone, not yet callable arbitrary Z-Stack code or a deployable firmware
library. Other UBROF expression records, relocatable constants, and initialized
XDATA data outside the supported `XDATA_ID` subset are still unsupported by
the converter.
