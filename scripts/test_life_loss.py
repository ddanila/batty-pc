#!/usr/bin/env python3
"""Losing a life removes exactly one life indicator.

notes/parity-gaps.md lists the game-FLOW transitions as not end-to-end
gated. Game-over and hi-score got visual gates recently; this is
life-loss, the one that happens mid-level and leaves the game running.

It became cheap once BATTY_REPLAY_LIVES existed. The scenario is an A/B
on a single knob:

  both runs   BATTY_REPLAY_LIVES=3 + BATTY_HIDE_BALL=1, so
              handle_no_ball_death is reached on the FIRST frame
  control     BATTY_SUPPRESS_NO_BALL_DEATH=1 — the death is suppressed,
              lives stays 3
  subject     no suppression — the death runs, lives becomes 2

Same level, same seed, same frame; the only difference is whether the
death fired. So a difference in the indicator strip IS the life loss,
with nothing else to attribute it to.

The indicator draws `lives - 1` icons of 16 px from x=8 at y=185
(LIVES_X_PX / LIVES_Y_PX, the original's $B9). Three lives is two icons,
two lives is one.

A NOTE ON THE MEASUREMENT, which took two attempts.

"Lit pixels" does not work: the playfield background is not colour 0, so
every pixel in the strip is non-zero in both runs. Same lesson as
test-game-over-visual.

"Any pixel that is not the background colour" does not work either. The
background carries a vertical line every 16 px, so EVERY cell contains
non-background pixels and the first version counted four icons where
there are two. What works is DENSITY. Measured over the 16x6 cell:

    icons        74 and 46 non-background pixels
    empty cells   6, every time — one grid column, six rows

so the threshold below sits in a gap an order of magnitude wide. (Cell 0
reads higher than cell 1 because it overlaps the frame border at x=8.)
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
OUT = Path("build/test_life_loss")

LIVES_X, LIVES_Y, ICON_W, ICON_H = 8, 185, 16, 6
BG = 14          # the playfield background index in the indicator band
MAX_CELLS = 4    # far more than any scenario here needs
# Measured: icons 74 / 46 non-BG pixels, empty cells exactly 6. See the
# docstring — the background's 16 px grid line is why this is a density
# test and not a presence test.
ICON_MIN_PIXELS = 20


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    for needle, why in (
        ("#define LIVES_X_PX    8", "the indicator's x origin this gate reads"),
        ("#define LIVES_Y_PX    0xB9", "the indicator's y origin"),
        ("int show = player.lives - 1;",
         "the lives-to-icons rule; if it changes, the counts below change"),
    ):
        if needle not in src:
            raise SystemExit(f"FAIL: `{needle}` is gone — {why}")


def build_floppy(suppress: bool) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_LIVES": "3",
        "BATTY_HIDE_BALL": "1",
        "BATTY_NOSOUND": "1",
    })
    if suppress:
        env["BATTY_SUPPRESS_NO_BALL_DEATH"] = "1"
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(tag: str):
    OUT.mkdir(parents=True, exist_ok=True)
    ppm = OUT / f"{tag}.ppm"
    run_qemu(TEST_FLOPPY, [f"SLEEP 9.0", f"screendump {ppm}"],
             OUT / f"{tag}.log")
    return ppm_inner_to_indices(ppm)


def count_icons(idx) -> int:
    """Cells from x=8 that hold something other than background."""
    n = 0
    for cell in range(MAX_CELLS):
        x0 = LIVES_X + cell * ICON_W
        density = sum(1 for y in range(LIVES_Y, LIVES_Y + ICON_H)
                      for x in range(x0, x0 + ICON_W)
                      if idx[y * 256 + x] != BG)
        if density < ICON_MIN_PIXELS:
            break        # icons are contiguous from the left
        n += 1
    return n


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()

    results = {}
    for tag, suppress, want in (("suppressed", True, 2), ("died", False, 1)):
        build_floppy(suppress)
        idx = capture(tag)
        if len(idx) != 256 * 192:
            raise SystemExit(f"FAIL[{tag}]: captured {len(idx)} pixels — the "
                             f"port never left text mode")
        got = count_icons(idx)
        results[tag] = got
        if got != want:
            raise SystemExit(
                f"FAIL[{tag}]: {got} life icon(s), expected {want}. With "
                f"BATTY_REPLAY_LIVES=3 the indicator shows lives-1 icons, so "
                f"{want} means lives == {want + 1}. "
                + ("The death was suppressed, so no life should have been "
                   "lost." if suppress else
                   "BATTY_HIDE_BALL should reach handle_no_ball_death on the "
                   "first frame and cost exactly one life."))

    if results["suppressed"] - results["died"] != 1:
        raise SystemExit(
            f"FAIL: the death changed the indicator by "
            f"{results['suppressed'] - results['died']} icons, expected "
            f"exactly 1")

    print(f"PASS life_loss: {results['suppressed']} icons without the death, "
          f"{results['died']} with it — exactly one life lost")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
