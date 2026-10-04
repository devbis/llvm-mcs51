from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
from pathlib import Path
import re


# Z-Stack's CC2530 libraries use this UBROF 11 MCS-51 module header. The
# following byte is the module-name length, followed by the ASCII name.
UBROF_MODULE_HEADER = bytes.fromhex("00 0b 05 10 03 0a 76")
UBROF_VERSION = 11
MODULE_NAME_RE = re.compile(rb"^[A-Za-z_][A-Za-z0-9_]*$")
MODULE_MARKER = bytes.fromhex("bd 0b")


class UBROFError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class ModuleSpan:
    name: str
    start: int
    end: int

    @property
    def size(self) -> int:
        return self.end - self.start

    def to_dict(self, data: bytes) -> dict[str, object]:
        result = asdict(self)
        result["size"] = self.size
        result["sha256"] = hashlib.sha256(data[self.start : self.end]).hexdigest()
        return result


def _module_at(data: bytes, offset: int) -> tuple[str, int] | None:
    name_length_offset = offset + len(UBROF_MODULE_HEADER)
    if name_length_offset >= len(data):
        return None

    name_length = data[name_length_offset]
    name_start = name_length_offset + 1
    name_end = name_start + name_length
    if name_length == 0 or name_end + len(MODULE_MARKER) > len(data):
        return None

    raw_name = data[name_start:name_end]
    if not MODULE_NAME_RE.fullmatch(raw_name):
        return None
    if data[name_end : name_end + len(MODULE_MARKER)] != MODULE_MARKER:
        return None
    return raw_name.decode("ascii"), name_end


def find_modules(data: bytes) -> list[ModuleSpan]:
    """Return complete UBROF module slices from a concatenated IAR library.

    Unlike the previous string-marker approach, spans begin at the UBROF
    module header, so extracting a member preserves its version and target
    fields. The signature is intentionally limited to the UBROF header used
    by the bundled Z-Stack libraries; unknown variants fail closed.
    """
    starts: list[tuple[int, str]] = []
    offset = 0
    while True:
        offset = data.find(UBROF_MODULE_HEADER, offset)
        if offset < 0:
            break
        parsed = _module_at(data, offset)
        if parsed is not None:
            name, _ = parsed
            starts.append((offset, name))
        offset += 1

    if not starts:
        raise UBROFError("no supported UBROF 11 MCS-51 module headers found")
    if starts[0][0] != 0:
        raise UBROFError(
            f"unexpected data before first UBROF module (offset 0x{starts[0][0]:x})"
        )

    spans = [
        ModuleSpan(name, start, starts[index + 1][0] if index + 1 < len(starts) else len(data))
        for index, (start, name) in enumerate(starts)
    ]
    names = [span.name for span in spans]
    if len(names) != len(set(names)):
        duplicates = sorted(name for name in set(names) if names.count(name) > 1)
        raise UBROFError("duplicate module names: " + ", ".join(duplicates))
    return spans


def inspect_library(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    modules = find_modules(data)
    return {
        "library": str(path.resolve()),
        "format": f"IAR UBROF {UBROF_VERSION}",
        "size": len(data),
        "modules": [span.to_dict(data) for span in modules],
    }


def extract_library(path: Path, out_dir: Path) -> dict[str, object]:
    data = path.read_bytes()
    modules = find_modules(data)
    out_dir.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    for span in modules:
        output = out_dir / f"{span.name}.r51"
        member = data[span.start : span.end]
        output.write_bytes(member)
        entry = span.to_dict(data)
        entry["path"] = str(output.resolve())
        entries.append(entry)

    return {
        "library": str(path.resolve()),
        "format": f"IAR UBROF {UBROF_VERSION}",
        "size": len(data),
        "output_dir": str(out_dir.resolve()),
        "modules": entries,
    }
