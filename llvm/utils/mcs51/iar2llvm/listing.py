from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


class ListingError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Segment:
    ident: str
    address_space: str
    name: str


@dataclass(frozen=True, slots=True)
class Symbol:
    name: str
    segment: str
    offset: int


def _records(lines: list[str]) -> Iterator[tuple[str, str]]:
    """Yield XLIB listing records with wrapped continuation lines joined."""
    index = 0
    start_re = re.compile(r"^\s{4}(t_[A-Za-z0-9_]+)\s*(.*)$")
    while index < len(lines):
        match = start_re.match(lines[index])
        if not match:
            index += 1
            continue
        tag, text = match.groups()
        parts = [text.strip()]
        index += 1
        while index < len(lines) and not start_re.match(lines[index]):
            continuation = lines[index].strip()
            if continuation:
                if parts and parts[-1].count('"') % 2:
                    parts[-1] += continuation
                else:
                    parts.append(continuation)
            index += 1
        yield tag, " ".join(parts)


def _modules(listing: str) -> dict[str, list[tuple[str, str]]]:
    modules: dict[str, list[tuple[str, str]]] = {}
    current_name: str | None = None
    current_records: list[tuple[str, str]] = []
    for record in _records(listing.splitlines()):
        tag, text = record
        if tag == "t_beg_lib":
            if current_name is not None:
                if current_name in modules:
                    raise ListingError(
                        f"duplicate module {current_name!r} in XLIB listing"
                    )
                modules[current_name] = current_records
            match = re.search(r"'([^']+)'", text)
            if not match:
                raise ListingError("XLIB t_beg_lib record has no module name")
            current_name = match.group(1)
            current_records = []
        elif current_name is not None:
            current_records.append(record)
    if current_name is not None:
        if current_name in modules:
            raise ListingError(
                f"duplicate module {current_name!r} in XLIB listing"
            )
        modules[current_name] = current_records
    if not modules:
        raise ListingError("no XLIB t_beg_lib module records found")
    return modules


def _selected_bss(listing: str, module_name: str, symbol_names: list[str]):
    modules = _modules(listing)
    if module_name not in modules:
        raise ListingError(f"module {module_name!r} is not present in the XLIB listing")

    records = modules[module_name]
    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
    definitions_by_segment: dict[str, list[str]] = {}
    for tag, text in records:
        if tag == "t_seg":
            match = re.search(
                r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                text,
            )
            if match:
                ident, address_space, name = match.groups()
                segments[ident.upper()] = Segment(ident.upper(), address_space, name)
        elif tag == "t_symbol_def2":
            match = re.search(
                r"([A-Z_]+_REL)\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                kind, segment, offset, name = match.groups()
                segment = segment.upper()
                definitions_by_segment.setdefault(segment, []).append(name)
                if kind == "PUBLIC_REL":
                    symbols[name] = Symbol(name, segment, int(offset, 16))

    missing = [name for name in symbol_names if name not in symbols]
    if missing:
        raise ListingError("public symbols not found: " + ", ".join(missing))

    by_segment: dict[str, list[Symbol]] = {}
    for name in symbol_names:
        symbol = symbols[name]
        by_segment.setdefault(symbol.segment, []).append(symbol)

    extents: dict[str, int] = {}
    discarded_expression_ops: dict[str, list[str]] = {}
    emitted_bytes: set[str] = set()
    current_segment: str | None = None
    cursor = 0
    for tag, text in records:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if match:
                current_segment = match.group(1).upper()
                cursor = int(match.group(2), 16)
                extents[current_segment] = max(extents.get(current_segment, 0), cursor)
            continue
        if current_segment not in by_segment:
            continue
        if tag in ("t_push_ext", "t_delete_tos"):
            discarded_expression_ops.setdefault(current_segment, []).append(tag)
            continue
        if tag.startswith("t_push_") or tag.startswith("t_pop_"):
            discarded_expression_ops.setdefault(current_segment, []).append(tag)
            continue
        if tag in ("t_abs_8", "t_abs_16", "t_abs_24", "t_abs_32"):
            digits = {"t_abs_8": 2, "t_abs_16": 4, "t_abs_24": 6, "t_abs_32": 8}[tag]
            match = re.fullmatch(r"[0-9A-Fa-f]{%d}" % digits, text)
            if not match:
                raise ListingError(f"malformed {tag} record in segment {current_segment}")
            emitted_bytes.add(current_segment)
            cursor += digits // 2
            extents[current_segment] = max(extents.get(current_segment, 0), cursor)
        elif tag.startswith("t_abs_"):
            raise ListingError(f"unsupported absolute emission record {tag}")
        elif tag != "t_asm_mode_change":
            raise ListingError(
                f"unsupported XLIB record {tag} in selected XDATA_Z segment "
                f"{current_segment}"
            )

    result: list[tuple[Symbol, Segment, int, list[str]]] = []
    for segment_id, segment_symbols in by_segment.items():
        segment = segments.get(segment_id)
        if segment is None:
            raise ListingError(f"symbol refers to missing segment {segment_id}")
        if segment.address_space != "XDATA" or segment.name != "XDATA_Z":
            raise ListingError(
                f"{', '.join(s.name for s in segment_symbols)} is in unsupported "
                f"IAR segment {segment.name} ({segment.address_space}); this prototype "
                "converts zero-initialized XDATA_Z objects only"
            )
        if len(segment_symbols) != 1:
            raise ListingError(
                f"segment {segment_id} contains multiple selected symbols; "
                "their individual sizes are not known in this prototype"
            )
        symbol = segment_symbols[0]
        if definitions_by_segment.get(segment_id) != [symbol.name]:
            raise ListingError(
                f"segment {segment_id} has additional relative symbols; "
                "their individual sizes are not known in this prototype"
            )
        size = extents.get(segment_id, 0) - symbol.offset
        if size <= 0:
            raise ListingError(f"could not determine a nonzero size for {symbol.name}")
        if symbol.offset != 0:
            raise ListingError("this prototype only supports symbols at segment offset zero")
        if segment_id in emitted_bytes:
            raise ListingError(
                f"segment {segment_id} contains initialized bytes; this prototype only "
                "converts zero-initialized objects"
            )

        ops = discarded_expression_ops.get(segment_id, [])
        invalid_ops = any(op not in ("t_push_ext", "t_delete_tos") for op in ops)
        unmatched_discard = ops.count("t_push_ext") != ops.count("t_delete_tos")
        if ops and (invalid_ops or unmatched_discard):
            raise ListingError(
                f"segment {segment_id} has an emitted or unsupported relocation expression"
            )
        result.append((symbol, segment, size, ops))

    return result


def _default_tool(name: str) -> Path:
    repo_root = Path(__file__).resolve().parents[4]
    candidate = repo_root.parent / "llvm-mcs51-build" / "bin" / name
    if candidate.exists():
        return candidate
    located = shutil.which(name)
    if located:
        return Path(located)
    raise ListingError(f"cannot find {name}; pass its path explicitly")


def convert_listing_to_archive(
    listing_path: Path,
    module_name: str,
    symbol_names: list[str],
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
) -> dict[str, object]:
    """Convert selected, zero-initialized IAR XDATA symbols into an ELF archive.

    This intentionally handles one data-only UBROF/XLIB subset. Executable
    records, initializers, and emitted relocations are rejected rather than
    approximated.
    """
    listing = listing_path.read_text(encoding="latin1")
    selected = _selected_bss(listing, module_name, symbol_names)
    llvm_mc = llvm_mc or _default_tool("llvm-mc")
    llvm_ar = llvm_ar or _default_tool("llvm-ar")

    assembly: list[str] = ["; Generated from an IAR XLIB listing by iar2llvm."]
    for symbol, _segment, size, _ops in selected:
        if not re.fullmatch(r"[A-Za-z_.$][A-Za-z0-9_.$]*", symbol.name):
            raise ListingError(
                f"symbol name cannot be emitted in LLVM assembly: {symbol.name!r}"
            )
        assembly.extend(
            [
                f'.section .bss.{symbol.name},"aw",@nobits',
                ".balign 1",
                f".globl {symbol.name}",
                f".type {symbol.name},@object",
                f"{symbol.name}:",
                f".zero {size}",
                f".size {symbol.name}, {size}",
            ]
        )

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="iar2llvm-", dir=output.parent) as temp:
        temp_dir = Path(temp)
        asm_path = temp_dir / f"{module_name}.s"
        object_path = temp_dir / f"{module_name}.o"
        archive_path = temp_dir / output.name
        asm_path.write_text("\n".join(assembly) + "\n", encoding="utf-8")

        assemble = subprocess.run(
            [
                str(llvm_mc),
                "-triple",
                "mcs51",
                "-filetype=obj",
                str(asm_path),
                "-o",
                str(object_path),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if assemble.returncode:
            raise ListingError(
                f"llvm-mc failed ({assemble.returncode}):\n{assemble.stderr.strip()}"
            )
        archive = subprocess.run(
            [str(llvm_ar), "rcs", str(archive_path), str(object_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if archive.returncode:
            raise ListingError(
                f"llvm-ar failed ({archive.returncode}):\n{archive.stderr.strip()}"
            )
        archive_path.replace(output)

    return {
        "module": module_name,
        "output": str(output),
        "symbols": [
            {"name": symbol.name, "size": size}
            for symbol, _, size, _ in selected
        ],
        "format": "ELF32 MCS-51 archive",
        "scope": "zero-initialized XDATA_Z objects only",
    }


def _selected_code_constants(
    listing: str, module_name: str, symbol_names: list[str]
) -> list[tuple[Symbol, int, bytes]]:
    modules = _modules(listing)
    if module_name not in modules:
        raise ListingError(f"module {module_name!r} is not present in the XLIB listing")

    records = modules[module_name]
    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
    definitions_by_segment: dict[str, list[str]] = {}
    for tag, text in records:
        if tag == "t_seg":
            match = re.search(
                r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                text,
            )
            if match:
                ident, address_space, name = match.groups()
                segments[ident.upper()] = Segment(ident.upper(), address_space, name)
        elif tag == "t_symbol_def2":
            match = re.search(
                r"([A-Z_]+_REL)\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                kind, segment, offset, name = match.groups()
                segment = segment.upper()
                definitions_by_segment.setdefault(segment, []).append(name)
                if kind == "PUBLIC_REL":
                    symbols[name] = Symbol(name, segment, int(offset, 16))

    if not symbol_names:
        raise ListingError("select at least one public constant")
    requested: list[Symbol] = []
    for name in symbol_names:
        symbol = symbols.get(name)
        if symbol is None:
            raise ListingError(f"public constant {name!r} was not found")
        segment = segments.get(symbol.segment)
        if segment is None or (segment.address_space, segment.name) != (
            "CONST",
            "XDATA_ROM_C",
        ):
            raise ListingError(
                f"{name} is not in an IAR CONST/XDATA_ROM_C segment"
            )
        if symbol.offset != 0:
            raise ListingError(f"nonzero symbol offset is unsupported for {name}")
        if definitions_by_segment.get(symbol.segment) != [name]:
            raise ListingError(
                f"segment {symbol.segment} has additional relative symbols; "
                "their individual sizes are not known in this prototype"
            )
        requested.append(symbol)

    selected_segments = {symbol.segment for symbol in requested}
    extents: dict[str, int] = {}
    bytes_by_segment: dict[str, dict[int, int]] = {}
    current_segment: str | None = None
    cursor = 0
    for tag, text in records:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match:
                current_segment = None
                continue
            current_segment = match.group(1).upper()
            cursor = int(match.group(2), 16)
            if current_segment in selected_segments:
                extents[current_segment] = max(
                    extents.get(current_segment, 0), cursor
                )
                bytes_by_segment.setdefault(current_segment, {})
            continue
        if current_segment not in selected_segments:
            continue
        if tag in ("t_abs_8", "t_abs_16", "t_abs_24", "t_abs_32"):
            digits = {
                "t_abs_8": 2,
                "t_abs_16": 4,
                "t_abs_24": 6,
                "t_abs_32": 8,
            }[tag]
            if not re.fullmatch(r"[0-9A-Fa-f]{%d}" % digits, text):
                raise ListingError(
                    f"malformed {tag} record in constant segment {current_segment}"
                )
            emitted = bytes.fromhex(text)
            target = bytes_by_segment[current_segment]
            for offset, value in enumerate(emitted, cursor):
                if offset in target:
                    raise ListingError(
                        f"overlapping constant bytes in segment {current_segment}"
                    )
                target[offset] = value
            cursor += len(emitted)
            extents[current_segment] = max(extents[current_segment], cursor)
        elif tag not in ("t_asm_mode_change", "t_stack_error"):
            raise ListingError(
                f"unsupported XLIB record {tag} in selected constant segment "
                f"{current_segment}; relocatable constants are not supported"
            )

    result: list[tuple[Symbol, int, bytes]] = []
    for symbol in requested:
        end = extents.get(symbol.segment, 0)
        size = end - symbol.offset
        if size <= 0:
            raise ListingError(f"could not determine a nonzero size for {symbol.name}")
        contents = bytes_by_segment.get(symbol.segment, {})
        if any(offset not in contents for offset in range(symbol.offset, end)):
            raise ListingError(
                f"constant {symbol.name} has uninitialized gaps in segment "
                f"{symbol.segment}"
            )
        result.append(
            (symbol, size, bytes(contents[offset] for offset in range(symbol.offset, end)))
        )
    return result


def convert_const_listing_to_archive(
    listing_path: Path,
    module_name: str,
    symbol_names: list[str],
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
) -> dict[str, object]:
    """Convert selected absolute IAR CONST/XDATA_ROM_C data to ELF CODE sections."""
    selected = _selected_code_constants(
        listing_path.read_text(encoding="latin1"), module_name, symbol_names
    )
    llvm_mc = llvm_mc or _default_tool("llvm-mc")
    llvm_ar = llvm_ar or _default_tool("llvm-ar")

    assembly: list[str] = ["; Generated from an IAR XLIB listing by iar2llvm."]
    for symbol, size, contents in selected:
        alias = _asm_name(symbol.name)
        assembly.extend(
            [
                f'.section .rodata.{alias},"a",@progbits',
                ".balign 1",
                f".globl {alias}",
                f".type {alias},@object",
                f"{alias}:",
            ]
        )
        assembly.extend(
            ".byte "
            + ", ".join(
                f"0x{byte:02x}" for byte in contents[offset : offset + 16]
            )
            for offset in range(0, size, 16)
        )
        assembly.append(f".size {alias}, {size}")

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="iar2llvm-", dir=output.parent) as temp:
        temp_dir = Path(temp)
        module_token = module_name.encode("utf-8").hex()
        asm_path = temp_dir / f"{module_token}.s"
        object_path = temp_dir / f"{module_token}.o"
        archive_path = temp_dir / output.name
        asm_path.write_text("\n".join(assembly) + "\n", encoding="utf-8")

        assemble = subprocess.run(
            [
                str(llvm_mc),
                "-triple",
                "mcs51",
                "-filetype=obj",
                str(asm_path),
                "-o",
                str(object_path),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if assemble.returncode:
            raise ListingError(
                f"llvm-mc failed ({assemble.returncode}):\n{assemble.stderr.strip()}"
            )
        archive = subprocess.run(
            [str(llvm_ar), "rcs", str(archive_path), str(object_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if archive.returncode:
            raise ListingError(
                f"llvm-ar failed ({archive.returncode}):\n{archive.stderr.strip()}"
            )
        archive_path.replace(output)

    return {
        "module": module_name,
        "output": str(output),
        "symbols": [
            {"name": symbol.name, "size": size} for symbol, size, _ in selected
        ],
        "format": "ELF32 MCS-51 archive",
        "scope": "absolute CONST/XDATA_ROM_C objects in CODE; relocations rejected",
    }


def _selected_initialized_xdata(
    listing: str, module_name: str, symbol_names: list[str]
) -> list[tuple[Symbol, bytes]]:
    modules = _modules(listing)
    if module_name not in modules:
        raise ListingError(f"module {module_name!r} is not present in the XLIB listing")

    records = modules[module_name]
    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
    definitions_by_segment: dict[str, list[str]] = {}
    external_names: dict[str, str] = {}
    for tag, text in records:
        if tag == "t_seg":
            match = re.search(
                r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                text,
            )
            if match:
                ident, address_space, name = match.groups()
                segments[ident.upper()] = Segment(ident.upper(), address_space, name)
        elif tag == "t_symbol_def2":
            match = re.search(
                r"([A-Z_]+_REL)\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                kind, segment, offset, name = match.groups()
                segment = segment.upper()
                definitions_by_segment.setdefault(segment, []).append(name)
                if kind == "PUBLIC_REL":
                    symbols[name] = Symbol(name, segment, int(offset, 16))
                continue
            match = re.search(
                r"EXTERNAL\s+<([0-9A-Fa-f]+)>\s+[0-9A-Fa-f]+\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                external_names[match.group(1).upper()] = match.group(2)

    if not symbol_names:
        raise ListingError("select at least one public initialized XDATA symbol")
    requested: list[Symbol] = []
    for name in symbol_names:
        symbol = symbols.get(name)
        if symbol is None:
            raise ListingError(f"public initialized XDATA symbol {name!r} was not found")
        segment = segments.get(symbol.segment)
        if segment is None or (segment.address_space, segment.name) != (
            "XDATA",
            "XDATA_I",
        ):
            raise ListingError(f"{name} is not in an IAR XDATA/XDATA_I segment")
        if symbol.offset != 0:
            raise ListingError(f"nonzero symbol offset is unsupported for {name}")
        if definitions_by_segment.get(symbol.segment) != [name]:
            raise ListingError(
                f"segment {symbol.segment} has additional relative symbols; "
                "their individual sizes are not known in this prototype"
            )
        requested.append(symbol)

    records_by_segment: dict[str, list[tuple[str, str]]] = {}
    current_segment: str | None = None
    for tag, text in records:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+", text)
            current_segment = match.group(1).upper() if match else None
            if current_segment is not None:
                records_by_segment.setdefault(current_segment, [])
                records_by_segment[current_segment].append((tag, text))
            continue
        if current_segment is not None:
            records_by_segment.setdefault(current_segment, []).append((tag, text))

    initializer_for: dict[str, str] = {}
    for symbol in requested:
        source_records = records_by_segment.get(symbol.segment, [])
        relative_targets: list[str] = []
        initializer_markers: list[str] = []
        for tag, text in source_records:
            if tag == "t_org_rel1":
                continue
            if tag == "t_push_rel":
                match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
                if not match:
                    raise ListingError(
                        f"malformed XDATA_I initializer reference for {symbol.name}"
                    )
                target, addend = match.groups()
                if int(addend, 16) != 0:
                    raise ListingError(
                        f"nonzero initializer addend is unsupported for {symbol.name}"
                    )
                relative_targets.append(target.upper())
            elif tag == "t_push_ext":
                match = re.fullmatch(r"<([0-9A-Fa-f]+)>\s+[0-9A-Fa-f]+", text)
                if not match:
                    raise ListingError(
                        f"malformed XDATA_I initialization marker for {symbol.name}"
                    )
                external = external_names.get(match.group(1).upper())
                if external is None:
                    raise ListingError(
                        f"unknown XDATA_I initialization marker for {symbol.name}"
                    )
                initializer_markers.append(external)
            elif tag not in (
                "t_asm_mode_change",
                "t_delete_tos",
                "t_stack_error",
                "t_end",
            ):
                raise ListingError(
                    f"unsupported XLIB record {tag} in XDATA_I segment "
                    f"{symbol.segment}"
                )

        if len(relative_targets) != 1 or initializer_markers != ["__INIT_XDATA_I"]:
            raise ListingError(
                f"unsupported IAR initializer metadata for {symbol.name}"
            )
        initializer = relative_targets[0]
        initializer_segment = segments.get(initializer)
        if initializer_segment is None or (
            initializer_segment.address_space,
            initializer_segment.name,
        ) != ("CODE", "XDATA_ID"):
            raise ListingError(
                f"initializer for {symbol.name} does not reference an XDATA_ID segment"
            )
        initializer_for[symbol.name] = initializer

    if len(set(initializer_for.values())) != len(initializer_for):
        raise ListingError("multiple XDATA_I symbols share one initializer segment")

    result: list[tuple[Symbol, bytes]] = []
    for symbol in requested:
        initializer = initializer_for[symbol.name]
        cursor = 0
        emitted: dict[int, int] = {}
        for tag, text in records_by_segment.get(initializer, []):
            if tag == "t_org_rel1":
                match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
                if not match or match.group(1).upper() != initializer:
                    raise ListingError(
                        f"malformed initializer segment {initializer} for {symbol.name}"
                    )
                cursor = int(match.group(2), 16)
            elif tag in ("t_abs_8", "t_abs_16", "t_abs_24", "t_abs_32"):
                digits = {
                    "t_abs_8": 2,
                    "t_abs_16": 4,
                    "t_abs_24": 6,
                    "t_abs_32": 8,
                }[tag]
                if not re.fullmatch(r"[0-9A-Fa-f]{%d}" % digits, text):
                    raise ListingError(
                        f"malformed {tag} record in initializer segment {initializer}"
                    )
                data = bytes.fromhex(text)
                for offset, value in enumerate(data, cursor):
                    if offset in emitted:
                        raise ListingError(
                            f"overlapping initializer bytes in segment {initializer}"
                        )
                    emitted[offset] = value
                cursor += len(data)
            elif tag not in ("t_asm_mode_change", "t_stack_error", "t_end"):
                raise ListingError(
                    f"unsupported XLIB record {tag} in XDATA_ID segment "
                    f"{initializer}; relocatable initializers are not supported"
                )
        if not emitted:
            raise ListingError(f"initializer for {symbol.name} contains no bytes")
        size = max(emitted) + 1
        if set(emitted) != set(range(size)):
            raise ListingError(
                f"initializer for {symbol.name} contains uninitialized gaps"
            )
        result.append((symbol, bytes(emitted[offset] for offset in range(size))))
    return result


def convert_initialized_listing_to_archive(
    listing_path: Path,
    module_name: str,
    symbol_names: list[str],
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
) -> dict[str, object]:
    """Convert literal XDATA_I initializers into native ELF .data sections."""
    selected = _selected_initialized_xdata(
        listing_path.read_text(encoding="latin1"), module_name, symbol_names
    )
    llvm_mc = llvm_mc or _default_tool("llvm-mc")
    llvm_ar = llvm_ar or _default_tool("llvm-ar")

    assembly: list[str] = ["; Generated from an IAR XLIB listing by iar2llvm."]
    for symbol, contents in selected:
        alias = _asm_name(symbol.name)
        assembly.extend(
            [
                f'.section .data.{alias},"aw",@progbits',
                ".balign 1",
                f".globl {alias}",
                f".type {alias},@object",
                f"{alias}:",
            ]
        )
        assembly.extend(
            ".byte "
            + ", ".join(
                f"0x{byte:02x}" for byte in contents[offset : offset + 16]
            )
            for offset in range(0, len(contents), 16)
        )
        assembly.append(f".size {alias}, {len(contents)}")

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="iar2llvm-", dir=output.parent) as temp:
        temp_dir = Path(temp)
        module_token = module_name.encode("utf-8").hex()
        asm_path = temp_dir / f"{module_token}.s"
        object_path = temp_dir / f"{module_token}.o"
        archive_path = temp_dir / output.name
        asm_path.write_text("\n".join(assembly) + "\n", encoding="utf-8")

        assemble = subprocess.run(
            [
                str(llvm_mc),
                "-triple",
                "mcs51",
                "-filetype=obj",
                str(asm_path),
                "-o",
                str(object_path),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if assemble.returncode:
            raise ListingError(
                f"llvm-mc failed ({assemble.returncode}):\n{assemble.stderr.strip()}"
            )
        archive = subprocess.run(
            [str(llvm_ar), "rcs", str(archive_path), str(object_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if archive.returncode:
            raise ListingError(
                f"llvm-ar failed ({archive.returncode}):\n{archive.stderr.strip()}"
            )
        archive_path.replace(output)

    return {
        "module": module_name,
        "output": str(output),
        "symbols": [
            {"name": symbol.name, "size": len(contents)}
            for symbol, contents in selected
        ],
        "format": "ELF32 MCS-51 archive",
        "scope": "literal XDATA_I initializers through XDATA_ID; relocations rejected",
    }


@dataclass(frozen=True, slots=True)
class _CodeItem:
    kind: str
    value: int | str
    operand: str | None = None

    @property
    def size(self) -> int:
        return {
            "byte": 1,
            "byte_reloc": 1,
            "byte_reloc_lo": 1,
            "byte_reloc_hi": 1,
            "byte_reloc_bit": 1,
            "word": 2,
            "word_reloc": 2,
            "branch": 2,
            "call": 3,
            "relcall": 3,
            "dptr": 3,
            "bankptr": 3,
        }[self.kind]


@dataclass(frozen=True, slots=True)
class _IARValue:
    kind: str
    value: int | str
    addend: int = 0


def _asm_name(name: str) -> str:
    if re.fullmatch(r"[A-Za-z_.$][A-Za-z0-9_.$]*", name):
        return name
    return "__iar_sym_" + name.encode("utf-8").hex()


def _code_group(segment: Segment) -> str | None:
    if segment.address_space == "HUGECODE" and segment.name == "BANKED_CODE":
        return "banked"
    if segment.address_space == "CODE" and segment.name == "BANK_RELAYS":
        return "relays"
    if segment.address_space == "CODE" and segment.name == "CODE_C":
        return "common"
    return None


def _constant_segment(segment: Segment) -> bool:
    return segment.address_space == "CONST" and segment.name == "XDATA_ROM_C"


def _parse_literal_segment(
    records: list[tuple[str, str]], segment_id: str
) -> bytes:
    emitted: dict[int, int] = {}
    cursor = 0
    for tag, text in records:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match or match.group(1).upper() != segment_id:
                raise ListingError(f"malformed absolute segment {segment_id}")
            cursor = int(match.group(2), 16)
        elif tag in ("t_abs_8", "t_abs_16", "t_abs_24", "t_abs_32"):
            digits = {
                "t_abs_8": 2,
                "t_abs_16": 4,
                "t_abs_24": 6,
                "t_abs_32": 8,
            }[tag]
            if not re.fullmatch(r"[0-9A-Fa-f]{%d}" % digits, text):
                raise ListingError(f"malformed {tag} record in segment {segment_id}")
            data = bytes.fromhex(text)
            for offset, value in enumerate(data, cursor):
                if offset in emitted:
                    raise ListingError(f"overlapping bytes in segment {segment_id}")
                emitted[offset] = value
            cursor += len(data)
        elif tag not in ("t_asm_mode_change", "t_stack_error", "t_end"):
            raise ListingError(
                f"unsupported record {tag} in literal constant segment {segment_id}"
            )
    if not emitted:
        raise ListingError(f"constant segment {segment_id} has no emitted bytes")
    size = max(emitted) + 1
    if set(emitted) != set(range(size)):
        raise ListingError(f"constant segment {segment_id} contains uninitialized gaps")
    return bytes(emitted[offset] for offset in range(size))


def _parse_private_xdata_segment(
    records: list[tuple[str, str]],
    segment_id: str,
    segment: Segment,
    segments: dict[str, Segment],
    records_by_segment: dict[str, list[tuple[str, str]]],
    external_names: dict[str, str],
) -> tuple[int, bytes | None]:
    cursors: list[int] = []
    relative_targets: list[str] = []
    initializer_markers: list[str] = []
    push_count = 0
    delete_count = 0
    for tag, text in records:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match or match.group(1).upper() != segment_id:
                raise ListingError(f"malformed XDATA segment {segment_id}")
            cursors.append(int(match.group(2), 16))
        elif tag == "t_push_rel":
            match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match or int(match.group(2), 16) != 0:
                raise ListingError(f"unsupported XDATA initializer reference in {segment_id}")
            relative_targets.append(match.group(1).upper())
            push_count += 1
        elif tag == "t_push_ext":
            match = re.fullmatch(r"<([0-9A-Fa-f]+)>\s+[0-9A-Fa-f]+", text)
            if not match:
                raise ListingError(f"malformed XDATA initialization marker in {segment_id}")
            external = external_names.get(match.group(1).upper())
            if external is None:
                raise ListingError(f"unknown XDATA initialization marker in {segment_id}")
            initializer_markers.append(external)
            push_count += 1
        elif tag == "t_delete_tos":
            delete_count += 1
        elif tag not in ("t_asm_mode_change", "t_stack_error", "t_end"):
            raise ListingError(f"unsupported record {tag} in private XDATA segment {segment_id}")

    if not cursors or min(cursors) != 0:
        raise ListingError(f"cannot determine the size of private XDATA segment {segment_id}")
    size = max(cursors)
    if size == 0 or push_count != delete_count:
        raise ListingError(f"invalid initialization metadata in XDATA segment {segment_id}")

    if segment.name == "XDATA_Z":
        if segment.address_space != "XDATA" or relative_targets or initializer_markers != [
            "__INIT_XDATA_Z"
        ]:
            raise ListingError(f"unsupported zero-initialized XDATA segment {segment_id}")
        return size, None

    if segment.name != "XDATA_I" or segment.address_space != "XDATA":
        raise ListingError(f"unsupported private data segment {segment_id}")
    if len(relative_targets) != 1 or initializer_markers != ["__INIT_XDATA_I"]:
        raise ListingError(f"unsupported initialized XDATA segment {segment_id}")
    initializer = relative_targets[0]
    initializer_segment = segments.get(initializer)
    if initializer_segment is None or (
        initializer_segment.address_space,
        initializer_segment.name,
    ) != ("CODE", "XDATA_ID"):
        raise ListingError(f"initializer for XDATA segment {segment_id} is not XDATA_ID")
    contents = _parse_literal_segment(records_by_segment.get(initializer, []), initializer)
    if len(contents) != size:
        raise ListingError(
            f"initializer size for XDATA segment {segment_id} does not match its load image"
        )
    return size, contents


def _parse_code_item_records(
    records: list[tuple[str, str]],
    segment_id: str,
    segment_labels: dict[str, str],
    external_names: dict[str, str],
    external_segments: set[str],
    code_segments: set[str],
    banked_segments: set[str],
    source_aliases: dict[str, tuple[str, bool]],
) -> list[_CodeItem]:
    items: list[_CodeItem] = []
    stack: list[_IARValue] = []
    pending_high: _IARValue | None = None
    skipping_complex_expression = False
    complex_expression_alias: tuple[str, bool] | None = None
    skipping_bit_expression = False
    pending_bit_address: _IARValue | None = None

    def pop_value(tag: str) -> _IARValue:
        if not stack:
            raise ListingError(f"{tag} underflows the expression stack in segment {segment_id}")
        return stack.pop()

    def source_byte_relocation(
        text: str, fallback: tuple[str, bool] | None
    ) -> _IARValue | None:
        if "Source:" not in text:
            return None
        source = text.split("Source:", 1)[1].strip().rstrip("'")
        operand = re.search(r",\s*#(.*)$", source)
        if not operand:
            return None
        expression = operand.group(1).strip()
        shift = re.search(r">>\s*8", expression)
        relocation_kind = "high" if shift else "low"
        if shift:
            expression = expression[: shift.start()]
        elif not re.search(r"&\s*(?:0x)?ff\b", expression, re.IGNORECASE):
            return None
        expression = re.sub(r"&\s*(?:0x)?ff\b", "", expression, flags=re.IGNORECASE)
        expression = expression.lstrip("(").rstrip(") ").strip()
        symbol_match = re.fullmatch(
            r"(`[^`]+`|[A-Za-z_?$][A-Za-z0-9_?$.:]*)"
            r"\s*(?:([+-])\s*(\d+))?",
            expression,
        )
        if not symbol_match:
            return None
        source_name = symbol_match.group(1).strip("`")
        sign, digits = symbol_match.group(2), symbol_match.group(3)
        addend = int(digits, 10) if digits else 0
        if sign == "-":
            addend = -addend
        target = source_aliases.get(source_name)
        if target is None:
            target = fallback
        if target is None:
            return None
        alias, is_external = target
        kind = ("external_" if is_external else "relative_") + relocation_kind + "_single"
        return _IARValue(kind, alias, addend)

    for tag, text in records:
        if skipping_bit_expression:
            if tag == "t_pop_8":
                if pending_bit_address is None:
                    raise ListingError(
                        f"missing IAR bit-address expression in segment {segment_id}"
                    )
                alias = _asm_name(str(pending_bit_address.value))
                operand = alias
                if pending_bit_address.addend:
                    operand += (
                        f"+{pending_bit_address.addend}"
                        if pending_bit_address.addend > 0
                        else str(pending_bit_address.addend)
                    )
                items.append(_CodeItem("byte_reloc_bit", operand, alias))
                pending_bit_address = None
                skipping_bit_expression = False
            elif tag.startswith("t_abs_") or tag == "t_org_rel1":
                raise ListingError(
                    f"unexpected code record inside IAR bit expression in segment {segment_id}"
                )
            continue
        if skipping_complex_expression:
            if tag == "t_stack_error" and "Source:" in text:
                value = source_byte_relocation(text, complex_expression_alias)
                if value is None:
                    raise ListingError(
                        f"cannot translate complex IAR byte expression in segment "
                        f"{segment_id}: {text}"
                    )
                stack.append(value)
                skipping_complex_expression = False
                complex_expression_alias = None
            elif tag.startswith("t_abs_") or tag == "t_org_rel1":
                raise ListingError(
                    f"unexpected code record inside complex expression in segment {segment_id}"
                )
            continue
        if tag in ("t_abs_8", "t_abs_16", "t_abs_24", "t_abs_32"):
            digits = {
                "t_abs_8": 2,
                "t_abs_16": 4,
                "t_abs_24": 6,
                "t_abs_32": 8,
            }[tag]
            if not re.fullmatch(r"[0-9A-Fa-f]{%d}" % digits, text):
                raise ListingError(f"malformed {tag} record in segment {segment_id}")
            items.extend(_CodeItem("byte", byte) for byte in bytes.fromhex(text))
        elif tag == "t_push_rel":
            match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match:
                raise ListingError(f"malformed t_push_rel record in segment {segment_id}")
            target, addend = match.groups()
            stack.append(_IARValue("rel", target.upper(), int(addend, 16)))
        elif tag == "t_push_ext":
            match = re.fullmatch(r"<([0-9A-Fa-f]+)>\s+([0-9A-Fa-f]+)", text)
            if not match:
                raise ListingError(f"malformed t_push_ext record in segment {segment_id}")
            external_id, addend = match.groups()
            if external_id.upper() not in external_names:
                raise ListingError(f"unknown external symbol {external_id} in segment {segment_id}")
            stack.append(
                _IARValue("external", external_names[external_id.upper()], int(addend, 16))
            )
        elif tag == "t_push_pcr":
            if not re.fullmatch(r"[0-9A-Fa-f]+", text):
                raise ListingError(f"malformed t_push_pcr record in segment {segment_id}")
            stack.append(_IARValue("pcr", 0, int(text, 16)))
        elif tag == "t_push_abs":
            if not re.fullmatch(r"[0-9A-Fa-f]+", text):
                raise ListingError(f"malformed t_push_abs record in segment {segment_id}")
            stack.append(_IARValue("absolute", int(text, 16)))
        elif tag == "t_load_ind":
            # IAR emits a longer linker-expression program for byte extraction
            # from an address. Its diagnostic annotation retains the original
            # assembler operand, which is sufficient to preserve the byte
            # relocation without evaluating that internal expression VM.
            for candidate in reversed(stack):
                if candidate.kind == "external":
                    complex_expression_alias = (_asm_name(str(candidate.value)), True)
                    break
                if candidate.kind == "rel":
                    target = str(candidate.value).upper()
                    alias = segment_labels.get(target)
                    if alias is not None:
                        complex_expression_alias = (
                            alias,
                            target in external_segments,
                        )
                        break
            stack.clear()
            skipping_complex_expression = True
        elif tag == "t_plus":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if lhs.kind == "absolute" and rhs.kind == "absolute":
                stack.append(_IARValue("absolute", int(lhs.value) + int(rhs.value)))
            elif lhs.kind in ("rel", "external") and rhs.kind == "absolute":
                stack.append(
                    _IARValue(lhs.kind, lhs.value, lhs.addend + int(rhs.value))
                )
            elif rhs.kind in ("rel", "external") and lhs.kind == "absolute":
                stack.append(
                    _IARValue(rhs.kind, rhs.value, rhs.addend + int(lhs.value))
                )
            elif lhs.kind == "bit_address" and rhs.kind == "absolute":
                stack.append(
                    _IARValue(lhs.kind, lhs.value, lhs.addend + int(rhs.value))
                )
            else:
                raise ListingError(f"unsupported t_plus expression in segment {segment_id}")
        elif tag == "t_minus":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if lhs.kind == "rel" and rhs.kind == "pcr":
                if rhs.addend:
                    raise ListingError(
                        f"nonzero PCR addend is unsupported in segment {segment_id}"
                    )
                stack.append(_IARValue("pcrel", lhs.value, lhs.addend))
            elif lhs.kind == "pcrel" and rhs.kind == "absolute":
                if int(rhs.value) != 1:
                    raise ListingError(
                        f"unsupported PC-relative adjustment in segment {segment_id}"
                    )
                stack.append(lhs)
            elif lhs.kind in ("rel", "external") and rhs.kind == "absolute":
                stack.append(
                    _IARValue(lhs.kind, lhs.value, lhs.addend - int(rhs.value))
                )
            else:
                raise ListingError(f"unsupported t_minus expression in segment {segment_id}")
        elif tag == "t_and":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if (
                lhs.kind in ("rel", "relative_high", "external", "external_high")
                and rhs.kind == "absolute"
                and int(rhs.value) in (0xFF, 0xFFFF)
            ):
                stack.append(lhs)
            else:
                raise ListingError(f"unsupported t_and expression in segment {segment_id}")
        elif tag == "t_copy":
            if not stack:
                raise ListingError(f"t_copy underflows the expression stack in segment {segment_id}")
            if stack[-1].kind == "bit_address":
                pending_bit_address = stack[-1]
                stack.clear()
                skipping_bit_expression = True
                continue
            stack.append(stack[-1])
        elif tag == "t_mul":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if (
                lhs.kind == "external"
                and str(lhs.value) == "?VB"
                and rhs.kind == "absolute"
                and int(rhs.value) == 8
            ):
                stack.append(_IARValue("bit_address", lhs.value, lhs.addend))
            elif lhs.kind == "absolute" and rhs.kind == "absolute":
                stack.append(_IARValue("absolute", int(lhs.value) * int(rhs.value)))
            else:
                raise ListingError(f"unsupported t_mul expression in segment {segment_id}")
        elif tag == "t_lsr":
            if not re.fullmatch(r"[0-9A-Fa-f]+", text):
                raise ListingError(f"malformed t_lsr record in segment {segment_id}")
            shift = int(text, 16)
            value = pop_value(tag)
            if value.kind == "external" and shift == 8:
                stack.append(_IARValue("external_high", value.value, value.addend))
            elif value.kind == "rel" and shift == 8:
                stack.append(_IARValue("relative_high", value.value, value.addend))
            elif value.kind == "absolute":
                stack.append(_IARValue("absolute", int(value.value) >> shift))
            else:
                raise ListingError(f"unsupported t_lsr expression in segment {segment_id}")
        elif tag == "t_check":
            # Range masks are consumed by XLINK and do not change the value.
            continue
        elif tag == "t_pop_8":
            value = pop_value(tag)
            if value.kind in (
                "external_high_single",
                "relative_high_single",
                "external_low_single",
                "relative_low_single",
            ):
                high = value.kind.endswith("high_single")
                external = value.kind.startswith("external_")
                operand = str(value.value)
                if value.addend:
                    operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
                items.append(
                    _CodeItem(
                        "byte_reloc_hi" if high else "byte_reloc_lo",
                        operand,
                        str(value.value) if external else None,
                    )
                )
            elif value.kind == "pcrel":
                if pending_high is not None or not items or items[-1] != _CodeItem("byte", 0x80):
                    raise ListingError(
                        f"only SJMP PC-relative bytes are supported in segment {segment_id}"
                    )
                target = str(value.value).upper()
                if target not in code_segments:
                    raise ListingError(f"relative target segment {target} is unavailable")
                items[-1] = _CodeItem("branch", segment_labels[target], str(value.addend))
            elif value.kind in ("external_high", "relative_high"):
                if pending_high is not None:
                    raise ListingError(f"nested 16-bit address in segment {segment_id}")
                base_kind = "external" if value.kind == "external_high" else "rel"
                has_low_byte = (
                    bool(stack)
                    and stack[-1].kind == base_kind
                    and stack[-1].value == value.value
                    and stack[-1].addend == value.addend
                )
                if has_low_byte:
                    pending_high = value
                else:
                    raise ListingError(
                        f"standalone high-byte relocation is unsupported in segment "
                        f"{segment_id}"
                    )
            elif value.kind in ("external", "rel"):
                if pending_high is None:
                    if value.kind == "external":
                        alias = _asm_name(str(value.value))
                        external = True
                    else:
                        target = str(value.value).upper()
                        if target not in segment_labels:
                            raise ListingError(
                                f"relative byte target segment {target} has no ELF symbol"
                            )
                        alias = segment_labels[target]
                        external = target in external_segments
                    operand = alias
                    if value.addend:
                        operand += f"+{value.addend}"
                    items.append(
                        _CodeItem("byte_reloc", operand, alias if external else None)
                    )
                    continue
                expected_high_kind = (
                    "external_high" if value.kind == "external" else "relative_high"
                )
                if (
                    pending_high is None
                    or pending_high.kind != expected_high_kind
                    or pending_high.value != value.value
                    or pending_high.addend != value.addend
                    or not items
                    or items[-1].kind != "byte"
                ):
                    raise ListingError(
                        f"unsupported 16-bit address expression in segment {segment_id}"
                    )
                opcode = int(items[-1].value)
                if opcode == 0x90:
                    if value.kind == "external":
                        alias = _asm_name(str(value.value))
                        operand = alias
                    else:
                        target = str(value.value).upper()
                        if target not in segment_labels:
                            raise ListingError(
                                f"relative data target segment {target} has no ELF symbol"
                            )
                        alias = segment_labels[target]
                        operand = alias
                    if value.addend:
                        operand += f"+{value.addend}"
                    items.pop()
                    external = value.kind == "external" or str(value.value).upper() in external_segments
                    items.append(_CodeItem("dptr", operand, alias if external else None))
                elif opcode in (0x02, 0x12):
                    if value.kind == "external":
                        if value.addend:
                            raise ListingError(
                                f"external call addends are unsupported in segment {segment_id}"
                            )
                        operand = _asm_name(str(value.value))
                    else:
                        target = str(value.value).upper()
                        if target not in code_segments:
                            raise ListingError(
                                f"relative call target segment {target} is unavailable"
                            )
                        operand = segment_labels[target]
                        if value.addend:
                            operand += f"+{value.addend}"
                    mnemonic = "ljmp" if opcode == 0x02 else "lcall"
                    items.pop()
                    items.append(
                        _CodeItem(
                            "call" if value.kind == "external" else "relcall",
                            mnemonic,
                            operand,
                        )
                    )
                else:
                    raise ListingError(
                        f"16-bit address is not DPTR/LJMP/LCALL in segment {segment_id}"
                    )
                pending_high = None
            else:
                raise ListingError(f"unsupported t_pop_8 expression in segment {segment_id}")
        elif tag == "t_pop_24":
            value = pop_value(tag)
            if value.kind != "rel":
                raise ListingError(f"unsupported t_pop_24 expression in segment {segment_id}")
            target = str(value.value).upper()
            if target not in banked_segments:
                raise ListingError(f"24-bit target segment {target} is unavailable")
            operand = segment_labels[target]
            if value.addend:
                operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
            items.append(_CodeItem("bankptr", operand))
        elif tag == "t_pop_16":
            value = pop_value(tag)
            if value.kind == "absolute":
                number = int(value.value)
                if not 0 <= number <= 0xFFFF:
                    raise ListingError(f"16-bit value is out of range in segment {segment_id}")
                items.append(_CodeItem("word", number))
            elif value.kind == "external":
                alias = _asm_name(str(value.value))
                operand = alias
                if value.addend:
                    operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
                items.append(_CodeItem("word_reloc", operand, alias))
            elif value.kind == "rel":
                target = str(value.value).upper()
                alias = segment_labels.get(target)
                if alias is None:
                    raise ListingError(
                        f"16-bit relocation target segment {target} has no ELF symbol"
                    )
                operand = alias
                if value.addend:
                    operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
                items.append(
                    _CodeItem(
                        "word_reloc",
                        operand,
                        alias if target in external_segments else None,
                    )
                )
            else:
                raise ListingError(f"unsupported t_pop_16 expression in segment {segment_id}")
        elif tag == "t_delete_tos":
            pop_value(tag)
        elif tag in (
            "t_org_rel1",
            "t_asm_mode_change",
            "t_stack_error",
            "t_call_frame_info",
            "t_src_call",
            "t_count",
            "t_sub_def_frame_size",
            "t_end",
        ):
            continue
        elif tag.startswith("t_abs_"):
            raise ListingError(f"unsupported code emission record {tag} in segment {segment_id}")
        elif tag.startswith("t_push_") or tag.startswith("t_pop_"):
            raise ListingError(f"unsupported expression record {tag} in segment {segment_id}")
        else:
            raise ListingError(f"unsupported XLIB record {tag} in code segment {segment_id}")

    if skipping_bit_expression:
        raise ListingError(f"unterminated IAR bit expression in segment {segment_id}")
    if skipping_complex_expression:
        raise ListingError(f"unterminated complex expression in segment {segment_id}")
    if stack or pending_high is not None:
        raise ListingError(f"unconsumed expression in code segment {segment_id}")
    return items


def convert_code_listing_to_archive(
    listing_path: Path,
    module_name: str,
    symbol_names: list[str],
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
    all_public_banked: bool = False,
) -> dict[str, object]:
    """Convert selected IAR banked code, relays, and supported references.

    The converter preserves machine instructions, lowers known XLIB
    expressions to ELF relocations, and does not adapt the IAR calling
    convention to Clang's ABI.
    """
    modules = _modules(listing_path.read_text(encoding="latin1"))
    if module_name not in modules:
        raise ListingError(f"module {module_name!r} is not present in the XLIB listing")

    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
    local_symbols: dict[str, Symbol] = {}
    external_names: dict[str, str] = {}
    for tag, text in modules[module_name]:
        if tag == "t_seg":
            match = re.search(
                r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                text,
            )
            if match:
                ident, address_space, name = match.groups()
                segments[ident.upper()] = Segment(ident.upper(), address_space, name)
        elif tag == "t_symbol_def2":
            match = re.search(
                r"PUBLIC_REL\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                ident, offset, name = match.groups()
                symbols[name] = Symbol(name, ident.upper(), int(offset, 16))
                continue
            match = re.search(
                r"LOCAL_REL\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                ident, offset, name = match.groups()
                local_symbol = Symbol(name, ident.upper(), int(offset, 16))
                local_symbols[name] = local_symbol
                normalized = name.removeprefix("<internal>::")
                local_symbols.setdefault(normalized, local_symbol)
                continue
            match = re.search(
                r"EXTERNAL\s+<([0-9A-Fa-f]+)>\s+[0-9A-Fa-f]+\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                ident, name = match.groups()
                external_names[ident.upper()] = name

    code_records: dict[str, list[tuple[str, str]]] = {}
    segment_order: list[str] = []
    current_segment: str | None = None
    for tag, text in modules[module_name]:
        if tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+", text)
            current_segment = match.group(1).upper() if match else None
            segment = segments.get(current_segment or "")
            if segment and (
                _code_group(segment)
                or _constant_segment(segment)
                or segment.name in ("XDATA_Z", "XDATA_I", "XDATA_ID")
            ):
                if current_segment not in code_records:
                    code_records[current_segment] = []
                    if _code_group(segment) or _constant_segment(segment):
                        segment_order.append(current_segment)
                code_records[current_segment].append((tag, text))
            continue
        if current_segment in code_records:
            code_records.setdefault(current_segment, []).append((tag, text))

    requested: dict[str, Symbol] = {}
    if all_public_banked:
        if symbol_names:
            raise ListingError("--all-public-banked cannot be combined with --symbol")
        for name, symbol in symbols.items():
            segment = segments.get(symbol.segment)
            if segment is None or _code_group(segment) != "banked":
                continue
            if symbol.offset != 0:
                raise ListingError(f"nonzero symbol offset is unsupported for {name}")
            requested[name] = symbol
        if not requested:
            raise ListingError("module has no public BANKED_CODE symbols")
    else:
        if not symbol_names:
            raise ListingError(
                "select a public BANKED_CODE symbol or use --all-public-banked"
            )
        for name in symbol_names:
            symbol = symbols.get(name)
            if symbol is None:
                raise ListingError(f"public code symbol {name!r} was not found")
            segment = segments.get(symbol.segment)
            if segment is None or _code_group(segment) != "banked":
                raise ListingError(f"{name} is not in an IAR BANKED_CODE segment")
            if symbol.offset != 0:
                raise ListingError(f"nonzero symbol offset is unsupported for {name}")
            requested[name] = symbol

    selected_segments = {symbol.segment for symbol in requested.values()}
    relay_exports: dict[str, Symbol] = {}
    for name in requested:
        relay = symbols.get(name + "::?relay")
        if relay:
            relay_segment = segments.get(relay.segment)
            if relay_segment is None or _code_group(relay_segment) != "relays":
                raise ListingError(f"relay for {name} is not in an IAR BANK_RELAYS segment")
            if relay.offset != 0:
                raise ListingError(f"nonzero relay symbol offset is unsupported for {name}")
            selected_segments.add(relay.segment)
            relay_exports[name] = relay

    # Relocatable code references keep code segments in this object. Data
    # references remain ELF symbol relocations and are resolved by other
    # converted archive members.
    selected_constant_segments: set[str] = set()
    selected_xdata_segments: set[str] = set()
    pending = list(selected_segments)
    while pending:
        source = pending.pop()
        for tag, text in code_records.get(source, []):
            if tag != "t_push_rel":
                continue
            match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+", text)
            if not match:
                raise ListingError(f"malformed t_push_rel record in segment {source}")
            target = match.group(1).upper()
            target_segment = segments.get(target)
            if target_segment is None:
                # Some XLIB expression records are immediately discarded by
                # t_delete_tos and refer to compiler-private segment ids.
                # The code decoder rejects a missing segment if it reaches an
                # emitted relocation.
                continue
            if _code_group(target_segment) and target not in selected_segments:
                selected_segments.add(target)
                pending.append(target)
            elif _constant_segment(target_segment):
                selected_constant_segments.add(target)
            elif target_segment.name in ("XDATA_Z", "XDATA_I"):
                selected_xdata_segments.add(target)

    module_token = module_name.encode("utf-8").hex()
    segment_labels = {
        ident: f"__iar_seg_{module_token}_{ident.lower()}" for ident in selected_segments
    }
    segment_labels.update(
        {
            ident: f"__iar_const_{module_token}_{ident.lower()}"
            for ident in selected_constant_segments
        }
    )
    public_data_symbols: dict[str, list[Symbol]] = {}
    for symbol in symbols.values():
        if symbol.segment not in selected_segments and symbol.segment not in selected_constant_segments:
            public_data_symbols.setdefault(symbol.segment, []).append(symbol)
    external_data_segments: set[str] = set()
    for ident, candidates in public_data_symbols.items():
        if len(candidates) == 1 and candidates[0].offset == 0:
            segment_labels[ident] = _asm_name(candidates[0].name)
            external_data_segments.add(ident)

    private_xdata: dict[str, tuple[int, bytes | None]] = {}
    for ident in selected_xdata_segments:
        candidates = public_data_symbols.get(ident, [])
        if ident in external_data_segments:
            continue
        if candidates:
            raise ListingError(
                f"public symbols in XDATA segment {ident} do not have a unique base"
            )
        segment = segments[ident]
        size, contents = _parse_private_xdata_segment(
            code_records.get(ident, []),
            ident,
            segment,
            segments,
            code_records,
            external_names,
        )
        private_xdata[ident] = (size, contents)
        segment_labels[ident] = f"__iar_data_{module_token}_{ident.lower()}"

    decoded: dict[str, list[_CodeItem]] = {}
    banked_segments = {
        ident for ident in selected_segments if _code_group(segments[ident]) == "banked"
    }
    source_aliases: dict[str, tuple[str, bool]] = {
        name: (_asm_name(name), True) for name in external_names.values()
    }
    for name, symbol in symbols.items():
        if name in requested or name in relay_exports:
            source_aliases[name] = (_asm_name(name), False)
            continue
        label = segment_labels.get(symbol.segment)
        if label is not None:
            alias = label
            if symbol.offset:
                alias += f"+{symbol.offset}"
            source_aliases[name] = (
                alias,
                symbol.segment in external_data_segments,
            )
        else:
            source_aliases[name] = (_asm_name(name), True)
    for name, symbol in local_symbols.items():
        normalized = name.removeprefix("<internal>::")
        label = segment_labels.get(symbol.segment)
        if label is not None:
            alias = label
            if symbol.offset:
                alias += f"+{symbol.offset}"
            source_aliases[name] = (alias, False)
            source_aliases.setdefault(normalized, (alias, False))
    for name, target in list(source_aliases.items()):
        if not name.startswith("??"):
            source_aliases.setdefault("??" + name, target)
    for ident in selected_segments:
        decoded[ident] = _parse_code_item_records(
            code_records.get(ident, []),
            ident,
            segment_labels,
            external_names,
            external_data_segments,
            selected_segments,
            banked_segments,
            source_aliases,
        )
    decoded_constants = {
        ident: _parse_literal_segment(code_records.get(ident, []), ident)
        for ident in selected_constant_segments
    }

    exports_by_segment: dict[str, list[str]] = {}
    exported_originals: dict[str, str] = {}
    for original, symbol in requested.items():
        alias = _asm_name(original)
        exports_by_segment.setdefault(symbol.segment, []).append(alias)
        exported_originals[original] = alias
    for original, symbol in relay_exports.items():
        alias = _asm_name(symbol.name)
        exports_by_segment.setdefault(symbol.segment, []).append(alias)
        exported_originals[symbol.name] = alias

    section_names = {
        "banked": f".mcs51.autobank.iar_{module_token}",
        "relays": f".text.__mcs51_iarrelay_{module_token}",
        "common": f".text.__mcs51_iarcommon_{module_token}",
    }
    lines = ["; Generated from an IAR XLIB listing by iar2llvm."]
    external_aliases = sorted(
        {
            _asm_name(str(item.operand))
            for items in decoded.values()
            for item in items
            if item.kind in (
                "call",
                "dptr",
                "byte_reloc",
                "byte_reloc_lo",
                "byte_reloc_hi",
                "byte_reloc_bit",
                "word_reloc",
            )
            and item.operand is not None
        }
    )
    external_alias_set = set(external_aliases)
    unresolved = {
        name for name in external_names.values() if _asm_name(name) in external_alias_set
    }
    unresolved.update(
        symbol.name
        for symbol in symbols.values()
        if symbol.segment not in selected_segments
        and symbol.segment not in selected_constant_segments
        and _asm_name(symbol.name) in external_alias_set
    )
    unresolved_names = sorted(unresolved)
    for alias in external_aliases:
        lines.append(f".globl {alias}")

    selected_order = [ident for ident in segment_order if ident in selected_segments]
    for group in ("banked", "relays", "common"):
        group_segments = [
            ident
            for ident in selected_order
            if _code_group(segments[ident]) == group
        ]
        if not group_segments:
            continue
        lines.extend(
            [
                f'.section {section_names[group]},"ax",@progbits',
                ".balign 1",
            ]
        )
        for ident in group_segments:
            exports = exports_by_segment.get(ident, [])
            for alias in exports:
                lines.extend([f".globl {alias}", f".type {alias},@function", f"{alias}:"])
            lines.append(f"{segment_labels[ident]}:")
            items = decoded[ident]
            if not items and exports:
                raise ListingError(
                    f"public symbol {exports[0]} has no emitted code in segment {ident}"
                )
            for item in items:
                if item.kind == "byte":
                    lines.append(f".byte 0x{int(item.value):02x}")
                elif item.kind == "byte_reloc":
                    lines.append(f".byte {item.value}")
                elif item.kind in ("byte_reloc_lo", "byte_reloc_hi"):
                    fixup = "fixup_lo8" if item.kind == "byte_reloc_lo" else "fixup_hi8"
                    lines.extend(
                        [
                            ".byte 0",
                            f".reloc .-1, {fixup}, {item.value}",
                        ]
                    )
                elif item.kind == "byte_reloc_bit":
                    lines.extend(
                        [
                            ".byte 0",
                            f".reloc .-1, fixup_bit8, {item.value}",
                        ]
                    )
                elif item.kind == "word":
                    lines.append(f".short 0x{int(item.value):04x}")
                elif item.kind == "word_reloc":
                    lines.append(f".short {item.value}")
                elif item.kind == "branch":
                    operand = str(item.value)
                    addend = int(item.operand or "0")
                    if addend:
                        operand += f"+{addend}" if addend > 0 else str(addend)
                    lines.append(f"sjmp {operand}")
                elif item.kind in ("call", "relcall"):
                    lines.append(f"{item.value} {item.operand}")
                elif item.kind == "dptr":
                    lines.append(f"mov dptr, #{item.value}")
                elif item.kind == "bankptr":
                    lines.append(f"bankaddr24 {item.value}")
            size = sum(item.size for item in items)
            for alias in exports:
                lines.append(f".size {alias}, {size}")

    if decoded_constants:
        lines.extend(
            [
                f'.section .rodata.iar_{module_token},"a",@progbits',
                ".balign 1",
            ]
        )
        for ident in segment_order:
            if ident not in decoded_constants:
                continue
            contents = decoded_constants[ident]
            lines.append(f"{segment_labels[ident]}:")
            lines.extend(
                ".byte "
                + ", ".join(
                    f"0x{byte:02x}" for byte in contents[offset : offset + 16]
                )
                for offset in range(0, len(contents), 16)
            )

    private_bss = {
        ident: size
        for ident, (size, contents) in private_xdata.items()
        if contents is None
    }
    if private_bss:
        lines.extend(
            [
                f'.section .bss.iar_{module_token},"aw",@nobits',
                ".balign 1",
            ]
        )
        for ident, size in private_bss.items():
            label = segment_labels[ident]
            lines.extend(
                [
                    f".type {label},@object",
                    f"{label}:",
                    f".zero {size}",
                    f".size {label}, {size}",
                ]
            )

    private_initialized = {
        ident: contents
        for ident, (_size, contents) in private_xdata.items()
        if contents is not None
    }
    if private_initialized:
        lines.extend(
            [
                f'.section .data.iar_{module_token},"aw",@progbits',
                ".balign 1",
            ]
        )
        for ident, contents in private_initialized.items():
            label = segment_labels[ident]
            lines.append(f".type {label},@object")
            lines.append(f"{label}:")
            lines.extend(
                ".byte "
                + ", ".join(
                    f"0x{byte:02x}" for byte in contents[offset : offset + 16]
                )
                for offset in range(0, len(contents), 16)
            )
            lines.append(f".size {label}, {len(contents)}")

    llvm_mc = llvm_mc or _default_tool("llvm-mc")
    llvm_ar = llvm_ar or _default_tool("llvm-ar")
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="iar2llvm-", dir=output.parent) as temp:
        temp_dir = Path(temp)
        asm_path = temp_dir / f"{module_token}.s"
        object_path = temp_dir / f"{module_token}.o"
        archive_path = temp_dir / output.name
        asm_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

        assemble = subprocess.run(
            [str(llvm_mc), "-triple", "mcs51", "-filetype=obj", str(asm_path), "-o", str(object_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if assemble.returncode:
            raise ListingError(
                f"llvm-mc failed ({assemble.returncode}):\n{assemble.stderr.strip()}"
            )
        archive = subprocess.run(
            [str(llvm_ar), "rcs", str(archive_path), str(object_path)],
            capture_output=True,
            text=True,
            check=False,
        )
        if archive.returncode:
            raise ListingError(
                f"llvm-ar failed ({archive.returncode}):\n{archive.stderr.strip()}"
            )
        archive_path.replace(output)

    return {
        "module": module_name,
        "output": str(output),
        "symbols": exported_originals,
        "segments": selected_order,
        "unresolved": unresolved_names,
        "unresolved_elf_symbols": external_aliases,
        "format": "ELF32 MCS-51 archive",
        "scope": (
            "all public IAR BANKED_CODE/BANK_RELAYS code, literal referenced constants, "
            "private XDATA objects, and supported address relocations"
            if all_public_banked
            else "selected IAR BANKED_CODE/BANK_RELAYS code and supported relocation records"
        ),
        "abi": "IAR calling convention preserved; no Clang ABI adaptation",
    }
