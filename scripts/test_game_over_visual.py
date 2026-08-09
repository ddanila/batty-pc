#!/usr/bin/env python3
"""Capture the GAME OVER screen and check it actually rendered.

`test-game-over` guards the sequence's ORDERING in source and says in its
own docstring that a visual gate is still worth having. This is it.

The reason there wasn't one is that reaching game over meant losing three
lives, and the only way a gate could do that was three death animations
on wall-clock waits — the same shape that made test-bat-redraw-window
flaky (notes/testing.md). Two knobs remove the wall clock entirely:

  BATTY_REPLAY_LIVES=1  start with one life instead of three
  BATTY_HIDE_BALL=1     no ball, so handle_no_ball_death fires on the
                        FIRST frame

so the death is immediate and deterministic rather than trajectory- or
timing-dependent. BATTY_HOLD_GAME_OVER then holds the screen for a key,
exactly as BATTY_HOLD_ROUND_BANNER does for the round banner, so the
capture cannot race the 65-tick timer.

WHAT THIS CHECKS: that the screen is reached at all, that it is the
CLEARED game-over screen and not the playfield, and that the three text
lines land on the rows render_game_over draws them at. That last one is
what a source gate cannot do — the ordering gate stays green if the
glyphs go off-screen or come out in the background colour.

A note on the discriminator. "How many pixels are lit" does NOT separate
these two screens: the playfield background is not black, so both are
~100% non-zero. What separates them is UNIFORMITY — the game-over screen
is 98% one cleared colour, the playfield 54% its most common one. The
first version of this gate used lit-pixel count and failed against a
correct capture, which is how the difference got measured.

WHAT IT DOES NOT CHECK: the glyph shapes against the Spectrum. There is
no captured ground truth for this screen (notes/parity-gaps.md), so this
pins the port's own rendering. Said out loud so a green run is not
over-read as parity.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from collections import Counter
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import ppm_inner_to_indices, run_qemu, test_floppy

TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_game_over_visual")


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    for needle, why in (
        ("BATTY_HOLD_GAME_OVER",
         "the hold hook this gate captures through"),
        ("BATTY_REPLAY_LIVES",
         "the knob that makes one death enough to end the game"),
    ):
        if needle not in src:
            raise SystemExit(f"FAIL: {needle} is gone — {why}")


def build_floppy() -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_LIVES": "1",
        "BATTY_HIDE_BALL": "1",
        "BATTY_HOLD_GAME_OVER": "1",
        "BATTY_NOSOUND": "1",
    })
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture() -> list:
    OUT.mkdir(parents=True, exist_ok=True)
    ppm = OUT / "game_over.ppm"
    # Long enough to cover boot, the round banner, the death animation and
    # the bat explosion. The hold means overshooting is free; undershooting
    # is what would break it, so this is generous on purpose.
    script = [
        "SLEEP 14.0",
        f"screendump {ppm}",
        "sendkey ret",
        "SLEEP 0.3",
        "sendkey ret",
        "SLEEP 0.3",
    ]
    run_qemu(TEST_FLOPPY, script, OUT / "qemu.log")
    return ppm_inner_to_indices(ppm)


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()
    build_floppy()
    idx = capture()

    if len(idx) != 256 * 192:
        raise SystemExit(f"FAIL: captured {len(idx)} pixels, expected "
                         f"{256 * 192} — the screen was not the playfield "
                         f"(a 720x400 result means the port never left text "
                         f"mode; see test_visual.ppm_inner_to_indices)")

    counts = Counter(idx)
    bg, bg_n = counts.most_common(1)[0]
    share = bg_n / float(len(idx))
    if share < 0.90:
        raise SystemExit(
            f"FAIL: the most common colour covers only {share:.1%} of the "
            f"screen. render_game_over fills the whole screen first, so a "
            f"cleared screen is ~98% one colour; the playfield is ~54%. "
            f"This looks like the level, i.e. the run never died — check "
            f"that BATTY_REPLAY_LIVES reached DOS (it must be in the "
            f"AUTOEXEC_T passthrough in the Makefile, not only in src).")

    # The three lines render_game_over draws in ink 15, at BORDER_Y + 70,
    # + 95 and + 110. Glyphs are 6px tall and TOP-anchored here — unlike
    # the round banner, whose original coordinates are bottom-anchored
    # (that difference was a real bug once; see show_round_banner).
    want = [(70, 75), (95, 100), (110, 115)]
    rows = [y for y in range(192)
            if any(idx[y * 256 + x] == 15 for x in range(256))]
    if not rows:
        raise SystemExit(
            "FAIL: no ink-15 pixel anywhere. The screen cleared but no text "
            "was drawn, or it was drawn in the background colour.")

    bands, run = [], [rows[0]]
    for y in rows[1:]:
        if y == run[-1] + 1:
            run.append(y)
        else:
            bands.append((run[0], run[-1]))
            run = [y]
    bands.append((run[0], run[-1]))

    if bands != want:
        raise SystemExit(
            f"FAIL: text bands are {bands}, expected {want} — the "
            f"GAME OVER / SCORE / HIGH lines must land at BORDER_Y + 70, "
            f"+ 95 and + 110 as render_game_over draws them.")

    xs = [x for x in range(256)
          if any(idx[y * 256 + x] == 15 for y in range(192))]
    if xs[0] < 8 or xs[-1] > 247:
        raise SystemExit(f"FAIL: text spans x {xs[0]}..{xs[-1]}, which "
                         f"runs into the frame border")

    print(f"PASS game_over_visual: cleared screen ({share:.1%} one colour), "
          f"three text bands at {bands}, x {xs[0]}..{xs[-1]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
