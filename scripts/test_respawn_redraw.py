#!/usr/bin/env python3
"""What the screen looks like AFTER a death, when the life count did not
change.

The bat explosion paints sparks over the whole playfield and takes the
bat off screen. Nothing that comes back from respawn_primary_ball looks
changed to the per-frame dirty tests — the bat returns to BAT_X_INIT
with prev_x already matching, same y, same width, same bonus byte — so
bat_changed() is false and only a row of running dots gets flushed over
a bat that is not there. Magnets are worse: they repaint when they
toggle, so one stays missing until it happens to.

For months this was invisible because losing a life changes the life
counter, and refresh_static_background rebuilds the whole cache on
lives_dirty for the sake of the indicators. The repaint was riding on a
counter it has nothing to do with. The first build where a death did NOT
change the counter showed the bat in fragments — reported from play, not
from a gate, which is why this file exists.

The A/B is one knob:

  mortal    BATTY_REPLAY_LIVES=3, the death costs a life
  infinite  BATTY_INFINITE_LIVES=1, the death costs nothing

Same level, same seed, same frame, one death each. Everything outside
the life-indicator strip must match. Comparing the two — rather than
checking the infinite run against a golden image — means the gate states
the actual property: whether a life was lost must not change how the
playfield is drawn.

Level 2 rather than 1, because it carries a magnet (0x74, 0x2C) and the
magnet is the half of this that a bat-only check would miss.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import ppm_inner_to_indices, run_qemu, test_floppy

TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_respawn_redraw")

# The bat comes back at BAT_X_INIT ($74) on row BAT_Y_PX ($AD), 4 bytes
# wide; a little margin either side catches a fragment that spills.
BAT_RECT = (112, 173, 40, 13)
# Level 2's single magnet: magnets_per_level[1] = { 1, 0x74, 0x2C },
# spr_magnet_on is 4 bytes x 30 rows from that origin.
MAGNET_RECT = (116, 44, 32, 30)
# Enough ink that an all-background rect cannot pass by matching another
# all-background rect. Measured: the drawn bat is far above this.
MIN_INK = 60
SETTLE_S = 14.0


def source_guard() -> None:
    """The gate reads coordinates out of the source; if they move, its
    rectangles are aimed at empty playfield and it passes on nothing."""
    src = Path("src/main.cpp").read_text()
    for needle, why in (
        ("#define BAT_X_INIT  0x74", "the x this gate expects the bat back at"),
        ("#define BAT_Y_PX    0xAD", "the bat's row"),
        ("{ 1, 0x74,0x2C, 0,0, 0,0, 0,0 },                           /* L2  */",
         "level 2's magnet position, which MAGNET_RECT is aimed at"),
        ("invalidate_static_cache_after_death();",
         "the explicit post-death invalidation this gate exists to hold"),
    ):
        if needle not in src:
            raise SystemExit(f"FAIL: `{needle}` is gone — {why}")


def build(infinite: bool) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_LEVEL": "2",
        "BATTY_REPLAY_LIVES": "3",
        "BATTY_HIDE_BALL": "1",      # handle_no_ball_death on frame 1
        "BATTY_NOSOUND": "1",
    })
    if infinite:
        env["BATTY_INFINITE_LIVES"] = "1"
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(tag: str):
    out = OUT / tag
    out.mkdir(parents=True, exist_ok=True)
    ppm = out / "after_respawn.ppm"
    run_qemu(TEST_FLOPPY, [f"SLEEP {SETTLE_S}", f"screendump {ppm}"],
             out / "qemu.log")
    idx = ppm_inner_to_indices(ppm)
    if len(idx) != 256 * 192:
        raise SystemExit(f"FAIL[{tag}]: captured {len(idx)} pixels — the port "
                         f"never left text mode")
    return idx


def rect_bytes(idx, rect):
    x, y, w, h = rect
    return [bytes(idx[row * 256 + x:row * 256 + x + w])
            for row in range(y, y + h)]


def ink(idx, rect) -> int:
    """Pixels differing from the rect's most common value — a stand-in
    for "something is drawn here" that does not need to know the
    background index of the level."""
    data = b"".join(rect_bytes(idx, rect))
    if not data:
        return 0
    bg = max(set(data), key=data.count)
    return sum(1 for v in data if v != bg)


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()

    shots = {}
    for tag, infinite in (("mortal", False), ("infinite", True)):
        build(infinite)
        shots[tag] = capture(tag)

    bat_ink = ink(shots["mortal"], BAT_RECT)
    if bat_ink < MIN_INK:
        raise SystemExit(
            f"FAIL: the reference run's bat rect holds only {bat_ink} non-"
            f"background pixels (want >= {MIN_INK}). The scenario did not "
            f"reach a respawned bat, so comparing the two runs would only "
            f"prove that both are empty.")

    for name, rect in (("bat", BAT_RECT), ("magnet", MAGNET_RECT)):
        a = b"".join(rect_bytes(shots["mortal"], rect))
        b = b"".join(rect_bytes(shots["infinite"], rect))
        if a != b:
            differing = sum(1 for x, y in zip(a, b) if x != y)
            raise SystemExit(
                f"FAIL: the {name} is drawn differently depending on whether "
                f"the death cost a life — {differing} of {len(a)} pixels in "
                f"{rect} differ.\nA life loss must not decide whether the "
                f"playfield gets repainted. If this broke, the post-death "
                f"invalidation stopped happening and the redraw is riding on "
                f"lives_dirty again.")

    print(f"PASS respawn_redraw: bat ({bat_ink} px of ink) and magnet redraw "
          f"identically with and without a life lost")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
