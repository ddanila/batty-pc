#!/usr/bin/env python3
"""Capture the original's bat-deflection ground truth (LAB1F @ $AB1F).

The L3 byte-exact run never contacts the bat (the ball orbits the upper
field; see notes/bat-deflection.md). To get a ball-onto-bat reference
WITHOUT a fresh snapshot, this reuses the proven-coherent `l3-brick-flash`
ball descriptor and only *repositions* the ball: drop it just above the
bat (y=0x96) heading down, then let the original compute the deflection
itself. Repositioning a coherent object stays coherent (unlike poking a
placeholder ball in the raw snapshot, which hangs — see parity-status.md).

For each starting x it frame-steps across the bat contact and reports the
contact position + the outgoing direction the original chose. That table
is the ground truth a LAB1F port must reproduce (the deflection cannot be
reliably hand-derived; the bit2 zones double-reflect — see the notes).

Usage:
  python3 scripts/capture_bat_deflection.py            # default sweep, dir 0x0C
  python3 scripts/capture_bat_deflection.py --dir 0x14 # other incoming dir
  python3 scripts/capture_bat_deflection.py --xs 104,116,128,140
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE_REPLAY = ROOT / "replays" / "l3-brick-flash.json"
BAT_X = 116          # object_bat_1 x ($9B54+2); width 28 -> spans [116,144]
BALL_ADDR = 0x9AD0


def build_spec(start_x: int, indir: int, y: int = 0x96) -> dict:
    spec = json.loads(BASE_REPLAY.read_text())
    for step in spec["original"]["setup"]:
        if step.get("op") == "write_memory" and step.get("address") == "0x9AD0":
            b = [int(x, 16) for x in step["bytes"]]
            b[2] = start_x      # x pixel
            b[4] = y            # y pixel (just above bat at 173)
            b[6] = indir        # direction (downward; in the LAB1F match set)
            b[14] = start_x     # prev-x == x (avoid stale-erase glitch)
            b[15] = y           # prev-y == y
            step["bytes"] = [f"0x{v:02X}" for v in b]
    return spec


def run_one(start_x: int, indir: int, frames: int, tmp: Path) -> tuple:
    tmp.write_text(json.dumps(build_spec(start_x, indir)))
    frame_list = ",".join(str(i) for i in range(0, frames))
    p = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "capture_frame_timeline_original.py"),
         "--setup-from-replay", str(tmp), "--frame-pc", "0x0038",
         "--probe-ball", hex(BALL_ADDR), "--frames", frame_list],
        capture_output=True, text=True, timeout=240)
    seq = re.findall(
        r"ball@frame(\d+): x=(\d+) xf=\d+ y=(\d+) \S+ dir=0x([0-9A-F]+)", p.stdout)
    contact_x = contact_y = out = None
    for _fr, x, y, d in seq:
        dv = int(d, 16)
        if dv != indir:
            out = dv
            break
        contact_x, contact_y = int(x), int(y)
    return contact_x, contact_y, out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", type=lambda s: int(s, 0), default=0x0C,
                    help="incoming (downward) direction; LAB1F matches "
                         "0x04/08/0C/14/18/1C")
    ap.add_argument("--xs", default="104,112,120,128,136,144",
                    help="comma-separated starting ball x values")
    ap.add_argument("--frames", type=int, default=12)
    args = ap.parse_args()

    xs = [int(t) for t in args.xs.split(",") if t.strip()]
    tmp = Path("/tmp/bat_deflection_sweep.json")
    print(f"incoming dir = 0x{args.dir:02X}; bat x={BAT_X} width 28 "
          f"(offset = ball_x + 3 - {BAT_X})")
    rows = []
    for sx in xs:
        cx, cy, out = run_one(sx, args.dir, args.frames, tmp)
        off = (cx + 3 - BAT_X) if cx is not None else None
        rows.append((sx, cx, cy, off, out))
        out_s = f"0x{out:02X}" if out is not None else "NO-FLIP (ball lost)"
        print(f"  start_x={sx:3d}  contact x={cx} y={cy}  offset={off}  "
              f"-> out_dir={out_s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
