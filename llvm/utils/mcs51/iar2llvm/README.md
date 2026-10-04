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
python3 -m iar2llvm convert-listing /private/tmp/Router-Pro.lst \
  --module cGP_stub --symbol gp_macReq \
  --out /tmp/libzstack-data.a \
  --llvm-mc ../../../../llvm-mcs51-build/bin/llvm-mc \
  --llvm-ar ../../../../llvm-mcs51-build/bin/llvm-ar
```

The initial converter accepts only selected, zero-initialized public objects in
IAR `XDATA_Z` segments. It emits `.bss.*` ELF sections and packages them into
an archive with `llvm-ar`; this subset can resolve ordinary MCS-51 XDATA
references in Clang objects. It rejects code, initialized data, and emitted
relocations. Most Z-Stack library code is banked and still needs UBROF
relocation decoding plus IAR-to-Clang calling-convention and banking support.
The output is therefore a deliberately narrow prototype, not a converted
Z-Stack library suitable for firmware execution.
