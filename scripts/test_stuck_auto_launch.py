#!/usr/bin/env python3
"""A ball left on the bat launches itself after STUCK_TIMEOUT ticks.

    if (ball.stuck_ticks[0] >= STUCK_TIMEOUT) {   /* 192 */
        ball.stuck[0] = 0;
        sound_queue(SND_BALL_START);
        primary_ball_launch_from_bat();

Nothing gated this. Mutating the counter to increment a different ball's
slot — so `[0]` never reaches the timeout and the ball sits on the bat
forever — survived `make test`, `test-normal-ball-launch` and the whole
suite. `STUCK_TIMEOUT` appeared in exactly one place outside the header:
a prose mention in another gate's docstring.

It went unnoticed because the timeout is ~3.8 s of a ball doing nothing.
Screendump gates capture level entry, where the ball IS meant to be
stuck, and the replay scenarios all launch it deliberately.

### The bracket, measured

Probing `object_ball_1` at a checkpoint, with the ball seeded stuck:

    frame 190   x=132 y=166 dir=$1B     resting
    frame 194   x=133 y=162 dir=$34     launched
    frame 198   x=136 y=155 dir=$34     climbing

y=166 is the rest height (`BAT_Y - BALL_H_PX`). So the launch happens
between 190 and 194, which is where a 192-tick timeout puts it — and the
two frames either side pin the constant, not just its existence. Moving
`STUCK_TIMEOUT` by more than a couple of ticks breaks one end or the
other.

The direction changing from the seeded `$1B` to `$34` is the second
half: `primary_ball_launch_from_bat` derives the angle from where the
ball rests on the bat, so a launch that fired without it would show up
here as an unchanged dir.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-auto.img")

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
REST_Y = 166          # BAT_Y - BALL_H_PX, where a held ball sits
SEED_DIR = 0x1B


def probe(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_auto.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_LEVEL=3 BATTY_START_LEVEL=1 "
        f"BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_COUNTER=0 BATTY_REPLAY_BALL_STUCK=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_auto"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"object_ball_1=([0-9A-Fa-f]+)", out.read_text())
    if not m:
        return None
    b = bytes.fromhex(m.group(1))
    return b[4], b[6]           # y, dir


def main() -> int:
    ok = True

    got = probe(190)
    if got is None:
        print("  frame 190: NO PROBE.TXT [FAIL]")
        return 1
    y, d = got
    good = (y == REST_Y and d == SEED_DIR)
    ok = ok and good
    print(f"  frame 190: y={y} dir=0x{d:02X} [{'PASS' if good else 'FAIL'}] "
          f"(expect y={REST_Y} dir=0x{SEED_DIR:02X} — still held, "
          f"STUCK_TIMEOUT is 192)")

    got = probe(194)
    if got is None:
        print("  frame 194: NO PROBE.TXT [FAIL]")
        return 1
    y, d = got
    good = (y < REST_Y and d != SEED_DIR)
    ok = ok and good
    print(f"  frame 194: y={y} dir=0x{d:02X} [{'PASS' if good else 'FAIL'}] "
          f"(expect y<{REST_Y} and dir changed — launched, with the angle "
          f"derived from the rest offset)")

    if ok:
        print("PASS stuck_auto_launch: held at 190, launched by 194")
        return 0
    print("FAIL stuck_auto_launch")
    return 1


if __name__ == "__main__":
    sys.exit(main())
