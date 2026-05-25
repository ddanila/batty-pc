#!/usr/bin/env python3
"""Analyze BATTY PROFILE.TXT output."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def read_text(path: str) -> str:
    if path != "-":
        return Path(path).read_text(errors="replace")
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
    parser = argparse.ArgumentParser()
    parser.add_argument("profile", nargs="?", default="-")
    parser.add_argument("--json", type=Path, dest="json_out")
    parser.add_argument("--min-frames", type=int, default=0)
    args = parser.parse_args()

    text = read_text(args.profile)
    buckets = {
        "paint_bg_to_buff": number_for("paint_bg_to_buff", text),
        "paint_frame_to_buff": number_for("paint_frame_to_buff", text),
        "HUD / Lives": number_for("HUD / Lives", text),
        "render_brick_band": number_for("render_brick_band", text),
        "buff_to_vga": number_for("buff_to_vga", text),
    }
    frames = frame_count(text)
    static_rebuilds = number_for("static rebuilds", text)
    full_dynamic_frames = number_for("full dynamic frames", text)
    ball_only_frames = number_for("ball-only frames", text)
    ball_object_frames = number_for("ball-object frames", text)
    ball_blockers = {
        "bat": number_for("ball block bat", text),
        "static": number_for("ball block static", text),
        "HUD": number_for("ball block HUD", text),
        "objects": number_for("ball block objects", text),
        "bricks": number_for("ball block bricks", text),
        "balls": number_for("ball block balls", text),
        "bat FX": number_for("ball block bat FX", text),
    }
    vga_rects = number_for("VGA rect flushes", text)
    vga_bytes = number_for("VGA bytes written", text)

    present = {k: v for k, v in buckets.items() if v is not None}
    if not present:
        print("No recognized profile counters found.")
        return 1

    total = sum(present.values())
    ordered = sorted(present.items(), key=lambda item: item[1], reverse=True)
    if args.min_frames and (frames is None or frames < args.min_frames):
        print(f"FAIL: profile captured {frames or 0} frames, expected at least {args.min_frames}")
        return 1

    print("\nAnalysis:")
    for name, value in ordered:
        pct = (value * 100.0 / total) if total else 0.0
        print(f"  {name:20s} {pct:5.1f}%")

    if frames:
        print(f"  frames:              {frames}")
    if static_rebuilds is not None:
        print(f"  static rebuilds:     {static_rebuilds}")
    if full_dynamic_frames is not None:
        pct = (full_dynamic_frames * 100.0 / frames) if frames else 0.0
        print(f"  full dynamic frames: {full_dynamic_frames} ({pct:.1f}%)")
    if ball_only_frames is not None:
        pct = (ball_only_frames * 100.0 / frames) if frames else 0.0
        print(f"  ball-only frames:    {ball_only_frames} ({pct:.1f}%)")
    if ball_object_frames is not None:
        pct = (ball_object_frames * 100.0 / frames) if frames else 0.0
        print(f"  ball-object frames:  {ball_object_frames} ({pct:.1f}%)")
    present_blockers = {k: v for k, v in ball_blockers.items() if v is not None and v}
    if present_blockers:
        print("  ball dirty blockers:")
        for name, value in sorted(present_blockers.items(), key=lambda item: item[1], reverse=True):
            print(f"    {name:8s} {value}")
    if vga_rects is not None:
        print(f"  VGA rects/frame:     {vga_rects / max(frames or 1, 1):.2f}")
    if vga_bytes is not None:
        print(f"  VGA bytes/frame:     {vga_bytes / max(frames or 1, 1):.0f}")

    if args.json_out is not None:
        frame_div = max(frames or 1, 1)
        summary = {
            "frames": frames,
            "total_pit_ticks": total,
            "buckets": {
                name: {
                    "ticks": value,
                    "pct": (value * 100.0 / total) if total else 0.0,
                    "ticks_per_frame": value / frame_div,
                }
                for name, value in present.items()
            },
            "static_rebuilds": static_rebuilds,
            "full_dynamic_frames": full_dynamic_frames,
            "full_dynamic_frame_pct": (
                (full_dynamic_frames * 100.0 / frames)
                if full_dynamic_frames is not None and frames
                else None
            ),
            "ball_only_frames": ball_only_frames,
            "ball_only_frame_pct": (
                (ball_only_frames * 100.0 / frames)
                if ball_only_frames is not None and frames
                else None
            ),
            "ball_object_frames": ball_object_frames,
            "ball_object_frame_pct": (
                (ball_object_frames * 100.0 / frames)
                if ball_object_frames is not None and frames
                else None
            ),
            "ball_dirty_blockers": ball_blockers,
            "vga_rect_flushes": vga_rects,
            "vga_bytes_written": vga_bytes,
            "vga_rects_per_frame": (vga_rects / frame_div) if vga_rects is not None else None,
            "vga_bytes_per_frame": (vga_bytes / frame_div) if vga_bytes is not None else None,
            "top_bucket": ordered[0][0],
        }
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
        print(f"  JSON summary:        {args.json_out}")

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
