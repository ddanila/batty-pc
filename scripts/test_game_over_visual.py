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


def build_floppy(score: str | None) -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_LIVES": "1",
        "BATTY_HIDE_BALL": "1",
        "BATTY_HOLD_GAME_OVER": "1",
        "BATTY_NOSOUND": "1",
    })
    if score is not None:
        env["BATTY_REPLAY_SCORE"] = score
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


def capture(tag: str) -> list:
    OUT.mkdir(parents=True, exist_ok=True)
    ppm = OUT / f"game_over_{tag}.ppm"
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


def bands_of(idx, ink):
    rows = [y for y in range(192)
            if any(idx[y * 256 + x] == ink for x in range(256))]
    if not rows:
        return []
    out, run = [], [rows[0]]
    for y in rows[1:]:
        if y == run[-1] + 1:
            run.append(y)
        else:
            out.append((run[0], run[-1]))
            run = [y]
    out.append((run[0], run[-1]))
    return out


def check_screen(idx, tag: str) -> float:
    if len(idx) != 256 * 192:
        raise SystemExit(f"FAIL[{tag}]: captured {len(idx)} pixels, expected "
                         f"{256 * 192} — a 720x400 result means the port "
                         f"never left text mode")

    bg, bg_n = Counter(idx).most_common(1)[0]
    share = bg_n / float(len(idx))
    if share < 0.90:
        raise SystemExit(
            f"FAIL[{tag}]: the most common colour covers only {share:.1%}. "
            f"render_game_over fills the screen first, so a cleared screen "
            f"is ~98% one colour and the playfield ~54% — this is the level, "
            f"i.e. the run never died. Check BATTY_REPLAY_LIVES reached DOS "
            f"(Makefile AUTOEXEC_T passthrough), and that a seeded score did "
            f"not hand out extra lives via award_score_milestones.")

    # GAME OVER / SCORE / HIGH, ink 15, at BORDER_Y + 70, + 95, + 110.
    # Glyphs are 6px and TOP-anchored here, unlike the round banner's
    # bottom-anchored originals.
    want = [(70, 75), (95, 100), (110, 115)]
    got = bands_of(idx, 15)
    if got != want:
        raise SystemExit(
            f"FAIL[{tag}]: ink-15 bands are {got}, expected {want} — the "
            f"GAME OVER / SCORE / HIGH lines must land at BORDER_Y + 70, "
            f"+ 95 and + 110 as render_game_over draws them.")
    return share


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()

    # Both sides of render_game_over's `if (high_score_beaten_this_game)`.
    # Without a seeded score the run ends on 0, which does not beat the
    # stored high score, so the NEW HIGH line must be ABSENT. That branch
    # had no coverage at all until BATTY_REPLAY_SCORE existed.
    results = []
    for tag, score, expect_new_high in (
        ("plain", None, False),
        ("record", "123456", True),
    ):
        build_floppy(score)
        idx = capture(tag)
        share = check_screen(idx, tag)

        # NEW HIGH is drawn in ink 14 at BORDER_Y + 130; the saved
        # initials, also ink 14, sit on the HIGH line at + 110.
        ink14 = bands_of(idx, 14)
        has_new_high = (130, 135) in ink14
        if has_new_high != expect_new_high:
            raise SystemExit(
                f"FAIL[{tag}]: NEW HIGH line "
                f"{'missing' if expect_new_high else 'present'} — ink-14 "
                f"bands are {ink14}. With score "
                f"{score or '0'} the high score should "
                f"{'have been' if expect_new_high else 'NOT have been'} "
                f"beaten.")
        results.append(f"{tag}: {share:.1%} cleared, "
                       f"new_high={has_new_high}")

    print("PASS game_over_visual: " + "; ".join(results))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
