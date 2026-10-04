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


@dataclass(frozen=True, slots=True)
class _CodeItem:
    kind: str
    value: int | str
    operand: str | None = None

    @property
    def size(self) -> int:
        return {"byte": 1, "branch": 2, "call": 3, "bankptr": 3}[self.kind]


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
    return None


def _parse_code_item_records(
    records: list[tuple[str, str]],
    segment_id: str,
    segment_labels: dict[str, str],
    external_names: dict[str, str],
) -> list[_CodeItem]:
    items: list[_CodeItem] = []
    stack: list[_IARValue] = []
    pending_high: str | None = None

    def pop_value(tag: str) -> _IARValue:
        if not stack:
            raise ListingError(f"{tag} underflows the expression stack in segment {segment_id}")
        return stack.pop()

    for tag, text in records:
        if tag == "t_abs_8":
            if not re.fullmatch(r"[0-9A-Fa-f]{2}", text):
                raise ListingError(f"malformed t_abs_8 record in segment {segment_id}")
            items.append(_CodeItem("byte", int(text, 16)))
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
        elif tag == "t_minus":
            rhs = pop_value(tag)
            lhs = pop_value(tag)
            if lhs.kind == "rel" and rhs.kind == "pcr":
                stack.append(_IARValue("pcrel", lhs.value, lhs.addend - rhs.addend))
            elif lhs.kind == "pcrel" and rhs.kind == "absolute":
                stack.append(_IARValue("pcrel", lhs.value, lhs.addend - int(rhs.value)))
            else:
                raise ListingError(f"unsupported t_minus expression in segment {segment_id}")
        elif tag == "t_copy":
            if not stack:
                raise ListingError(f"t_copy underflows the expression stack in segment {segment_id}")
            stack.append(stack[-1])
        elif tag == "t_lsr":
            if not re.fullmatch(r"[0-9A-Fa-f]+", text):
                raise ListingError(f"malformed t_lsr record in segment {segment_id}")
            shift = int(text, 16)
            value = pop_value(tag)
            if value.kind == "external" and shift == 8:
                stack.append(_IARValue("external_high", value.value, value.addend))
            elif value.kind == "absolute":
                stack.append(_IARValue("absolute", int(value.value) >> shift))
            else:
                raise ListingError(f"unsupported t_lsr expression in segment {segment_id}")
        elif tag == "t_check":
            # Range masks are consumed by XLINK and do not change the value.
            continue
        elif tag == "t_pop_8":
            value = pop_value(tag)
            if value.kind == "pcrel":
                if pending_high is not None or not items or items[-1] != _CodeItem("byte", 0x80):
                    raise ListingError(
                        f"only SJMP PC-relative bytes are supported in segment {segment_id}"
                    )
                target = str(value.value).upper()
                if target not in segment_labels:
                    raise ListingError(f"relative target segment {target} is unavailable")
                items[-1] = _CodeItem("branch", segment_labels[target], str(value.addend))
            elif value.kind == "external_high":
                if pending_high is not None:
                    raise ListingError(f"nested 16-bit external address in segment {segment_id}")
                pending_high = str(value.value)
            elif value.kind == "external":
                if value.addend:
                    raise ListingError(
                        f"external call addends are unsupported in segment {segment_id}"
                    )
                if pending_high != value.value or not items or items[-1].kind != "byte":
                    raise ListingError(f"unsupported external address expression in segment {segment_id}")
                opcode = int(items[-1].value)
                if opcode not in (0x02, 0x12):
                    raise ListingError(
                        f"16-bit external address is not an LJMP/LCALL in segment {segment_id}"
                    )
                items.pop()
                items.append(
                    _CodeItem("call", "ljmp" if opcode == 0x02 else "lcall", _asm_name(str(value.value)))
                )
                pending_high = None
            else:
                raise ListingError(f"unsupported t_pop_8 expression in segment {segment_id}")
        elif tag == "t_pop_24":
            value = pop_value(tag)
            if value.kind != "rel":
                raise ListingError(f"unsupported t_pop_24 expression in segment {segment_id}")
            target = str(value.value).upper()
            if target not in segment_labels:
                raise ListingError(f"24-bit target segment {target} is unavailable")
            operand = segment_labels[target]
            if value.addend:
                operand += f"+{value.addend}" if value.addend > 0 else str(value.addend)
            items.append(_CodeItem("bankptr", operand))
        elif tag == "t_delete_tos":
            pop_value(tag)
        elif tag in ("t_asm_mode_change", "t_stack_error", "t_call_frame_info"):
            continue
        elif tag.startswith("t_abs_"):
            raise ListingError(f"unsupported code emission record {tag} in segment {segment_id}")
        elif tag.startswith("t_push_") or tag.startswith("t_pop_"):
            raise ListingError(f"unsupported expression record {tag} in segment {segment_id}")
        else:
            raise ListingError(f"unsupported XLIB record {tag} in code segment {segment_id}")

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
) -> dict[str, object]:
    """Convert selected IAR banked code and relay records into an ELF archive.

    This initial executable subset accepts XLIB BANKED_CODE bodies, their
    BANK_RELAYS, SJMP references, absolute LCALL/LJMP references, and 24-bit
    banked-code pointers. It preserves machine instructions rather than
    translating IAR function bodies or their calling convention.
    """
    modules = _modules(listing_path.read_text(encoding="latin1"))
    if module_name not in modules:
        raise ListingError(f"module {module_name!r} is not present in the XLIB listing")

    segments: dict[str, Segment] = {}
    symbols: dict[str, Symbol] = {}
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
            if current_segment and current_segment not in code_records:
                code_records[current_segment] = []
                if current_segment in segments and _code_group(segments[current_segment]):
                    segment_order.append(current_segment)
            continue
        if current_segment and current_segment in segments and _code_group(segments[current_segment]):
            code_records.setdefault(current_segment, []).append((tag, text))

    requested: dict[str, Symbol] = {}
    if not symbol_names:
        raise ListingError("select at least one public BANKED_CODE symbol")
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

    # Relocatable references and short-branch destinations keep their IAR
    # segments in the selected module group, including common helper segments.
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
            if target_segment is None or _code_group(target_segment) is None:
                raise ListingError(f"code segment {source} references unsupported segment {target}")
            if target not in selected_segments:
                selected_segments.add(target)
                pending.append(target)

    module_token = module_name.encode("utf-8").hex()
    segment_labels = {
        ident: f"__iar_seg_{module_token}_{ident.lower()}" for ident in selected_segments
    }
    decoded: dict[str, list[_CodeItem]] = {}
    for ident in selected_segments:
        decoded[ident] = _parse_code_item_records(
            code_records.get(ident, []), ident, segment_labels, external_names
        )

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
    }
    lines = ["; Generated from an IAR XLIB listing by iar2llvm."]
    external_aliases = sorted(
        {
            _asm_name(str(item.operand))
            for items in decoded.values()
            for item in items
            if item.kind == "call" and item.operand is not None
        }
    )
    external_alias_set = set(external_aliases)
    unresolved_names = sorted(
        name for name in external_names.values() if _asm_name(name) in external_alias_set
    )
    for alias in external_aliases:
        lines.append(f".globl {alias}")

    selected_order = [ident for ident in segment_order if ident in selected_segments]
    for group in ("banked", "relays"):
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
                elif item.kind == "branch":
                    operand = str(item.value)
                    addend = int(item.operand or "0")
                    if addend:
                        operand += f"+{addend}" if addend > 0 else str(addend)
                    lines.append(f"sjmp {operand}")
                elif item.kind == "call":
                    lines.append(f"{item.value} {item.operand}")
                elif item.kind == "bankptr":
                    lines.append(f"bankaddr24 {item.value}")
            size = sum(item.size for item in items)
            for alias in exports:
                lines.append(f".size {alias}, {size}")

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
        "scope": "IAR BANKED_CODE/BANK_RELAYS code and confirmed ECC relocation records",
        "abi": "IAR calling convention preserved; no Clang ABI adaptation",
    }
