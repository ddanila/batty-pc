#!/usr/bin/env python3
"""Port-side bat-deflection regression (LAB1F port in step_ball).

Seeds the DOS port with a coherent ball dropped just above the bat
(y=0x96) heading down at dir 0x0C, steps it to a post-contact frame, and
asserts the deflected direction matches the ground truth captured from the
original (ZEsarUX) by scripts/capture_bat_deflection.py. See
notes/bat-deflection.md for the decode and the captured table.

Because the port's ball motion is byte-exact, seeding the same descending
ball reaches the same contact pixel and so must produce the same outgoing
dir as the Spectrum. This is the end-to-end gate on the LAB1F port.

ZEsarUX-free (QEMU only). Exit 0 = all PASS.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = "build/batty-test.img"
# Normal bat at x=116 width 28; ball 8 wide, dropped at y=0x96 dir 0x0C.
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"

# start_x -> (probe_frame, expected_out_dir). probe_frame is a few frames
# past contact (deflection lands ~frame 6); the dir is stable afterward.
# Expected dirs are the captured Spectrum ground truth (incoming 0x0C):
#   contact offset -3->0x28, 5->0x2C, 13->0x34, 21->0x38, 29->0x38.
GROUND_TRUTH = {
    104: (8, 0x28),
    112: (8, 0x2C),
    120: (8, 0x34),
    128: (8, 0x38),
    136: (9, 0x38),
}


def ball_object_hex(start_x: int) -> str:
    b = bytes.fromhex("0200800096000C03020CEEF008078096020C0000008C")
    a = bytearray(b)
    a[2] = start_x       # x pixel
    a[14] = start_x      # prev-x (avoid stale-erase glitch)
    return a.hex().upper()


def dir_at(start_x: int, frame: int) -> int:
    Path(FLOPPY).unlink(missing_ok=True)
    probe = Path("build/PROBE_laffc.txt")
    probe.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_LAFFC=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={ball_object_hex(start_x)} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_bat"], stdout=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        return -1
    m = re.search(r"object_ball_1=([0-9A-Fa-f]+)", probe.read_text())
    if not m:
        return -1
    return bytes.fromhex(m.group(1))[6]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--xs", default=",".join(str(x) for x in GROUND_TRUTH),
                    help="subset of start_x values to verify")
    args = ap.parse_args()
    xs = [int(t) for t in args.xs.split(",") if t.strip()]

    fails = 0
    for sx in xs:
        frame, exp = GROUND_TRUTH[sx]
        got = dir_at(sx, frame)
        ok = got == exp
        print(f"  start_x={sx:3d} frame={frame}: out_dir=0x{got:02X} "
              f"[{'PASS' if ok else f'FAIL exp 0x{exp:02X}'}]")
        if not ok:
            fails += 1

    if fails == 0:
        print(f"PASS bat_deflection: port LAB1F matches Spectrum ground "
              f"truth at {len(xs)} bat positions")
    else:
        print(f"FAIL bat_deflection: {fails}/{len(xs)} positions diverged")
    return fails


if __name__ == "__main__":
    sys.exit(main())
