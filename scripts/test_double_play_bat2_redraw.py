#!/usr/bin/env python3
"""Bat 2's PICTURE follows bat 2's object.

`test-double-play-input` proved bat 2 steers by reading `object_bat_2`
out of the probe. It was right and it was not enough: the object moved
the whole way while the SPRITE stayed where the level-entry compose had
put it.

Bat 2 was drawn in exactly one place — `compose_level_scene`, which runs
at level entry. Nothing in the per-frame redraw paths knew about it, and
`bat_moved` was `BAT_X != BAT_PREV_X`, bat 1's x alone, so a frame in
which only player 2 moved took the early-out in `redraw_frame` and drew
nothing.

Measured before the fix: object at $DC, sprite still at $B4.

### The scenario

Two captures of the same Double Play level, ball held on the bat so
nothing else moves:

  still  no keys      -> bat 2 sits on its $B0 start
  right  K held       -> bat 2 drives to the right wall at $DC

Their pixel difference must span BOTH footprints — the one bat 2
vacated ($B0..$B0+31) and the one it now occupies ($DC..$DC+31). The
run also reads `object_bat_2` from its own probe, so the expected right
edge is derived from where the object actually got to rather than
hardcoded.

Before the fix the difference stopped at 211: one 4 px step, which is
what a single frame of movement before the level-entry compose looks
like. That is the failure this gate is shaped to catch — not "bat 2
never moved" but "bat 2 moved a little and then froze", which is what a
stale sprite actually looks like and is easy to glance past.

### Why a pixel gate and not a probe row

Because the probe row was already green. Everything about bat 2's state
was correct; only the drawing was wrong, and no amount of reading state
can see that.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from test_visual import PALETTE_RGB, ppm_inner_to_indices, run_qemu  # noqa: E402

BAT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
SC_K = "25"
BAT2_START = 0xB0
BAT_SPRITE_W = 32


def capture(label: str, hold: str | None):
    img = ROOT / f"build/batty-b2redraw-{label}.img"
    img.unlink(missing_ok=True)
    env = os.environ.copy()
    env.update({
        "BATTY_TEST_FLOPPY": str(img), "BATTY_START_LEVEL": "1",
        "BATTY_LEVEL": "1", "BATTY_GAME_MODE": "2", "BATTY_NOSOUND": "1",
        "BATTY_REPLAY_WAIT_KEY": "1", "BATTY_REPLAY_BALL_STUCK": "1",
        "BATTY_REPLAY_BAT_OBJECT": BAT,
        "BATTY_SUPPRESS_NO_BALL_DEATH": "1", "BATTY_REPLAY_PROBE": "1",
        "BATTY_VISUAL_PROBE_FRAMES": "100",
    })
    if hold:
        env["BATTY_HOLD_KEYS"] = hold
    subprocess.run(["make", str(img)], cwd=ROOT, env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   check=True)
    # The image is rebuilt from scratch above and so carries no probe,
    # but say so to the tooling as well: test-gate-freshness looks for an
    # mdel or a FLOPPY-named unlink, and `img` is neither. Belt and
    # braces on a gate whose whole job is to not grade a stale artifact.
    subprocess.run(["mdel", "-i", str(img), "::PROBE.TXT"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   check=False)
    ppm = ROOT / f"build/b2redraw_{label}.ppm"
    run_qemu(img, ["SLEEP 9.0", "sendkey ret", "SLEEP 3.0",
                   f"screendump {ppm}", "sendkey esc", "SLEEP 0.5"],
             ROOT / f"build/b2redraw_{label}.log")
    probe = ROOT / f"build/PROBE_b2redraw_{label}.txt"
    # Delete first: an mcopy that fails leaves the PREVIOUS run's probe
    # in place, and this gate would then derive its expected right edge
    # from a bat position that never happened. test-gate-freshness
    # catches exactly this and caught it here.
    probe.unlink(missing_ok=True)
    subprocess.run(["mcopy", "-n", "-o", "-i", str(img), "::PROBE.TXT",
                    str(probe)], cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    obj_x = None
    if probe.exists():
        m = re.search(r"object_bat_2=([0-9A-Fa-f]+)", probe.read_text())
        if m:
            obj_x = bytes.fromhex(m.group(1))[2]
    return ppm_inner_to_indices(ppm), obj_x


def main() -> int:
    still, _ = capture("still", None)
    right, obj_x = capture("right", SC_K)

    if obj_x is None:
        print("FAIL: no object_bat_2 in the probe — cannot derive where the "
              "bat actually went")
        return 1
    if obj_x <= BAT2_START:
        print(f"FAIL: bat 2's OBJECT did not move (x=${obj_x:02X}); this gate "
              f"is about the sprite, so fix the steering first")
        return 1

    pts = [(x, y) for y in range(192) for x in range(256)
           if PALETTE_RGB[still[y * 256 + x]] != PALETTE_RGB[right[y * 256 + x]]]
    if not pts:
        print("FAIL: the two captures are identical — bat 2 was not drawn at "
              "either position")
        return 1

    xs = [p[0] for p in pts]
    got = (min(xs), max(xs))
    want = (BAT2_START, obj_x + BAT_SPRITE_W - 1)
    ok = got == want
    print(f"  object bat 2 at ${obj_x:02X}; pixel difference spans "
          f"x {got[0]}..{got[1]} [{'PASS' if ok else 'FAIL'}] (expect "
          f"{want[0]}..{want[1]} — the footprint it left plus the one it "
          f"now covers)")

    if not ok:
        print("FAIL double_play_bat2_redraw: the drawn bat does not agree "
              "with the object. A span that stops a few px past the start "
              "is the stale-sprite signature.")
        return 1
    print("PASS double_play_bat2_redraw: bat 2's sprite tracks its object")
    return 0


if __name__ == "__main__":
    sys.exit(main())
