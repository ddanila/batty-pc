#!/usr/bin/env python3
"""In Double Play a SECONDARY ball meets bat 2 as well as bat 1.

`LAB1F` tries bat 1, and in mode $02 falls through to bat 2 when bat 1
did not overlap. It runs once per BALL object, so this applies to the
extras exactly as it does to the primary. The port's `step_extra_ball`
tested bat 1 only — it read `eff_bat_left`/`eff_bat_right` and nothing
else — so an extra fell straight through bat 2 and was lost.

That was the gap left open, and named, when the secondary CATCH landed.

### The scenario

Mode 2, extras spawned at entry with `BATTY_REPLAY_MULTIBALL`, the
primary seeded over bat 2's half and heading down so the extras are
thrown into the same half:

  subject  mode 2 -> bat 2 exists: ball 2 survives past the frame it
           would have crossed the bat row, and is RISING
  control  mode 0 -> no bat 2: it falls past and is deactivated
           (sprite_set bit 7)

### The three-outcome bug this gate would not have caught

The helper returns no-contact / deflected / caught, and the first draft
returned only "was it caught". A bat-1 DEFLECTION then read as "no
contact", so the ball was offered to bat 2 and could be handled twice in
one frame — deflected off bat 1, then again off bat 2.

It is not gated here, and saying so is the point: bat 1 and bat 2 are
28 px apart at their closest and both deflections would have to land in
the same frame, which no seeded trajectory here produces. It was caught
by reading the control flow, not by a test, and the comment in
`extra_ball_meets_bat` is what carries it.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-x2bat2.img")
FRAME = 26

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"


def ball_seed() -> str:
    b = bytearray(bytes.fromhex(
        "02006C004E001F03020CEEF008076C4E020C0000008C"))
    b[2] = 0xBC      # x = 188, over bat 2's body (176..204)
    b[4] = 0x90      # y = 144
    b[6] = 0x08      # down-right
    return b.hex().upper()


def probe(mode: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_x2bat2.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=0 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_MULTIBALL=1 BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed()} "
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_x2bat2"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"object_ball_2=([0-9A-Fa-f]+)", out.read_text())
    if not m:
        raise SystemExit("FAIL: PROBE.TXT has no object_ball_2 row")
    b = bytes.fromhex(m.group(1))
    return b[0], b[2], b[4]     # sprite_set, x, y


def main() -> int:
    ok = True

    got = probe("2")
    if got is None:
        print("  mode 2: NO PROBE.TXT [FAIL]")
        return 1
    sset, x, y = got
    alive = not (sset & 0x80)
    good = alive and y < 173
    ok = ok and good
    print(f"  mode 2 (Double Play): set=${sset:02X} x={x} y={y} "
          f"[{'PASS' if good else 'FAIL'}] (expect still active and above "
          f"the bat row — bat 2 sent it back)")

    got = probe("0")
    if got is None:
        print("  mode 0: NO PROBE.TXT [FAIL]")
        return 1
    sset, x, y = got
    good = (sset & 0x80) != 0
    ok = ok and good
    print(f"  mode 0 (1 Player):    set=${sset:02X} x={x} y={y} "
          f"[{'PASS' if good else 'FAIL'}] (expect deactivated — there is "
          f"no bat 2 and bat 1 at $74 is nowhere near it)")

    if ok:
        print("PASS extra_ball_bat2: a secondary ball is deflected by bat 2 "
              "in Double Play and lost without it")
        return 0
    print("FAIL extra_ball_bat2")
    return 1


if __name__ == "__main__":
    sys.exit(main())
