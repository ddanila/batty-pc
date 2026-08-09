#!/usr/bin/env python3
"""Capture the NEW HIGH SCORE name-entry screen.

The last screen in the game with no visual coverage. Reaching it needs
three things stacked:

  BATTY_REPLAY_LIVES=1   one life, so a single death ends the game
  BATTY_HIDE_BALL=1      no ball, so handle_no_ball_death fires on the
                         FIRST frame
  BATTY_REPLAY_SCORE=N   a score that BEATS the stored high score, which
                         is what makes play_game_over call this at all

then one key to leave the game-over screen.

The ENTER press leaves the game-over screen promptly. It used to be
mandatory: the hold counted BIOS ticks and bios_ticks() does not advance
during gameplay, so the loop was infinite except for a keypress
(known-bugs.md #15). That is fixed — the hold counts PIT frames now and
expires on its own in ~3.6 s — but the key is kept, because a gate that
does not depend on a timer is the more robust of the two.

WHAT THIS CHECKS: that the screen is reached, that it is cleared, and
that all four elements land where input_new_record_name draws them —
title, prompt, the three-letter row, and the hint. Three different inks
are involved (14 title, 15 prompt and letters, 13 hint), and checking
them separately is what catches a colour regression as well as a
position one.

The blink is deliberately not asserted on. The current slot alternates
between ink 15 and ink 8 via blink_phase(), so the letter row's exact
ink is timing-dependent; the gate requires the row to EXIST in ink 15,
which holds in both blink phases because two of the three letters are
never dimmed.

WHAT IT DOES NOT CHECK: glyph shapes against the Spectrum. No captured
ground truth exists for this screen (notes/parity-gaps.md), so this pins
the port's own rendering.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import ppm_inner_to_indices, run_qemu, test_floppy

TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_name_entry_visual")

# (ink, band, what it is) — from input_new_record_name's draw calls.
EXPECT = [
    (14, (50, 55),   "NEW HIGH SCORE title at BORDER_Y + 50"),
    (15, (70, 75),   "ENTER YOUR NAME prompt at BORDER_Y + 70"),
    (15, (90, 95),   "the three-letter row at BORDER_Y + 90"),
    (13, (130, 135), "L R SELECT ENTER hint at BORDER_Y + 130"),
]


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    for needle, why in (
        ("static void input_new_record_name(void)", "the screen this captures"),
        ("BATTY_REPLAY_SCORE", "the knob that makes the high score beatable"),
        ("BATTY_REPLAY_LIVES", "the knob that makes one death end the game"),
    ):
        if needle not in src:
            raise SystemExit(f"FAIL: {needle} is gone — {why}")


def build_floppy() -> None:
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_LIVES": "1",
        "BATTY_HIDE_BALL": "1",
        "BATTY_REPLAY_SCORE": "123456",
        "BATTY_NOSOUND": "1",
    })
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)


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


def capture():
    """Two shots: the screen as drawn, then after one LEFT.

    The second is what covers step_name_letter. Everything else here is
    static furniture that would look identical whether the arrow keys
    worked or not."""
    OUT.mkdir(parents=True, exist_ok=True)
    first = OUT / "name_entry.ppm"
    after = OUT / "name_entry_left.ppm"
    # 12 s covers boot, the round banner, the death and the bat explosion.
    # The single ENTER leaves the game-over screen; name entry then waits
    # for keys of its own, so the captures cannot race anything.
    run_qemu(TEST_FLOPPY,
             ["SLEEP 12.0", "sendkey ret", "SLEEP 1.5",
              f"screendump {first}",
              "sendkey left", "SLEEP 1.0",
              f"screendump {after}",
              "sendkey ret", "SLEEP 0.3"],
             OUT / "qemu.log")
    return ppm_inner_to_indices(first), ppm_inner_to_indices(after)


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()
    build_floppy()
    idx, idx_left = capture()

    if len(idx) != 256 * 192:
        raise SystemExit(f"FAIL: captured {len(idx)} pixels, expected "
                         f"{256 * 192} — the port never left text mode")

    bg, bg_n = Counter(idx).most_common(1)[0]
    share = bg_n / float(len(idx))
    if share < 0.90:
        raise SystemExit(
            f"FAIL: the most common colour covers only {share:.1%}, so this "
            f"is not a cleared screen. Either the run never died (check "
            f"BATTY_REPLAY_LIVES, and that a seeded score did not grant "
            f"extra lives through award_score_milestones) or the ENTER did "
            f"not leave the game-over screen.")

    seen = {ink: bands_of(idx, ink) for ink in (13, 14, 15)}
    missing = [(ink, band, what) for ink, band, what in EXPECT
               if band not in seen[ink]]
    if missing:
        print("FAIL: the name-entry screen is not laid out as expected\n")
        for ink, band, what in missing:
            print(f"  ink {ink} band {band} absent — {what}")
        print(f"\n  actually present: "
              f"{ {k: v for k, v in seen.items() if v} }")
        print("\nIf this is the GAME OVER screen (ink 15 bands at 70, 95 and "
              "110), the ENTER did not register and the run is still in "
              "play_game_over.")
        return 1

    # LEFT must change the first letter. high_score_name starts AAA
    # ($0A), and $0A steps DOWN to $23 — the wrap, which is the one arm
    # of step_name_letter a single keypress can reach from the initial
    # state. Compare only the letter row: the rest of the screen is
    # static, and the blink would make a whole-screen compare flap.
    y0, y1 = 88, 102
    row_before = [idx[y * 256 + x] for y in range(y0, y1) for x in range(256)]
    row_after = [idx_left[y * 256 + x] for y in range(y0, y1) for x in range(256)]
    if row_before == row_after:
        raise SystemExit(
            "FAIL: LEFT did not change the letter row. step_name_letter "
            "should have stepped the first slot from $0A (A) round to $23. "
            "The row is pixel-identical, so either the key never reached "
            "the port or the arrow arm is broken.")

    # ...and the rest of the screen must NOT have moved.
    for ink, band, what in EXPECT:
        if band != (90, 95) and band not in bands_of(idx_left, ink):
            raise SystemExit(f"FAIL: after LEFT, {what} is gone — the arrow "
                             f"key should only repaint the letter row")

    print(f"PASS name_entry_visual: cleared screen ({share:.1%} one colour), "
          f"title/prompt/letters/hint all placed, LEFT cycles a letter")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
