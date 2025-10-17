#!/usr/bin/env python3
"""OpenRGB-style lint checks used by pre-commit and CI.

Checks performed:
  * ASCII-only Markdown docs in docs/guides/ and root README/CONTRIBUTING
  * No hard tab characters in plugin source directories (Audio, Effects3D, sdk, ui)
  * No left-over merge conflict markers
  * Optional scan for uto keyword in plugin directories (warns, but non-failing)

Exit codes: 0 success, 1 failure.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC_TARGETS = [ROOT / "README.md", ROOT / "CONTRIBUTING.md"]
DOC_TARGETS.extend((ROOT / "docs" / "guides").glob("*.md"))
SOURCE_DIRS = [ROOT / "Audio", ROOT / "Effects3D", ROOT / "sdk", ROOT / "ui"]
CONFLICT_RE = re.compile(r"^<<<<<<< |^=======|^>>>>>>> ")
AUTO_RE = re.compile(r"\bauto\b")


def ascii_check(path: Path) -> list[str]:
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        diagnostics.append(f"{path.relative_to(ROOT)}: not UTF-8")
        return diagnostics

    for idx, line in enumerate(text.splitlines(), 1):
        if any(ord(ch) >= 128 for ch in line):
            diagnostics.append(f"{path.relative_to(ROOT)}:{idx}: non-ASCII character")
    return diagnostics


def tab_check(path: Path) -> list[str]:
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return diagnostics

    for idx, line in enumerate(text.splitlines(), 1):
        if "\t" in line:
            diagnostics.append(f"{path.relative_to(ROOT)}:{idx}: tab character")
    return diagnostics


def conflict_check(path: Path) -> list[str]:
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except UnicodeDecodeError:
        return diagnostics
    for idx, line in enumerate(text.splitlines(), 1):
        if CONFLICT_RE.match(line):
            diagnostics.append(f"{path.relative_to(ROOT)}:{idx}: merge conflict marker")
    return diagnostics


def auto_check(path: Path) -> list[str]:
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return diagnostics
    for idx, line in enumerate(text.splitlines(), 1):
        if AUTO_RE.search(line):
            diagnostics.append(f"{path.relative_to(ROOT)}:{idx}: auto keyword usage")
    return diagnostics


def scan_source_files() -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    for directory in SOURCE_DIRS:
        if not directory.exists():
            continue
        for ext in ("*.cpp", "*.h", "*.inl", "*.hpp"):
            for path in directory.rglob(ext):
                errors.extend(tab_check(path))
                errors.extend(conflict_check(path))
                warnings.extend(auto_check(path))
    return errors, warnings


def scan_docs() -> list[str]:
    errors: list[str] = []
    for path in DOC_TARGETS:
        if path.exists():
            errors.extend(ascii_check(path))
            errors.extend(conflict_check(path))
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="OpenRGB plugin lint checks")
    parser.add_argument("paths", nargs="*", help="Optional file paths to restrict the scan")
    parser.add_argument("--fail-on-auto", action="store_true", help="Treat auto keyword as hard failure")
    args = parser.parse_args(argv)

    errors: list[str] = []
    warnings: list[str] = []

    errors.extend(scan_docs())
    src_errors, src_warnings = scan_source_files()
    errors.extend(src_errors)
    warnings.extend(src_warnings)

    if args.fail_on_auto:
        errors.extend(src_warnings)
        warnings = []

    if warnings:
        print("Warnings:")
        for entry in warnings:
            print(f"  {entry}")

    if errors:
        print("Errors:")
        for entry in errors:
            print(f"  {entry}")
        return 1

    print("Lint checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
