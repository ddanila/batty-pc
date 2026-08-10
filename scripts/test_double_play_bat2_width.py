#!/usr/bin/env python3
"""Bat 2 grows when IT catches BIG_BAT, and bat 1 does not.

The last item in WS3. `bat.extra_px` / `extra_target` / `big_ticks` were
one bat's worth of state, so a BIG_BAT caught by bat 2 first widened
BAT 1 (wrong bat), then — once the bonus BYTE split — widened nobody
(missing bat). `bats[2]` and a generalised renderer ended both.

The original keeps them apart with `bonus_flag_swap` around every bat-2
call, which presupposes exactly this: two copies of everything a bonus
touches.

### The scenario

Two captures of the same Double Play level, both with nothing moving:

  plain  no bonus                  -> bat 2 is the 32 px sprite
  big    BIG_BAT dropped on bat 2  -> bat 2 grows 8 px each side

Their pixel difference is read on the BAT ROW, and must be a pair of
side lobes — 8 px on the left of bat 2 and 8 on the right — with
nothing outside bat 2's band. Bat 1 must not appear in the diff at all,
which is the half that fails if the width goes to the wrong bat.

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
# On bat 2, and already overlapping it so the catch lands on frame 1.
BONUS_ON_BAT2 = "2,186,168"


def capture(label: str, bonus: str | None):
    img = ROOT / f"build/batty-b2width-{label}.img"
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
    ppm = ROOT / f"build/b2width_{label}.ppm"
    run_qemu(img, ["SLEEP 9.0", "sendkey ret", "SLEEP 3.0",
                   f"screendump {ppm}", "sendkey esc", "SLEEP 0.5"],
             ROOT / f"build/b2width_{label}.log")
    probe = ROOT / f"build/PROBE_b2width_{label}.txt"
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
    big, obj_x_big = capture("big", BONUS_ON_BAT2)

    if obj_x is None or obj_x_big is None:
        print("FAIL: no object_bat_2 in the probe")
        return 1
    if obj_x != obj_x_big:
        print(f"FAIL: bat 2 moved between the two runs (${obj_x:02X} vs "
              f"${obj_x_big:02X}); the diff would not be about width")
        return 1

    row = BAT_ROW * 256
    diff = [x for x in range(256)
            if PALETTE_RGB[plain[row + x]] != PALETTE_RGB[big[row + x]]]
    if not diff:
        print("FAIL: the bat row is identical — bat 2 did not grow at all")
        return 1

    # The property, not a guessed pixel set:
    #
    #   * every change lies inside bat 2's WIDENED footprint, and
    #   * both side lobes contain at least one change.
    #
    # Not equality with the lobes: at extra_px >= 8 the renderer swaps to
    # SPR_BAT_BIG, a 48 px sprite with its own interior texture, so the
    # BODY legitimately differs from the 32 px one too. And not "all 16
    # lobe pixels": the bat is textured, so a lobe column can match the
    # background it replaced. Two earlier drafts asserted each of those
    # and failed against a correct build.
    footprint = set(range(obj_x - BIG_EXTRA,
                          obj_x + BAT_SPRITE_W + BIG_EXTRA))
    left_lobe = set(range(obj_x - BIG_EXTRA, obj_x))
    right_lobe = set(range(obj_x + BAT_SPRITE_W,
                           obj_x + BAT_SPRITE_W + BIG_EXTRA))
    got = set(diff)
    ok = (got <= footprint
          and bool(got & left_lobe) and bool(got & right_lobe))
    print(f"  bat 2 at ${obj_x:02X}; bat-row pixels that changed: "
          f"{min(diff)}..{max(diff)} ({len(diff)} px) "
          f"[{'PASS' if ok else 'FAIL'}] (expect all of them inside the "
          f"widened footprint {obj_x - BIG_EXTRA}.."
          f"{obj_x + BAT_SPRITE_W + BIG_EXTRA - 1}, with at least one in "
          f"each {BIG_EXTRA} px side lobe — a bat that grew only one way "
          f"is a clamp bug, not a width bug)")

    bat1_side = [x for x in diff if x < 0x80]
    if bat1_side:
        print(f"FAIL: bat 1's half of the court changed too "
              f"(cols {bat1_side[:8]}) — the width went to the wrong bat")
        return 1

    if not ok:
        print("FAIL double_play_bat2_width")
        return 1
    print("PASS double_play_bat2_width: bat 2 grows on its own catch, and "
          "bat 1 is untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
