#!/usr/bin/env python3
"""Each ball carries its OWN owner bit, so bricks pay the right player.

`LAB1F_0` writes the owner on the ball being handled —

    RES 7,(IX+$12) / BIT 7,(IY+$02) / JR Z / SET 7,(IX+$12)

— and `handling_ball` runs once per ball object, so every ball has one.
`+$12`'s low bits are a counter and every operation on them preserves
bit 7 on purpose (see notes/double-play.md), which is what makes it
persistent state rather than a derived value.

The port kept ONE bit and spent it on the primary. Brick points from a
secondary were therefore credited to whoever last deflected the PRIMARY
— a ball the player may not have touched for seconds, possibly on the
other side of the court.

### The scenario

Mode 2, extras spawned at entry just above bat 1, whose different
derived directions spread them out. Ball 2 reaches bat 1 first; the
primary and ball 3 drift past it.

  frame  2   all three owners EQUAL — the entry side, before any contact
  frame 12   slot 1 differs from slots 0 and 2, which are unchanged

The second half is the point, and it is the whole difference between a
per-ball owner and a shared one. Bat 1 touched exactly one ball, so
exactly one bit may move.

The values are asserted as a RELATIONSHIP, not as literals. The entry
side alternates every level entry (`ball_start_right`, `XOR $88` in the
original), so "all three start at 0" would have been a fact about which
entry this happens to be — the first draft asserted (0,0,0) and failed
against a run that legitimately started on the right.

### Why the owner and not the score

Scoring a brick to the right player needs a brick broken by the
secondary at a known moment, which is a much longer and more fragile
seed than reading the bit that decides it. `check_two_player_state`
already pins that the brick site passes `ball_owner_side[slot]` and
nothing else, so the bit is the whole of what remained unverified.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-x2own.img")

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"


def ball_seed() -> str:
    b = bytearray(bytes.fromhex(
        "02006C004E001F03020CEEF008076C4E020C0000008C"))
    b[2] = 0x78      # x = 120, just inside bat 1's body (116..144)
    b[4] = 0xA0      # y = 160, a short drop to the bat top at 173
    b[6] = 0x08      # down-right
    return b.hex().upper()


def probe(frame: int):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_x2own.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE=2 BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=0 BATTY_SUPPRESS_NO_BALL_DEATH=1 "
        f"BATTY_REPLAY_MULTIBALL=1 BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed()} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_x2own"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    m = re.search(r"ball_owners=([0-9A-Fa-f]{6})", out.read_text())
    if not m:
        raise SystemExit("FAIL: PROBE.TXT has no ball_owners row — if the "
                         "probe field was renamed, point this gate at it")
    h = m.group(1)
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def main() -> int:
    ok = True

    before = probe(2)
    if before is None:
        print("  frame 2: NO PROBE.TXT [FAIL]")
        return 1
    good = before[0] == before[1] == before[2]
    ok = ok and good
    entry = before[0]
    print(f"  frame  2: owners={before} [{'PASS' if good else 'FAIL'}] "
          f"(expect all three EQUAL — the entry side, whichever it is "
          f"this time, before any bat contact)")

    after = probe(12)
    if after is None:
        print("  frame 12: NO PROBE.TXT [FAIL]")
        return 1
    good = (after[1] != entry and after[0] == entry and after[2] == entry)
    ok = ok and good
    print(f"  frame 12: owners={after} [{'PASS' if good else 'FAIL'}] "
          f"(expect slot 1 flipped away from {entry} — bat 1 re-owned it — "
          f"and slots 0 and 2 untouched, because bat 1 hit neither)")

    if ok:
        print("PASS extra_ball_owner: the owner bit is per ball — a bat "
              "deflection moves only the ball it hit")
        return 0
    print("FAIL extra_ball_owner")
    return 1


if __name__ == "__main__":
    sys.exit(main())
