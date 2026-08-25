#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Triage Shadertoy-style dumps into 1D / 2D / 3D / reject lanes.

Usage:
  python tools/shader_triage/triage_shaders.py C:\\path\\to\\effect-shaders [--limit N] [--out report.csv]
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

REJECT_PATTERNS = [
    (r"iChannel\d", "multipass/channel texture"),
    (r"iMouse", "mouse input"),
    (r"texture\s*\(", "texture sample"),
    (r"raymarch|ray.?march|map\s*\(\s*vec3", "raymarch/SDF camera"),
    (r"iAudio|FFT|microphone", "audio"),
]

SOFT_2D_HINTS = [
    r"mainImage\s*\(",
    r"noise\s*\(",
    r"fbm\s*\(",
    r"plasma",
    r"ripple",
    r"sin\s*\(",
]

STRIP_1D_HINTS = [
    r"fract\s*\(\s*uv\.x",
    r"led",
    r"strip",
    r"chase",
    r"comet",
    r"meteor",
]

VOLUME_3D_HINTS = [
    r"p01",
    r"volumeMain",
    r"vec3\s+p\b",
    r"fbm\s*\(\s*vec3",
]


def classify(text: str, name: str) -> tuple[str, str]:
    low = text.lower()
    reasons = []
    for pat, reason in REJECT_PATTERNS:
        if re.search(pat, text, re.I):
            reasons.append(reason)
    if reasons:
        return "reject", "; ".join(reasons)

    score_1d = sum(1 for p in STRIP_1D_HINTS if re.search(p, low))
    score_2d = sum(1 for p in SOFT_2D_HINTS if re.search(p, low))
    score_3d = sum(1 for p in VOLUME_3D_HINTS if re.search(p, low))

    if "mainimage" not in low and "spatialmain" not in low and "volumemain" not in low:
        return "reject", "no mainImage/spatialMain/volumeMain"

    if score_3d >= 2 and score_3d >= score_2d:
        return "3d_volume_candidate", f"hints={score_3d}"
    if score_1d >= 2 and score_1d > score_2d:
        return "1d_kernel_candidate", f"hints={score_1d}"
    if score_2d >= 1:
        return "2d_spatialmain_candidate", f"hints={score_2d}"
    return "review", f"weak_match name={name}"


def iter_files(root: Path):
    for p in sorted(root.iterdir()):
        if p.is_file() and p.suffix.lower() in ("", ".glsl", ".frag", ".fs", ".txt"):
            yield p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=Path)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args()
    if not args.root.is_dir():
        print(f"not a directory: {args.root}", file=sys.stderr)
        return 1

    rows = []
    for i, path in enumerate(iter_files(args.root)):
        if args.limit and i >= args.limit:
            break
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError as e:
            rows.append((path.name, path.stat().st_size if path.exists() else 0, "reject", str(e)))
            continue
        lane, reason = classify(text, path.name)
        rows.append((path.name, path.stat().st_size, lane, reason))

    counts: dict[str, int] = {}
    for _, _, lane, _ in rows:
        counts[lane] = counts.get(lane, 0) + 1

    print("counts:", counts)
    out = args.out or (Path(__file__).resolve().parent / "last_triage.csv")
    with out.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["name", "bytes", "lane", "reason"])
        w.writerows(rows)
    print(f"wrote {out} ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
