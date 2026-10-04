from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from .archive import UBROFError, extract_library, inspect_library
from .listing import (
    ListingError,
    convert_code_listing_to_archive,
    convert_const_listing_to_archive,
    convert_initialized_listing_to_archive,
    convert_listing_to_archive,
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="iar2llvm",
        description="Inspect, extract, and partially convert IAR UBROF 11 MCS-51 libraries.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect = subparsers.add_parser("inspect", help="list modules and byte ranges")
    inspect.add_argument("library", type=Path)
    inspect.add_argument("--json", action="store_true")

    extract = subparsers.add_parser("extract", help="extract complete .r51 module files")
    extract.add_argument("library", type=Path)
    extract.add_argument("--out-dir", type=Path, required=True)
    extract.add_argument("--json", action="store_true")

    convert = subparsers.add_parser(
        "convert-listing",
        help="convert selected zero-initialized XDATA_Z symbols from an XLIB listing",
    )
    convert.add_argument("listing", type=Path)
    convert.add_argument("--module", required=True)
    convert.add_argument("--symbol", action="append", required=True, dest="symbols")
    convert.add_argument("--out", type=Path, required=True)
    convert.add_argument("--llvm-mc", type=Path)
    convert.add_argument("--llvm-ar", type=Path)
    convert.add_argument("--json", action="store_true")

    convert_code = subparsers.add_parser(
        "convert-code-listing",
        help="convert selected or all public IAR BANKED_CODE functions and relays",
    )
    convert_code.add_argument("listing", type=Path)
    convert_code.add_argument("--module", required=True)
    convert_code.add_argument("--symbol", action="append", dest="symbols", default=[])
    convert_code.add_argument("--all-public-banked", action="store_true")
    convert_code.add_argument("--out", type=Path, required=True)
    convert_code.add_argument("--llvm-mc", type=Path)
    convert_code.add_argument("--llvm-ar", type=Path)
    convert_code.add_argument("--json", action="store_true")

    convert_const = subparsers.add_parser(
        "convert-const-listing",
        help="convert selected absolute CONST/XDATA_ROM_C objects from an XLIB listing",
    )
    convert_const.add_argument("listing", type=Path)
    convert_const.add_argument("--module", required=True)
    convert_const.add_argument("--symbol", action="append", required=True, dest="symbols")
    convert_const.add_argument("--out", type=Path, required=True)
    convert_const.add_argument("--llvm-mc", type=Path)
    convert_const.add_argument("--llvm-ar", type=Path)
    convert_const.add_argument("--json", action="store_true")

    convert_initialized = subparsers.add_parser(
        "convert-init-listing",
        help="convert literal XDATA_I initializers from XDATA_ID records",
    )
    convert_initialized.add_argument("listing", type=Path)
    convert_initialized.add_argument("--module", required=True)
    convert_initialized.add_argument(
        "--symbol", action="append", required=True, dest="symbols"
    )
    convert_initialized.add_argument("--out", type=Path, required=True)
    convert_initialized.add_argument("--llvm-mc", type=Path)
    convert_initialized.add_argument("--llvm-ar", type=Path)
    convert_initialized.add_argument("--json", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "inspect":
            payload = inspect_library(args.library)
        elif args.command == "extract":
            payload = extract_library(args.library, args.out_dir)
        elif args.command == "convert-listing":
            payload = convert_listing_to_archive(
                args.listing,
                args.module,
                args.symbols,
                args.out,
                args.llvm_mc,
                args.llvm_ar,
            )
        elif args.command == "convert-code-listing":
            payload = convert_code_listing_to_archive(
                args.listing,
                args.module,
                args.symbols,
                args.out,
                args.llvm_mc,
                args.llvm_ar,
                args.all_public_banked,
            )
        elif args.command == "convert-const-listing":
            payload = convert_const_listing_to_archive(
                args.listing,
                args.module,
                args.symbols,
                args.out,
                args.llvm_mc,
                args.llvm_ar,
            )
        else:
            payload = convert_initialized_listing_to_archive(
                args.listing,
                args.module,
                args.symbols,
                args.out,
                args.llvm_mc,
                args.llvm_ar,
            )
    except (OSError, UBROFError, ListingError) as error:
        print(f"iar2llvm: error: {error}", file=sys.stderr)
        return 1

    if args.command in (
        "convert-listing",
        "convert-code-listing",
        "convert-const-listing",
        "convert-init-listing",
    ):
        if args.json:
            print(json.dumps(payload, indent=2, sort_keys=True))
        else:
            print(f"Archive: {payload['output']}")
            print(f"Format: {payload['format']}")
            print(f"Scope: {payload['scope']}")
            if args.command == "convert-code-listing":
                for name, alias in payload["symbols"].items():
                    print(f"  {name} -> {alias}")
                print("  Unresolved IAR symbols: " + ", ".join(payload["unresolved"]))
            else:
                for symbol in payload["symbols"]:
                    print(f"  {symbol['name']:<24} {symbol['size']:>8} bytes")
        return 0

    if args.json:
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(f"Library: {payload['library']}")
        print(f"Format: {payload['format']}")
        print(f"Modules: {len(payload['modules'])}")
        if args.command == "extract":
            print(f"Output: {payload['output_dir']}")
        for module in payload["modules"]:
            print(f"  {module['name']:<24} {module['size']:>8} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
