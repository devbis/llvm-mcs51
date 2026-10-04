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
