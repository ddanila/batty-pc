#!/usr/bin/env python3
"""Analyze BATTY PROFILE.TXT output."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def read_text() -> str:
    if len(sys.argv) > 1 and sys.argv[1] != "-":
        return Path(sys.argv[1]).read_text(errors="replace")
    return sys.stdin.read()


def number_for(label: str, text: str) -> int | None:
    match = re.search(rf"^\s*{re.escape(label)}:\s+([0-9]+)", text, re.M)
    if not match:
        return None
    return int(match.group(1))


def frame_count(text: str) -> int | None:
    match = re.search(r"^Profiling Report over\s+([0-9]+)\s+frames:", text, re.M)
    if not match:
        return None
    return int(match.group(1))


def main() -> int:
    text = read_text()
    buckets = {
        "paint_bg_to_buff": number_for("paint_bg_to_buff", text),
        "paint_frame_to_buff": number_for("paint_frame_to_buff", text),
        "HUD / Lives": number_for("HUD / Lives", text),
        "render_brick_band": number_for("render_brick_band", text),
        "buff_to_vga": number_for("buff_to_vga", text),
    }
    frames = frame_count(text)
    static_rebuilds = number_for("static rebuilds", text)
    vga_rects = number_for("VGA rect flushes", text)
    vga_bytes = number_for("VGA bytes written", text)

    present = {k: v for k, v in buckets.items() if v is not None}
    if not present:
        print("No recognized profile counters found.")
        return 1

    total = sum(present.values())
    ordered = sorted(present.items(), key=lambda item: item[1], reverse=True)

    print("\nAnalysis:")
    for name, value in ordered:
        pct = (value * 100.0 / total) if total else 0.0
        print(f"  {name:20s} {pct:5.1f}%")

    if frames:
        print(f"  frames:              {frames}")
    if static_rebuilds is not None:
        print(f"  static rebuilds:     {static_rebuilds}")
    if vga_rects is not None:
        print(f"  VGA rects/frame:     {vga_rects / max(frames or 1, 1):.2f}")
    if vga_bytes is not None:
        print(f"  VGA bytes/frame:     {vga_bytes / max(frames or 1, 1):.0f}")

    top = ordered[0][0]
    print("\nNext target:")
    if top == "HUD / Lives":
        print("  HUD/lives composition dominates; localize lives changes and reduce top-row restores.")
    elif top == "paint_bg_to_buff":
        print("  Background restore dominates; reduce static rebuilds and copied dirty byte ranges.")
    elif top == "buff_to_vga":
        print("  VGA output dominates; optimize rectangle flushes or reduce dirty bytes.")
    elif top == "render_brick_band":
        print("  Brick-band composition dominates; update individual brick cells instead of rebuilding the band.")
    else:
        print(f"  Investigate {top}.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
