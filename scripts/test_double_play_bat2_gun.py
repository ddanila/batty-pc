#!/usr/bin/env python3
"""An armed bat 2 shows the GUN sprite, and bat 1 does not.

`bat_body_sprite` picks the laser-cannon body from the BAT's own
`bonus_applied`, and `render_bat_of` draws whichever bat it is handed.
Bat 2 could not reach either while the bonus byte was mirrored and its
renderer was a three-line copy that always drew SPR_BAT_NORMAL.

Both changed on 2026-08-10, and the note recording it said in as many
words: "Not gated: no scenario yet drops a LASER on bat 2." The very
next commit built that scenario for `test-double-play-bat2-laser`, so
the hole cost one capture to close.

### The scenario

Two captures of the same Double Play level, both with nothing moving:

  plain  no bonus                 -> bat 2 is the plain body
  armed  LASER dropped on bat 2   -> bat 2 is the gun-mounted body

FIRE is deliberately NOT held. The resting gun sprite is the whole
subject; pressing fire would add bullets and muzzle-flash frames to the
diff and make "did the body change" unanswerable.

Their pixel difference is read on the BAT ROW and must fall entirely
inside bat 2's 32 px footprint — same width as before, since LASER does
not resize. Nothing in bat 1's half may move, which is the assertion
that fails if the sprite is picked from the wrong bat's byte.

### Why a pixel gate

`bats[BAT_SLOT(OBJ_BAT_2)].extra_px` is not in the probe, and adding it
would gate the state rather than the picture. Bat 2 spent a whole day
with correct state and a sprite that never moved
(see the redraw gate, and notes/lessons.md) — for a visible feature the
pixels are the thing to assert.
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
BAT2_START = 0xB0
BAT_SPRITE_W = 32
BIG_EXTRA = 8               # BAT_BIG_EXTRA_PX, added on each side
BAT_ROW = 178               # inside the bat band (y 173..185)
# On bat 2, already overlapping so the catch lands on frame 1.
BONUS_ON_BAT2 = "8,186,168"   # LASER (port type 8), original code $01


def capture(label: str, bonus: str | None):
    img = ROOT / f"build/batty-b2gun-{label}.img"
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
    if bonus:
        env["BATTY_REPLAY_BONUS"] = bonus
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
    ppm = ROOT / f"build/b2gun_{label}.ppm"
    run_qemu(img, ["SLEEP 9.0", "sendkey ret", "SLEEP 3.0",
                   f"screendump {ppm}", "sendkey esc", "SLEEP 0.5"],
             ROOT / f"build/b2gun_{label}.log")
    probe = ROOT / f"build/PROBE_b2gun_{label}.txt"
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
    plain, obj_x = capture("plain", None)
    armed, obj_x_armed = capture("armed", BONUS_ON_BAT2)

    if obj_x is None or obj_x_armed is None:
        print("FAIL: no object_bat_2 in the probe")
        return 1
    if obj_x != obj_x_armed:
        print(f"FAIL: bat 2 moved between the runs (${obj_x:02X} vs "
              f"${obj_x_armed:02X}); the diff would not be about the sprite")
        return 1

    row = BAT_ROW * 256
    diff = [x for x in range(256)
            if PALETTE_RGB[plain[row + x]] != PALETTE_RGB[armed[row + x]]]
    if not diff:
        print("FAIL: the bat row is identical — an armed bat 2 is still "
              "drawn with the plain body")
        return 1

    footprint = set(range(obj_x, obj_x + BAT_SPRITE_W))
    got = set(diff)
    ok = got <= footprint
    print(f"  bat 2 at ${obj_x:02X}; bat-row pixels that changed: "
          f"{min(diff)}..{max(diff)} ({len(diff)} px) "
          f"[{'PASS' if ok else 'FAIL'}] (expect all of them inside bat 2's "
          f"unchanged {BAT_SPRITE_W} px footprint {obj_x}.."
          f"{obj_x + BAT_SPRITE_W - 1} — LASER swaps the body, it does not "
          f"resize)")

    bat1_side = [x for x in diff if x < 0x80]
    if bat1_side:
        print(f"FAIL: bat 1's half changed too (cols {bat1_side[:8]}) — the "
              f"gun sprite is being picked from the wrong bat's byte")
        return 1

    if not ok:
        print("FAIL double_play_bat2_gun")
        return 1
    print("PASS double_play_bat2_gun: an armed bat 2 draws the gun body, "
          "and bat 1 is untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
