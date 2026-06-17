#!/usr/bin/env python3
"""Byte-exact ball-vs-brick parity on a NON-L3 level (broadens the oracle
beyond L3; locks the known-bugs #6 fix against the real game).

The L3 frame-step gate (test-laffc-ball-frame1) only verifies dynamic ball
parity on L3 — and #6 (the inverted LAFFC boundary-face mask) lived on the
edge-metal levels L5/L7, outside that coverage. This gate seeds the exact #6
scenario on L5 — a ball just above the row-0, col-0 metal brick (x=0x0C,
y=0x18) heading straight DOWN (dir=0x10) at speed 6 — and asserts the port's
trajectory equals the ORIGINAL's, captured from ZEsarUX via
replays/l5-metal-ball.json (poke $B7EA=4 -> $BA24 level-init loads L5 ->
$BA83 -> poke the ball at $9AD0; --probe-ball 0x9AD0 over frames 1..6):

    orig L5:  f1 x=12 y=25 dir=0x30   (bounces UP off the metal brick:
              y snapped to 32-7=25, dir 0x10->0x30)
              f3 x=12 y=14 dir=0x30   (travelling up)
              f5 x=12 y=8  dir=0x10   (bounced off the top wall)

With the pre-fix inverted mask the ball fell straight through (dir stayed
0x10). ZEsarUX-free (the original values are baked from the one-time
capture); same seed -> same outcome because the port motion+collision is
byte-exact. See notes/laffc-decode.md Update 28 + known-bugs.md #6.

    make test-laffc-ball-l5-metal
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, boot_until_gameplay

FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img")
OUT = Path("build/test_laffc_ball_l5_metal")
TAIL = "020CEEF008076C4E020C0000008C"
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BOOT_WAIT = os.environ.get("BATTY_BOOT_WAIT", "8")

# Ball: x=0x0C inside col-0 cell, y=0x18 above the row-0 brick, dir=0x10
# (down), speed 6 — the #6 scenario.
BALL = f"02000C0018001006{TAIL}"
# (frame, expected (x, y, dir)) — the ORIGINAL L5 trajectory.
CASES = [(1, (12, 25, 0x30)), (3, (12, 14, 0x30)), (5, (12, 8, 0x10))]


def probe(frame: int):
    Path(FLOPPY).unlink(missing_ok=True)
    env = (f"BATTY_LEVEL=5 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
           f"BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
           f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
           f"BATTY_REPLAY_BALL_OBJECT={BALL} BATTY_FRAME_PROBE={frame}")
    subprocess.run(f"BATTY_TEST_FLOPPY={FLOPPY} {env} make {FLOPPY}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    def drive():
        subprocess.run(["mdel", "-i", str(FLOPPY), "::PROBE.TXT"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        run_qemu(Path(FLOPPY), [f"SLEEP {BOOT_WAIT}", "sendkey ret",
                                f"SLEEP {1.0 + frame * 0.05}"], OUT / "q.log")
    p = boot_until_gameplay(Path(FLOPPY), drive, label=f"L5 metal f{frame}")
    b = bytes.fromhex(p.get("object_ball_1", ""))
    return (b[2], b[4], b[6]) if len(b) == 22 else None


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    fails = 0
    for frame, exp in CASES:
        got = probe(frame)
        ok = got == exp
        gs = (f"x={got[0]} y={got[1]} dir=0x{got[2]:02X}" if got else "no probe")
        print(f"  f{frame}: {gs}  [{'PASS' if ok else 'FAIL'}] "
              f"(expect x={exp[0]} y={exp[1]} dir=0x{exp[2]:02X})")
        if not ok:
            fails += 1
    if fails == 0:
        print("PASS laffc_ball_l5_metal: port L5 boundary-metal bounce is "
              "byte-exact vs the original (oracle-confirmed #6 fix beyond L3)")
        return 0
    print(f"FAIL laffc_ball_l5_metal: {fails}/{len(CASES)} frames diverged")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
