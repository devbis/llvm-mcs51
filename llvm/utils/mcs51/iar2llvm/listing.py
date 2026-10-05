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


@dataclass(frozen=True, slots=True)
class _InitializerRelocation:
    symbol: str
    addend: int = 0


_InitializerAtom = int | _InitializerRelocation


def _initializer_size(contents: list[_InitializerAtom]) -> int:
    return sum(2 if isinstance(atom, _InitializerRelocation) else 1 for atom in contents)


def _emit_initializer(lines: list[str], contents: list[_InitializerAtom]) -> None:
    literal_bytes: list[int] = []

    def flush_literal_bytes() -> None:
        for offset in range(0, len(literal_bytes), 16):
            lines.append(
                ".byte "
                + ", ".join(
                    f"0x{byte:02x}" for byte in literal_bytes[offset : offset + 16]
                )
            )
        literal_bytes.clear()

    for atom in contents:
        if isinstance(atom, int):
            literal_bytes.append(atom)
            continue
        flush_literal_bytes()
        operand = _asm_symbol(atom.symbol)
        if atom.addend:
            operand += f"+{atom.addend}" if atom.addend > 0 else str(atom.addend)
        lines.append(f".short {operand}")
    flush_literal_bytes()


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
    current_language: str | None = None
    current_records: list[tuple[str, str]] = []
    module_languages: dict[str, str | None] = {}
    duplicate_bases: set[str] = set()

    def store_module() -> None:
        if current_name is None:
            return
        if current_name not in modules and current_name not in duplicate_bases:
            modules[current_name] = current_records
            module_languages[current_name] = current_language
            return
        if current_name not in duplicate_bases:
            previous_records = modules.pop(current_name)
            previous_language = module_languages.pop(current_name)
            suffix = previous_language or "unknown"
            modules[f"{current_name}@LAN{suffix}"] = previous_records
            duplicate_bases.add(current_name)
        suffix = current_language or "unknown"
        key = f"{current_name}@LAN{suffix}"
        duplicate_index = 2
        while key in modules:
            key = f"{current_name}@LAN{suffix}_{duplicate_index}"
            duplicate_index += 1
        modules[key] = current_records

    for record in _records(listing.splitlines()):
        tag, text = record
        if tag == "t_beg_lib":
            store_module()
            match = re.search(r"'([^']+)'", text)
            if not match:
                raise ListingError("XLIB t_beg_lib record has no module name")
            current_name = match.group(1)
            language = re.search(r"\bLAN=([0-9A-Fa-f]+)", text)
            current_language = language.group(1).upper() if language else None
            current_records = []
        elif current_name is not None:
            current_records.append(record)
    store_module()
    if not modules:
        raise ListingError("no XLIB t_beg_lib module records found")
    return modules


def _selected_bss(
    listing: str,
    module_name: str,
    symbol_names: list[str],
    module_records: list[tuple[str, str]] | None = None,
):
    if module_records is None:
        modules = _modules(listing)
        if module_name not in modules:
            raise ListingError(
                f"module {module_name!r} is not present in the XLIB listing"
            )
        module_records = modules[module_name]

    records = module_records
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
        asm_name = _asm_symbol(symbol.name)
        section_name = f".bss.iar_{symbol.name.encode('utf-8').hex()}"
        assembly.extend(
            [
                f'.section {section_name},"aw",@nobits',
                ".balign 1",
                f".globl {asm_name}",
                f".type {asm_name},@object",
                f"{asm_name}:",
                f".zero {size}",
                f".size {asm_name}, {size}",
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
        asm_alias = _asm_symbol(alias)
        section_name = f".rodata.iar_{symbol.name.encode('utf-8').hex()}"
        assembly.extend(
            [
                f'.section {section_name},"a",@progbits',
                ".balign 1",
                f".globl {asm_alias}",
                f".type {asm_alias},@object",
                f"{asm_alias}:",
            ]
        )
        assembly.extend(
            ".byte "
            + ", ".join(
                f"0x{byte:02x}" for byte in contents[offset : offset + 16]
            )
            for offset in range(0, size, 16)
        )
        assembly.append(f".size {asm_alias}, {size}")

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
    listing: str,
    module_name: str,
    symbol_names: list[str],
    module_records: list[tuple[str, str]] | None = None,
) -> list[tuple[Symbol, list[_InitializerAtom]]]:
    if module_records is None:
        modules = _modules(listing)
        if module_name not in modules:
            raise ListingError(
                f"module {module_name!r} is not present in the XLIB listing"
            )
        module_records = modules[module_name]

    records = module_records
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

    result: list[tuple[Symbol, list[_InitializerAtom]]] = []
    for symbol in requested:
        initializer = initializer_for[symbol.name]
        cursor = 0
        emitted: dict[int, _InitializerAtom] = {}
        occupied: set[int] = set()
        pending_relocation: tuple[str, int] | None = None
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
                    if offset in occupied:
                        raise ListingError(
                            f"overlapping initializer bytes in segment {initializer}"
                        )
                    emitted[offset] = value
                    occupied.add(offset)
                cursor += len(data)
            elif tag == "t_push_rel":
                match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
                if not match or pending_relocation is not None:
                    raise ListingError(
                        f"unsupported relocation expression in XDATA_ID segment "
                        f"{initializer}"
                    )
                pending_relocation = (match.group(1).upper(), int(match.group(2), 16))
            elif tag == "t_pop_16":
                if pending_relocation is None:
                    raise ListingError(
                        f"16-bit initializer relocation has no target in segment {initializer}"
                    )
                target_segment, addend = pending_relocation
                target = segments.get(target_segment)
                if target is None or (target.address_space, target.name) != (
                    "CODE",
                    "CODE_C",
                ):
                    raise ListingError(
                        f"16-bit initializer relocation in segment {initializer} "
                        f"does not target a common CODE_C segment"
                    )
                target_symbols = [
                    candidate
                    for candidate in symbols.values()
                    if candidate.segment == target_segment and candidate.offset == 0
                ]
                if len(target_symbols) != 1:
                    raise ListingError(
                        f"16-bit initializer target segment {target_segment} does not "
                        "have one public base symbol"
                    )
                if cursor in occupied or cursor + 1 in occupied:
                    raise ListingError(
                        f"overlapping initializer relocation in segment {initializer}"
                    )
                emitted[cursor] = _InitializerRelocation(
                    target_symbols[0].name, addend
                )
                occupied.update((cursor, cursor + 1))
                cursor += 2
                pending_relocation = None
            elif tag not in ("t_asm_mode_change", "t_stack_error", "t_end"):
                raise ListingError(
                    f"unsupported XLIB record {tag} in XDATA_ID segment "
                    f"{initializer}"
                )
        if pending_relocation is not None:
            raise ListingError(
                f"unterminated initializer relocation in segment {initializer}"
            )
        if not occupied:
            raise ListingError(f"initializer for {symbol.name} contains no bytes")
        size = max(occupied) + 1
        if occupied != set(range(size)):
            raise ListingError(
                f"initializer for {symbol.name} contains uninitialized gaps"
            )
        contents: list[_InitializerAtom] = []
        offset = 0
        while offset < size:
            atom = emitted[offset]
            contents.append(atom)
            offset += 2 if isinstance(atom, _InitializerRelocation) else 1
        result.append((symbol, contents))
    return result


def convert_initialized_listing_to_archive(
    listing_path: Path,
    module_name: str,
    symbol_names: list[str],
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
) -> dict[str, object]:
    """Convert supported XDATA_I initializers into native ELF .data sections."""
    selected = _selected_initialized_xdata(
        listing_path.read_text(encoding="latin1"), module_name, symbol_names
    )
    llvm_mc = llvm_mc or _default_tool("llvm-mc")
    llvm_ar = llvm_ar or _default_tool("llvm-ar")

    assembly: list[str] = ["; Generated from an IAR XLIB listing by iar2llvm."]
    for symbol, contents in selected:
        alias = _asm_name(symbol.name)
        asm_alias = _asm_symbol(alias)
        section_name = f".data.iar_{symbol.name.encode('utf-8').hex()}"
        assembly.extend(
            [
                f'.section {section_name},"aw",@progbits',
                ".balign 1",
                f".globl {asm_alias}",
                f".type {asm_alias},@object",
                f"{asm_alias}:",
            ]
        )
        _emit_initializer(assembly, contents)
        assembly.append(f".size {asm_alias}, {_initializer_size(contents)}")

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
            {"name": symbol.name, "size": _initializer_size(contents)}
            for symbol, contents in selected
        ],
        "format": "ELF32 MCS-51 archive",
        "scope": "literal XDATA_I initializers and 16-bit CODE_C pointer relocations",
    }


@dataclass(frozen=True, slots=True)
class _CodeItem:
    kind: str
    value: int | str
    operand: str | None = None
    external_symbol: str | None = None

    @property
    def size(self) -> int:
        return {
            "byte": 1,
            "byte_reloc": 1,
            "byte_reloc_lo": 1,
            "byte_reloc_hi": 1,
            "byte_reloc_bit": 1,
            "pcrel8": 1,
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
    """Keep the ELF symbol name identical to the source IAR name."""
    return name


def _asm_symbol(name: str) -> str:
    """Format one ELF symbol name as an LLVM assembler identifier."""
    if re.fullmatch(r"[A-Za-z_.$][A-Za-z0-9_.$]*", name):
        return name
    escaped = name.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _asm_expression(expression: str) -> str:
    """Quote a symbol in a simple symbol-plus/minus-integer expression."""
    if re.fullmatch(r"(?:0[xX][0-9A-Fa-f]+|[0-9]+)", expression):
        return expression
    match = re.fullmatch(r"(.+?)([+-][0-9]+)", expression)
    if match:
        name, addend = match.groups()
        return _asm_symbol(name) + addend
    return _asm_symbol(expression)


def _expression_symbol(expression: str) -> str:
    """Return the symbol portion of a symbol-plus/minus-integer expression."""
    match = re.fullmatch(r"(.+?)([+-][0-9]+)", expression)
    return match.group(1) if match else expression


def _iar_segment_boundary_symbol(boundary: str, segment_name: str) -> str:
    safe_name = re.sub(r"[^A-Za-z0-9_$]", "_", segment_name)
    return f"__iar_{boundary.lower()}_{safe_name}"


def _virtual_register_support(
    records: list[tuple[str, str]],
    vb_byte_address: int | None = None,
) -> tuple[list[str], set[str], set[str]]:
    """Emit the IAR DATA symbols used by the selected CC2530 runtime model."""
    segments: dict[str, Segment] = {}
    symbols: list[tuple[str, str, int]] = []
    extents: dict[str, int] = {}
    for tag, text in records:
        if tag == "t_seg":
            match = re.search(
                r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                text,
            )
            if match:
                ident, address_space, name = match.groups()
                segments[ident.upper()] = Segment(
                    ident.upper(), address_space, name
                )
        elif tag == "t_symbol_def2":
            match = re.search(
                r"PUBLIC_REL\s+\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)\s+"
                r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                text,
            )
            if match:
                segment, offset, name = match.groups()
                symbols.append((segment.upper(), name, int(offset, 16)))
        elif tag == "t_org_rel1":
            match = re.search(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if match:
                segment, offset = match.groups()
                segment = segment.upper()
                extents[segment] = max(
                    extents.get(segment, 0), int(offset, 16)
                )

    lines = ["; Generated IAR virtual-register support."]
    exports: set[str] = set()
    names: set[str] = set()
    for segment_id, name, offset in symbols:
        segment = segments.get(segment_id)
        if segment is None or segment.address_space != "DATA":
            continue
        if segment.name == "VREG" and (
            match := re.fullmatch(r"\?V([0-7])", name)
        ):
            if offset != 0:
                raise ListingError(
                    f"IAR virtual register {name} has a nonzero offset"
                )
            alias = _asm_name(name)
            lines.extend(
                [
                    f".globl {_asm_symbol(alias)}",
                    f".equ {_asm_symbol(alias)}, {0x30 + int(match.group(1))}",
                ]
            )
            exports.add(alias)
            names.add(name)
            continue
        if segment.name not in ("XSP", "PSP") or name not in ("?XSP", "?PSP"):
            continue
        size = extents.get(segment_id, 0) - offset
        expected_size = 2 if name == "?XSP" else 1
        if offset != 0 or size != expected_size:
            raise ListingError(
                f"unexpected IAR {name} data layout: offset {offset}, size {size}"
            )
        alias = _asm_name(name)
        lines.extend(
            [
                '.section .mcs51.data1.bss,"aw",@nobits',
                ".balign 1",
                f".globl {_asm_symbol(alias)}",
                f".type {_asm_symbol(alias)},@object",
                f"{_asm_symbol(alias)}:",
                f".zero {size}",
                f".size {_asm_symbol(alias)}, {size}",
            ]
        )
        exports.add(alias)
        names.add(name)
    if vb_byte_address is not None:
        if not 0x20 <= vb_byte_address <= 0x2F:
            raise ListingError(
                f"IAR ?VB byte address 0x{vb_byte_address:x} is outside 0x20..0x2f"
            )
        alias = _asm_name("?VB")
        lines.extend(
            [
                f".globl {_asm_symbol(alias)}",
                f".equ {_asm_symbol(alias)}, {vb_byte_address}",
            ]
        )
        exports.add(alias)
        names.add("?VB")
    return lines, exports, names


def _code_group(segment: Segment) -> str | None:
    if segment.address_space == "HUGECODE" and segment.name == "BANKED_CODE":
        return "banked"
    if segment.address_space == "CODE" and segment.name == "BANK_RELAYS":
        return "relays"
    if segment.address_space == "CODE" and segment.name == "RCODE":
        return "common_code"
    if segment.address_space == "CODE" and segment.name == "CODE_C":
        return "common_const"
    return None


def _constant_segment(segment: Segment) -> bool:
    return segment.address_space == "CONST" and segment.name == "XDATA_ROM_C"


def _parse_literal_segment(
    records: list[tuple[str, str]],
    segment_id: str,
    relocation_aliases: dict[str, str] | None = None,
) -> bytes | list[_InitializerAtom]:
    emitted: dict[int, _InitializerAtom] = {}
    occupied: set[int] = set()
    cursor = 0
    pending_relocation: tuple[str, int] | None = None
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
                if offset in occupied:
                    raise ListingError(f"overlapping bytes in segment {segment_id}")
                emitted[offset] = value
                occupied.add(offset)
            cursor += len(data)
        elif tag == "t_push_rel":
            match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if (
                relocation_aliases is None
                or not match
                or pending_relocation is not None
            ):
                raise ListingError(
                    f"unsupported relocation expression in literal constant segment "
                    f"{segment_id}"
                )
            pending_relocation = (match.group(1).upper(), int(match.group(2), 16))
        elif tag == "t_pop_16":
            if relocation_aliases is None or pending_relocation is None:
                raise ListingError(
                    f"16-bit initializer relocation has no target in segment {segment_id}"
                )
            target, addend = pending_relocation
            alias = relocation_aliases.get(target)
            if alias is None:
                raise ListingError(
                    f"initializer relocation target {target} has no ELF symbol "
                    f"in segment {segment_id}"
                )
            if cursor in occupied or cursor + 1 in occupied:
                raise ListingError(
                    f"overlapping relocation bytes in segment {segment_id}"
                )
            emitted[cursor] = _InitializerRelocation(alias, addend)
            occupied.update((cursor, cursor + 1))
            cursor += 2
            pending_relocation = None
        elif tag not in ("t_asm_mode_change", "t_stack_error", "t_end"):
            raise ListingError(
                f"unsupported record {tag} in literal constant segment {segment_id}"
            )
    if not emitted:
        raise ListingError(f"constant segment {segment_id} has no emitted bytes")
    if pending_relocation is not None:
        raise ListingError(f"unterminated relocation in segment {segment_id}")
    size = max(occupied) + 1
    if occupied != set(range(size)):
        raise ListingError(f"constant segment {segment_id} contains uninitialized gaps")
    contents: list[_InitializerAtom] = []
    offset = 0
    while offset < size:
        atom = emitted[offset]
        contents.append(atom)
        offset += 2 if isinstance(atom, _InitializerRelocation) else 1
    if all(isinstance(atom, int) for atom in contents):
        return bytes(int(atom) for atom in contents)
    return contents


def _parse_private_xdata_segment(
    records: list[tuple[str, str]],
    segment_id: str,
    segment: Segment,
    segments: dict[str, Segment],
    symbols: dict[str, Symbol],
    segment_labels: dict[str, str],
    records_by_segment: dict[str, list[tuple[str, str]]],
    external_names: dict[str, str],
) -> tuple[int, bytes | list[_InitializerAtom] | None]:
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
    initializer_records = records_by_segment.get(initializer, [])
    relocation_aliases: dict[str, str] = {}
    for tag, text in initializer_records:
        if tag != "t_push_rel":
            continue
        match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+", text)
        if not match:
            raise ListingError(f"malformed initializer relocation in segment {initializer}")
        target = match.group(1).upper()
        target_segment = segments.get(target)
        if target_segment is None or not (
            (
                target_segment.address_space == "XDATA"
                and target_segment.name in ("XDATA_I", "XDATA_Z")
            )
            or (
                target_segment.address_space == "CODE"
                and target_segment.name == "CODE_C"
            )
        ):
            raise ListingError(
                f"initializer in segment {initializer} references unsupported "
                f"segment {target}"
            )
        target_symbols = [
            candidate
            for candidate in symbols.values()
            if candidate.segment == target and candidate.offset == 0
        ]
        if len(target_symbols) == 1:
            relocation_aliases[target] = _asm_name(target_symbols[0].name)
        elif target in segment_labels:
            relocation_aliases[target] = segment_labels[target]
        else:
            raise ListingError(
                f"initializer target segment {target} has no unique public base "
                "or private ELF section"
            )
    contents = _parse_literal_segment(
        initializer_records, initializer, relocation_aliases
    )
    if _initializer_size(contents) != size:
        raise ListingError(
            f"initializer size for XDATA segment {segment_id} does not match its load image"
        )
    return size, contents


def _parse_code_item_records(
    records: list[tuple[str, str]],
    segment_id: str,
    segment_names: dict[str, str],
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
        has_byte_mask = bool(
            re.search(r"&\s*(?:0x)?ff\b", expression, re.IGNORECASE)
        )
        expression = re.sub(
            r"&\s*(?:0x)?ff\b", "", expression, flags=re.IGNORECASE
        ).strip()

        def strip_outer_parentheses(value: str) -> str:
            value = value.strip()
            while value.startswith("(") and value.endswith(")"):
                depth = 0
                for index, character in enumerate(value):
                    if character == "(":
                        depth += 1
                    elif character == ")":
                        depth -= 1
                        if depth == 0 and index != len(value) - 1:
                            break
                else:
                    value = value[1:-1].strip()
                    continue
                break
            return value

        expression = strip_outer_parentheses(expression)
        shift = re.search(r">>\s*8", expression)
        relocation_kind = "high" if shift else "low"
        if shift:
            expression = expression[: shift.start()]
        elif not has_byte_mask:
            return None
        expression = strip_outer_parentheses(expression)
        boundary_match = re.fullmatch(
            r"(SFB|SFE)\s*\(\s*([A-Za-z0-9_$]+)\s*\)",
            expression,
            re.IGNORECASE,
        )
        if boundary_match:
            boundary, segment_name = boundary_match.groups()
            source_name = _iar_segment_boundary_symbol(boundary, segment_name)
            addend = 0
            target = (source_name, True)
        else:
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
        elif tag in ("t_push_sfb", "t_push_sfe"):
            match = re.fullmatch(r"\(([0-9A-Fa-f]+)\)\s+([0-9A-Fa-f]+)", text)
            if not match:
                raise ListingError(f"malformed {tag} record in segment {segment_id}")
            target, addend = match.groups()
            target = target.upper()
            name = segment_names.get(target)
            if name is None:
                raise ListingError(f"{tag} refers to unknown segment {target}")
            boundary = "sfb" if tag == "t_push_sfb" else "sfe"
            stack.append(
                _IARValue(
                    "external",
                    _iar_segment_boundary_symbol(boundary, name),
                    int(addend, 16),
                )
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
            if lhs.kind in ("rel", "external") and rhs.kind == "pcr":
                if rhs.addend:
                    raise ListingError(
                        f"nonzero PCR addend is unsupported in segment {segment_id}"
                    )
                stack.append(
                    _IARValue(
                        "external_pcrel" if lhs.kind == "external" else "pcrel",
                        lhs.value,
                        lhs.addend,
                    )
                )
            elif lhs.kind in ("pcrel", "external_pcrel") and rhs.kind == "absolute":
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
        elif tag == "t_BNOT":
            value = pop_value(tag)
            # XLINK evaluates this together with SFB(EXT_STACK); expose the
            # combined result as a linker-defined byte symbol.
            if value.kind == "external" and value.value == "?ESP_MASK":
                stack.append(_IARValue("external_not", value.value, value.addend))
            else:
                raise ListingError(f"unsupported t_BNOT expression in segment {segment_id}")
        elif tag == "t_and":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if (
                lhs.kind in ("external", "external_high", "external_high_single")
                and rhs.kind == "external_not"
                and rhs.value == "?ESP_MASK"
                and str(lhs.value).startswith("__iar_sfb_")
            ):
                segment_name = str(lhs.value).removeprefix("__iar_sfb_")
                stack.append(
                    _IARValue(
                        "external",
                        _iar_segment_boundary_symbol("sfb_highmask", segment_name),
                    )
                )
            elif (
                lhs.kind in ("rel", "relative_high", "external", "external_high")
                and rhs.kind == "absolute"
                and int(rhs.value) in (0xFF, 0xFFFF)
            ):
                stack.append(lhs)
            elif (
                lhs.kind == "sfb_byte3"
                and rhs.kind == "absolute"
                and int(rhs.value) in (7, 8)
            ):
                # The output bank fields depend on the final IAR segment bank.
                stack.append(
                    _IARValue("sfb_byte3_mask", lhs.value, int(rhs.value))
                )
            else:
                raise ListingError(
                    f"unsupported t_and operands {lhs.kind}={lhs.value!r}, "
                    f"{rhs.kind}={rhs.value!r} in segment {segment_id}"
                )
        elif tag == "t_SHL":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if lhs.kind == "absolute" and rhs.kind == "absolute":
                stack.append(
                    _IARValue("absolute", int(lhs.value) << int(rhs.value))
                )
            elif lhs.kind == "sfb_byte3_mask" and rhs.kind == "absolute":
                alias = _iar_segment_boundary_symbol(
                    f"sfb_byte3_mask{lhs.addend:02x}_shift{int(rhs.value)}",
                    str(lhs.value),
                )
                stack.append(_IARValue("external", alias))
            else:
                raise ListingError(f"unsupported t_SHL expression in segment {segment_id}")
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
        elif tag == "t_get_byte":
            if not re.fullmatch(r"[0-9A-Fa-f]+", text):
                raise ListingError(f"malformed t_get_byte record in segment {segment_id}")
            byte_index = int(text, 16)
            value = pop_value(tag)
            if value.kind == "absolute":
                stack.append(
                    _IARValue(
                        "absolute",
                        (int(value.value) >> (byte_index * 8)) & 0xFF,
                    )
                )
            elif value.kind in ("rel", "external") and byte_index in (0, 1):
                prefix = "relative" if value.kind == "rel" else "external"
                part = "low" if byte_index == 0 else "high"
                stack.append(
                    _IARValue(
                        f"{prefix}_{part}_single", value.value, value.addend
                    )
                )
            elif (
                value.kind == "external"
                and byte_index == 2
                and str(value.value).startswith("__iar_sfb_")
            ):
                segment_name = str(value.value).removeprefix("__iar_sfb_")
                stack.append(_IARValue("sfb_byte3", segment_name, value.addend))
            else:
                raise ListingError(
                    f"unsupported t_get_byte index {byte_index} in segment {segment_id}"
                )
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
                if external:
                    alias = _asm_name(str(value.value))
                else:
                    target = str(value.value).upper()
                    alias = segment_labels.get(target, str(value.value))
                    external = target in external_segments
                operand = alias
                if value.addend:
                    operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
                items.append(
                    _CodeItem(
                        "byte_reloc_hi" if high else "byte_reloc_lo",
                        operand,
                        alias if external else None,
                        alias if external else None,
                    )
                )
            elif value.kind in ("pcrel", "external_pcrel"):
                if pending_high is not None or not items:
                    raise ListingError(
                        f"invalid PC-relative byte in segment {segment_id}"
                    )
                if value.kind == "external_pcrel":
                    alias = _asm_name(str(value.value))
                    items.append(
                        _CodeItem(
                            "pcrel8", alias, str(value.addend), external_symbol=alias
                        )
                    )
                else:
                    target = str(value.value).upper()
                    if target not in code_segments:
                        raise ListingError(f"relative target segment {target} is unavailable")
                    items.append(
                        _CodeItem("pcrel8", segment_labels[target], str(value.addend))
                    )
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
                        operand += (
                            f"+{value.addend}"
                            if value.addend > 0
                            else str(value.addend)
                        )
                    items.append(
                        _CodeItem(
                            "byte_reloc_lo",
                            operand,
                            alias if external else None,
                            alias if external else None,
                        )
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
            "t_beg_pgm",
            "t_version",
            "t_aux",
            "t_aux_1",
            "t_type",
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
    all_public_code: bool = False,
    module_records: list[tuple[str, str]] | None = None,
) -> dict[str, object]:
    """Convert selected IAR banked code, relays, and supported references.

    The converter preserves machine instructions, lowers known XLIB
    expressions to ELF relocations, and does not adapt the IAR calling
    convention to Clang's ABI.
    """
    if module_records is None:
        listing_text = listing_path.read_text(encoding="latin1")
        modules = _modules(listing_text)
        if module_name not in modules:
            raise ListingError(
                f"module {module_name!r} is not present in the XLIB listing"
            )
        module_records = modules[module_name]
    else:
        listing_text = ""
    if all_public_banked and all_public_code:
        raise ListingError("--all-public-banked cannot be combined with --all-public-code")
    include_all_public = all_public_banked or all_public_code

    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
    local_symbols: dict[str, Symbol] = {}
    external_names: dict[str, str] = {}
    for tag, text in module_records:
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
    for tag, text in module_records:
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
    if include_all_public:
        if symbol_names:
            raise ListingError("all-public conversion cannot be combined with --symbol")
        for name, symbol in symbols.items():
            segment = segments.get(symbol.segment)
            group = _code_group(segment) if segment else None
            if group != "banked" and not (all_public_code and group == "common_code"):
                continue
            requested[name] = symbol
        if not requested:
            raise ListingError("module has no public executable code symbols")
    else:
        if not symbol_names:
            raise ListingError(
                "select a public code symbol or use an all-public conversion option"
            )
        for name in symbol_names:
            symbol = symbols.get(name)
            if symbol is None:
                raise ListingError(f"public code symbol {name!r} was not found")
            segment = segments.get(symbol.segment)
            if segment is None or _code_group(segment) not in ("banked", "common_code"):
                raise ListingError(f"{name} is not in a supported IAR executable segment")
            requested[name] = symbol

    public_bss: list[tuple[Symbol, Segment, int, list[str]]] = []
    public_initialized: list[tuple[Symbol, list[_InitializerAtom]]] = []
    unsupported_public_data: dict[str, str] = {}
    if include_all_public:
        public_xdata_z = [
            name
            for name, symbol in symbols.items()
            if segments.get(symbol.segment) == Segment(symbol.segment, "XDATA", "XDATA_Z")
        ]
        public_xdata_i = [
            name
            for name, symbol in symbols.items()
            if segments.get(symbol.segment) == Segment(symbol.segment, "XDATA", "XDATA_I")
        ]
        if public_xdata_z:
            try:
                public_bss = _selected_bss(
                    listing_text, module_name, public_xdata_z, module_records
                )
            except ListingError:
                for name in public_xdata_z:
                    try:
                        public_bss.extend(
                            _selected_bss(
                                listing_text, module_name, [name], module_records
                            )
                        )
                    except ListingError as symbol_error:
                        unsupported_public_data[name] = str(symbol_error)
        if public_xdata_i:
            try:
                public_initialized = _selected_initialized_xdata(
                    listing_text, module_name, public_xdata_i, module_records
                )
            except ListingError:
                for name in public_xdata_i:
                    try:
                        public_initialized.extend(
                            _selected_initialized_xdata(
                                listing_text, module_name, [name], module_records
                            )
                        )
                    except ListingError as symbol_error:
                        unsupported_public_data[name] = str(symbol_error)
    bundled_public_data_names = {
        symbol.name
        for symbol, _segment, _size, _ops in public_bss
    } | {symbol.name for symbol, _contents in public_initialized}

    selected_segments = {symbol.segment for symbol in requested.values()}
    if include_all_public:
        selected_segments.update(
            symbol.segment
            for symbol in symbols.values()
            if _code_group(segments[symbol.segment]) == "common_const"
        )
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
    if include_all_public:
        selected_constant_segments.update(
            symbol.segment
            for symbol in symbols.values()
            if _constant_segment(segments[symbol.segment])
        )
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

    for ident in selected_xdata_segments:
        if not public_data_symbols.get(ident):
            segment_labels[ident] = f"__iar_data_{module_token}_{ident.lower()}"

    private_xdata: dict[str, tuple[int, bytes | list[_InitializerAtom] | None]] = {}
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
            symbols,
            segment_labels,
            code_records,
            external_names,
        )
        private_xdata[ident] = (size, contents)

    decoded: dict[str, list[_CodeItem]] = {}
    segment_names = {ident: segment.name for ident, segment in segments.items()}
    banked_segments = {
        ident for ident in selected_segments if _code_group(segments[ident]) == "banked"
    }
    source_aliases: dict[str, tuple[str, bool]] = {
        name: (_asm_name(name), True) for name in external_names.values()
    }
    for name, symbol in symbols.items():
        if (
            name in requested
            or name in relay_exports
            or (
                symbol.segment in selected_segments
                and _code_group(segments[symbol.segment]) == "common_const"
            )
            or symbol.segment in selected_constant_segments
        ):
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
            segment_names,
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

    exports_by_segment: dict[str, list[tuple[str, int]]] = {}
    constant_exports_by_segment: dict[str, list[Symbol]] = {}
    exported_originals: dict[str, str] = {}
    for original, symbol in requested.items():
        alias = _asm_name(original)
        exports_by_segment.setdefault(symbol.segment, []).append((alias, symbol.offset))
        exported_originals[original] = alias
    for original, symbol in relay_exports.items():
        alias = _asm_name(symbol.name)
        exports_by_segment.setdefault(symbol.segment, []).append((alias, symbol.offset))
        exported_originals[symbol.name] = alias
    for name, symbol in symbols.items():
        if symbol.segment in selected_constant_segments:
            constant_exports_by_segment.setdefault(symbol.segment, []).append(symbol)
            exported_originals[name] = _asm_name(name)
            continue
        if (
            symbol.segment not in selected_segments
            or _code_group(segments[symbol.segment]) != "common_const"
        ):
            continue
        if symbol.offset != 0:
            raise ListingError(
                f"nonzero CODE_C data-symbol offset is unsupported for {name}"
            )
        alias = _asm_name(name)
        exports_by_segment.setdefault(symbol.segment, []).append((alias, symbol.offset))
        exported_originals[name] = alias

    section_names = {
        "banked": f".mcs51.autobank.iar_{module_token}",
        "relays": f".text.__mcs51_iarrelay_{module_token}",
        "common_code": f".text.__mcs51_iarcode_{module_token}",
        "common_const": f".rodata.__mcs51_iarconst_{module_token}",
    }
    lines = ["; Generated from an IAR XLIB listing by iar2llvm."]
    external_aliases = sorted(
        {
            item.external_symbol or _expression_symbol(str(item.operand))
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
                "pcrel8",
            )
            and (
                item.operand is not None
                if item.kind != "pcrel8"
                else item.external_symbol is not None
            )
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
        and symbol.name not in bundled_public_data_names
        and _asm_name(symbol.name) in external_alias_set
    )
    unresolved.update(
        item.external_symbol
        for items in decoded.values()
        for item in items
        if item.external_symbol
        and item.external_symbol.startswith(("__iar_sfb_", "__iar_sfe_"))
    )
    unresolved_names = sorted(unresolved)
    for alias in external_aliases:
        lines.append(f".globl {_asm_symbol(alias)}")

    selected_order = [ident for ident in segment_order if ident in selected_segments]
    for group in ("banked", "relays", "common_code", "common_const"):
        group_segments = [
            ident
            for ident in selected_order
            if _code_group(segments[ident]) == group
        ]
        if not group_segments:
            continue
        section_flags = "a" if group == "common_const" else "ax"
        lines.extend(
            [
                f'.section {section_names[group]},"{section_flags}",@progbits',
                ".balign 1",
            ]
        )
        for ident in group_segments:
            exports = exports_by_segment.get(ident, [])
            items = decoded[ident]
            size = sum(item.size for item in items)
            lines.append(f"{_asm_symbol(segment_labels[ident])}:")
            for alias, offset in exports:
                if offset > size:
                    raise ListingError(
                        f"public symbol {alias} starts beyond emitted code in segment {ident}"
                    )
                symbol_type = "object" if group == "common_const" else "function"
                if offset == size:
                    symbol_type = "notype"
                asm_alias = _asm_symbol(alias)
                lines.append(f".globl {asm_alias}")
                if symbol_type != "notype":
                    lines.append(f".type {asm_alias},@{symbol_type}")
                lines.append(
                    f".set {asm_alias}, "
                    f"{_asm_expression(f'{segment_labels[ident]}+{offset}')}"
                )
            if not items and exports:
                raise ListingError(
                    f"public symbol {exports[0][0]} has no emitted code in segment {ident}"
                )
            for item in items:
                if item.kind == "byte":
                    lines.append(f".byte 0x{int(item.value):02x}")
                elif item.kind == "byte_reloc":
                    lines.append(f".byte {_asm_expression(str(item.value))}")
                elif item.kind in ("byte_reloc_lo", "byte_reloc_hi"):
                    fixup = "fixup_lo8" if item.kind == "byte_reloc_lo" else "fixup_hi8"
                    lines.extend(
                        [
                            ".byte 0",
                            f".reloc .-1, {fixup}, {_asm_expression(str(item.value))}",
                        ]
                    )
                elif item.kind == "byte_reloc_bit":
                    lines.extend(
                        [
                            ".byte 0",
                            f".reloc .-1, fixup_bit8, {_asm_expression(str(item.value))}",
                        ]
                    )
                elif item.kind == "pcrel8":
                    addend = int(item.operand or "0") - 1
                    expression = _asm_expression(str(item.value))
                    if addend:
                        expression += f"+{addend}" if addend > 0 else str(addend)
                    lines.extend(
                        [
                            ".byte 0",
                            f".reloc .-1, fixup_pcrel8, {expression}",
                        ]
                    )
                elif item.kind == "word":
                    lines.append(f".short 0x{int(item.value):04x}")
                elif item.kind == "word_reloc":
                    lines.append(f".short {_asm_expression(str(item.value))}")
                elif item.kind == "branch":
                    operand = _asm_expression(str(item.value))
                    addend = int(item.operand or "0")
                    if addend:
                        operand += f"+{addend}" if addend > 0 else str(addend)
                    lines.append(f"sjmp {operand}")
                elif item.kind in ("call", "relcall"):
                    lines.append(f"{item.value} {_asm_expression(str(item.operand))}")
                elif item.kind == "dptr":
                    lines.append(f"mov dptr, #{_asm_expression(str(item.value))}")
                elif item.kind == "bankptr":
                    lines.append(f"bankaddr24 {_asm_expression(str(item.value))}")
            public_offsets = sorted(
                {
                    symbol.offset
                    for symbol in symbols.values()
                    if symbol.segment == ident and symbol.offset > 0
                }
            )
            for alias, offset in exports:
                next_offsets = [position for position in public_offsets if position > offset]
                end = min(next_offsets, default=size)
                lines.append(f".size {_asm_symbol(alias)}, {end - offset}")

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
            lines.append(f"{_asm_symbol(segment_labels[ident])}:")
            exports_at_offset: dict[int, list[Symbol]] = {}
            for symbol in constant_exports_by_segment.get(ident, []):
                if symbol.offset < 0 or symbol.offset >= len(contents):
                    raise ListingError(
                        f"public constant {symbol.name} has an invalid offset "
                        f"in segment {ident}"
                    )
                exports_at_offset.setdefault(symbol.offset, []).append(symbol)
            ordered_offsets = sorted(exports_at_offset)
            sizes = {
                offset: (
                    ordered_offsets[index + 1]
                    if index + 1 < len(ordered_offsets)
                    else len(contents)
                )
                - offset
                for index, offset in enumerate(ordered_offsets)
            }
            cursor = 0
            for offset in ordered_offsets:
                for start in range(cursor, offset, 16):
                    lines.append(
                        ".byte "
                        + ", ".join(
                            f"0x{byte:02x}"
                            for byte in contents[start : min(start + 16, offset)]
                        )
                    )
                for symbol in exports_at_offset[offset]:
                    alias = _asm_name(symbol.name)
                    asm_alias = _asm_symbol(alias)
                    lines.extend(
                        [
                            f".globl {asm_alias}",
                            f".type {asm_alias},@object",
                            f"{asm_alias}:",
                        ]
                    )
                cursor = offset
            for start in range(cursor, len(contents), 16):
                lines.append(
                    ".byte "
                    + ", ".join(
                        f"0x{byte:02x}"
                        for byte in contents[start : start + 16]
                    )
                )
            for offset in ordered_offsets:
                for symbol in exports_at_offset[offset]:
                    lines.append(
                        f".size {_asm_symbol(_asm_name(symbol.name))}, {sizes[offset]}"
                    )

    for symbol, _segment, size, _ops in public_bss:
        alias = _asm_name(symbol.name)
        asm_alias = _asm_symbol(alias)
        section_name = f".bss.iar_{symbol.name.encode('utf-8').hex()}"
        lines.extend(
            [
                f'.section {section_name},"aw",@nobits',
                ".balign 1",
                f".globl {asm_alias}",
                f".type {asm_alias},@object",
                f"{asm_alias}:",
                f".zero {size}",
                f".size {asm_alias}, {size}",
            ]
        )
    for symbol, contents in public_initialized:
        alias = _asm_name(symbol.name)
        asm_alias = _asm_symbol(alias)
        section_name = f".data.iar_{symbol.name.encode('utf-8').hex()}"
        lines.extend(
            [
                f'.section {section_name},"aw",@progbits',
                ".balign 1",
                f".globl {asm_alias}",
                f".type {asm_alias},@object",
                f"{asm_alias}:",
            ]
        )
        _emit_initializer(lines, contents)
        lines.append(f".size {asm_alias}, {_initializer_size(contents)}")

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
                    f".type {_asm_symbol(label)},@object",
                    f"{_asm_symbol(label)}:",
                    f".zero {size}",
                    f".size {_asm_symbol(label)}, {size}",
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
            lines.append(f".type {_asm_symbol(label)},@object")
            lines.append(f"{_asm_symbol(label)}:")
            _emit_initializer(lines, [*contents])
            lines.append(
                f".size {_asm_symbol(label)}, {_initializer_size([*contents])}"
            )

    for symbol, _segment, size, _ops in public_bss:
        exported_originals[symbol.name] = _asm_name(symbol.name)
    for symbol, _contents in public_initialized:
        exported_originals[symbol.name] = _asm_name(symbol.name)

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
        "unsupported_public_data": unsupported_public_data,
        "unresolved_elf_symbols": external_aliases,
        "format": "ELF32 MCS-51 archive",
        "scope": (
            "all public IAR code, literal constants, "
            "public XDATA_Z/XDATA_I objects, private XDATA references, and supported "
            "address relocations"
            if include_all_public
            else "selected IAR executable code and supported relocation records"
        ),
        "abi": "IAR calling convention preserved; no Clang ABI adaptation",
    }


def convert_library_listing_to_archive(
    listing_path: Path,
    output: Path,
    llvm_mc: Path | None = None,
    llvm_ar: Path | None = None,
    symbol_names: list[str] | None = None,
    language: str = "00",
    fail_fast: bool = False,
    vb_byte_address: int | None = None,
) -> dict[str, object]:
    """Convert public code modules from an XLIB listing into an ELF archive."""
    listing_text = listing_path.read_text(encoding="latin1")
    modules = _modules(listing_text)
    llvm_mc = (llvm_mc or _default_tool("llvm-mc")).resolve()
    llvm_ar = (llvm_ar or _default_tool("llvm-ar")).resolve()
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    converted_modules: list[str] = []
    skipped_modules: list[str] = []
    unsupported_modules: dict[str, str] = {}
    module_unresolved: list[set[str]] = []
    defined_symbols: set[str] = set()
    unsupported_public_data: dict[str, dict[str, str]] = {}
    wanted_symbols = set(symbol_names or [])
    requested_symbols_found = False

    with tempfile.TemporaryDirectory(prefix="iar2llvm-lib-", dir=output.parent) as temp:
        temp_dir = Path(temp)
        member_objects: list[Path] = []
        for module_name, records in modules.items():
            is_virtual_register_module = (
                module_name.split("@LAN", 1)[0] == "VIRTUAL_REGISTERS"
            )
            language_match = re.search(r"@LAN([A-Fa-f0-9]+)(?:_\d+)?$", module_name)
            if language_match and language_match.group(1).upper() != language.upper():
                skipped_modules.append(module_name)
                continue
            if is_virtual_register_module:
                try:
                    assembly, exports, original_names = _virtual_register_support(
                        records, vb_byte_address
                    )
                    if not exports:
                        skipped_modules.append(module_name)
                        continue
                    module_token = module_name.encode("utf-8").hex()
                    asm_path = temp_dir / f"{module_token}.s"
                    object_path = temp_dir / f"{module_token}.o"
                    asm_path.write_text(
                        "\n".join(assembly) + "\n", encoding="utf-8"
                    )
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
                            f"llvm-mc failed for {module_name} "
                            f"({assemble.returncode}):\n{assemble.stderr.strip()}"
                        )
                except ListingError as error:
                    if fail_fast:
                        raise
                    skipped_modules.append(module_name)
                    unsupported_modules[module_name] = str(error)
                    continue
                member_objects.append(object_path)
                converted_modules.append(module_name)
                defined_symbols.update(exports)
                if wanted_symbols and original_names.intersection(wanted_symbols):
                    requested_symbols_found = True
                continue

            segments: dict[str, Segment] = {}
            public_symbols: list[tuple[str, str]] = []
            for tag, text in records:
                if tag == "t_seg":
                    match = re.search(
                        r"\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+<rel,([A-Z0-9_]+)>\s+'([^']+)'",
                        text,
                    )
                    if match:
                        ident, address_space, name = match.groups()
                        segments[ident.upper()] = Segment(
                            ident.upper(), address_space, name
                        )
                elif tag == "t_symbol_def2":
                    match = re.search(
                        r"PUBLIC_REL\s+\(([0-9A-Fa-f]+)\)\s+[0-9A-Fa-f]+\s+"
                        r"N[0-9A-Fa-f]+\"([^\"]+)\"",
                        text,
                    )
                    if match:
                        public_symbols.append(
                            (match.group(1).upper(), match.group(2))
                        )

            public_names = {name for _ident, name in public_symbols}
            if wanted_symbols and not public_names.intersection(wanted_symbols):
                continue

            if any(
                ident in segments
                and _code_group(segments[ident]) in ("banked", "common_code")
                for ident, _name in public_symbols
            ):
                module_archive = temp_dir / f"{module_name.encode('utf-8').hex()}.a"
                try:
                    result = convert_code_listing_to_archive(
                        listing_path,
                        module_name,
                        [],
                        module_archive,
                        llvm_mc,
                        llvm_ar,
                        all_public_code=True,
                        module_records=records,
                    )
                    extract_dir = temp_dir / f"extract-{module_name.encode('utf-8').hex()}"
                    extract_dir.mkdir()
                    extract = subprocess.run(
                        [str(llvm_ar), "x", str(module_archive)],
                        cwd=extract_dir,
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    if extract.returncode:
                        raise ListingError(
                            f"llvm-ar extraction failed for {module_name} "
                            f"({extract.returncode}):\n{extract.stderr.strip()}"
                        )
                    objects = sorted(extract_dir.glob("*.o"))
                    if len(objects) != 1:
                        raise ListingError(
                            f"expected one ELF object in converted module "
                            f"{module_name}, found {len(objects)}"
                        )
                except ListingError as error:
                    if fail_fast:
                        raise
                    skipped_modules.append(module_name)
                    unsupported_modules[module_name] = str(error)
                    continue

                if wanted_symbols and public_names.intersection(wanted_symbols):
                    requested_symbols_found = True
                converted_modules.append(module_name)
                module_unresolved.append(set(result["unresolved"]))
                defined_symbols.update(result["symbols"].values())
                module_unsupported = result["unsupported_public_data"]
                if module_unsupported:
                    unsupported_public_data[module_name] = module_unsupported
                member_objects.extend(objects)
            elif public_symbols and (not wanted_symbols or public_names.intersection(wanted_symbols)):
                skipped_modules.append(module_name)

        if not member_objects or (wanted_symbols and not requested_symbols_found):
            details = ""
            if unsupported_modules:
                details = "\n" + "\n".join(
                    f"  {module}: {reason}"
                    for module, reason in unsupported_modules.items()
                )
            if wanted_symbols:
                raise ListingError(
                    "XLIB listing has no convertible modules defining: "
                    + ", ".join(sorted(wanted_symbols))
                    + details
                )
            raise ListingError(
                "XLIB listing has no convertible public code modules" + details
            )

        archive_path = temp_dir / output.name
        archive = subprocess.run(
            [str(llvm_ar), "rcs", str(archive_path), *(str(path) for path in member_objects)],
            capture_output=True,
            text=True,
            check=False,
        )
        if archive.returncode:
            raise ListingError(
                f"llvm-ar archive creation failed ({archive.returncode}):\n"
                f"{archive.stderr.strip()}"
            )
        archive_path.replace(output)

    unresolved = sorted(
        {
            name
            for names in module_unresolved
            for name in names
            if _asm_name(name) not in defined_symbols
        }
    )
    limitations: list[str] = []
    if any(
        module.split("@LAN", 1)[0] == "VIRTUAL_REGISTERS"
        for module in converted_modules
    ):
        limitations.extend(
            [
                "IAR PSP/XSP storage is emitted as DATA bss, but stack regions "
                "are not reserved and PSP/XSP are not initialized; startup must do so"
            ]
        )
        if vb_byte_address is not None:
            limitations.append(
                f"IAR ?VB is fixed at 0x{vb_byte_address:02x}; reserve that "
                "byte from other bit-addressed storage"
            )
        else:
            limitations.append(
                "IAR ?VB has no address in XLIB; provide "
                "--vb-byte-address to define it"
            )
    if any(
        name.startswith(("__iar_sfb_", "__iar_sfe_")) for name in unresolved
    ):
        limitations.append(
            "IAR SFB/SFE references and derived masked-address bytes need linker "
            "definitions before those runtime helpers can link"
        )
    return {
        "output": str(output),
        "modules": converted_modules,
        "skipped_modules": skipped_modules,
        "unsupported_modules": unsupported_modules,
        "partial": bool(unsupported_modules or unsupported_public_data),
        "unresolved": unresolved,
        "unsupported_public_data": unsupported_public_data,
        "language": language,
        "format": "ELF32 MCS-51 archive",
        "scope": (
            "module-wide public executable code, supported public data, and "
            "IAR virtual-register symbols"
        ),
        "limitations": limitations,
        "abi": "IAR calling convention preserved; no Clang ABI adaptation",
    }
