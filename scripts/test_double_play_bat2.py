#!/usr/bin/env python3
"""In Double Play the ball bounces off EITHER bat, and changes hands.

`LAB1F` tries bat 1 first and falls through to bat 2 only in mode $02:

    LD IY,object_bat_1 / CALL obj_compare / JR C,LAB1F_0
    LD A,(game_mode) / CP $02 / RET NZ
    LD IY,object_bat_2 / CALL obj_compare / RET NC
    LAB1F_0:
      RES 7,(IX+$12)      ; the ball's owner bit
      BIT 7,(IY+$02)      ; the HITTING bat's x
      JR Z,LAB1F_1
      SET 7,(IX+$12)

so the deflection and the re-ownership are the same event. Brick points
follow the owner (notes/double-play.md), which is why this gate checks
both.

### The scenario

A ball seeded at (184, 160) heading down-right, straight at bat 2's
resting place (x=$B0=176, y=$AD=173, 32 px wide).

  subject  BATTY_GAME_MODE=2 -> bat 2 exists: the ball deflects UP and
           the owner becomes 2UP
  control  BATTY_GAME_MODE=0 -> no bat 2: the ball falls straight past

Same seed, same frame. The only difference is the mode.

### Why the approach is diagonal

The first attempt seeded `dir=$10`, straight down, and the ball STUCK:
it snapped to the bat top every frame and never left, because `$10` is
not one of the directions `LAB1F_11` searches for
({$04,$08,$0C,$14,$18,$1C}) and the lookup returns the input unchanged.
Real trajectories are diagonal, so the original never presents that
input — but a seeded gate can, and a seed outside a table's domain
measures the table's edge rather than the code under test.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-bat2.img")
FRAME = 12

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"


def ball_seed() -> str:
    b = bytearray(bytes.fromhex(
        "02006C004E001F03020CEEF008076C4E020C0000008C"))
    b[2] = 0xB8      # x = 184, over bat 2's body (176..207)
    b[4] = 0xA0      # y = 160, above the bat top
    b[6] = 0x08      # down-right; see the docstring on why not $10
    return b.hex().upper()


def probe(mode: str):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_bat2.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
        f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"BATTY_REPLAY_BALL_OBJECT={ball_seed()} "
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_bat2"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    text = out.read_text()
    m = re.search(r"object_ball_1=([0-9A-Fa-f]+)", text)
    o = re.search(r"scores=\d+_\d+_own([0-9A-Fa-f]{2})", text)
    if not m or not o:
        raise SystemExit("FAIL: PROBE.TXT has no ball or no owner field")
    b = bytes.fromhex(m.group(1))
    return b[4], b[6], int(o.group(1), 16)      # y, dir, owner


def main() -> int:
    ok = True

    got = probe("2")
    if got is None:
        print("  mode 2: NO PROBE.TXT [FAIL]")
        return 1
    y, d, own = got
    good = (y < 160 and d == 0x38 and own == 1)
    ok = ok and good
    print(f"  mode 2 (Double Play): y={y} dir=0x{d:02X} owner={own} "
          f"[{'PASS' if good else 'FAIL'}] (expect y<160 rising, dir=0x38, "
          f"owner=1 — bat 2 deflected it and took it)")

    got = probe("0")
    if got is None:
        print("  mode 0: NO PROBE.TXT [FAIL]")
        return 1
    y, d, own = got
    good = (y > 170 and d == 0x08 and own == 0)
    ok = ok and good
    print(f"  mode 0 (1 Player):    y={y} dir=0x{d:02X} owner={own} "
          f"[{'PASS' if good else 'FAIL'}] (expect y>170 still falling, "
          f"dir unchanged 0x08, owner=0 — there is no bat 2)")

    if ok:
        print("PASS double_play_bat2: bat 2 deflects the ball and takes "
              "ownership; nothing of the sort happens in 1 Player")
        return 0
    print("FAIL double_play_bat2")
    return 1


if __name__ == "__main__":
    sys.exit(main())
