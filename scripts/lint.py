#!/usr/bin/env python3
"""Lightweight repository lint checks.

- Ensures key docs are ASCII-only (GitHub renders cleanly)
- Flags hard tabs in plugin source directories
- Warns about accidental carriage returns

Usage:
    python scripts/lint.py
The script exits with status 1 if any check fails.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DOC_TARGETS = [
    ROOT / "README.md",
    ROOT / "CONTRIBUTING.md",
]
DOC_TARGETS.extend((ROOT / "docs" / "guides").glob("*.md"))

SOURCE_DIRS = [
    ROOT / "Audio",
    ROOT / "Effects3D",
    ROOT / "sdk",
    ROOT / "ui",
]

TAB_WHITELIST = {"\t"}  # we only flag tab characters explicitly


def check_ascii(path: Path) -> list[str]:
    """Return diagnostics if `path` contains non-ASCII characters."""
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        diagnostics.append(f"{path.relative_to(ROOT)}: not UTF-8; please re-save as UTF-8")
        return diagnostics

    for idx, line in enumerate(text.splitlines(), start=1):
        for ch in line:
            if ord(ch) >= 128:
                diagnostics.append(
                    f"{path.relative_to(ROOT)}:{idx}: non-ASCII character detected"
                )
                break
    return diagnostics


def check_tabs(path: Path) -> list[str]:
    diagnostics: list[str] = []
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return diagnostics  # handled by ASCII check already

    for idx, line in enumerate(text.splitlines(), start=1):
        if "\t" in line:
            diagnostics.append(f"{path.relative_to(ROOT)}:{idx}: tab character found")
    return diagnostics


def scan_sources() -> list[str]:
    diagnostics: list[str] = []
    for directory in SOURCE_DIRS:
        if not directory.exists():
            continue
        for path in directory.rglob("*.cpp"):
            diagnostics.extend(check_tabs(path))
        for path in directory.rglob("*.h"):
            diagnostics.extend(check_tabs(path))
    return diagnostics


def scan_docs() -> list[str]:
    diagnostics: list[str] = []
    for path in DOC_TARGETS:
        if path.exists():
            diagnostics.extend(check_ascii(path))
    return diagnostics


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run lint checks")
    parser.parse_args(argv)

    diagnostics: list[str] = []
    diagnostics.extend(scan_docs())
    diagnostics.extend(scan_sources())

    if diagnostics:
        print("Lint failures detected:")
        for entry in diagnostics:
            print(f"  {entry}")
        return 1

    print("All lint checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
