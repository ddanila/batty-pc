#!/usr/bin/env python3
"""Clearing a level advances to the next, and the level index wraps.

The last two game-FLOW transitions parity-gaps.md listed as ungated.
Both are covered here because both need the same thing: a level that
clears without the player having to destroy ~50 bricks with the ball,
which is neither quick nor deterministic.

BATTY_REPLAY_CLEAR_BRICKS marks every destructible cell destroyed at
level entry, so live_bricks_remaining() is 0 on the first frame and
run_level takes the level-clear branch immediately. The game then runs
through levels as fast as it can draw them, which is what makes the wrap
reachable at all.

The two assertions:

  ADVANCE   round_number > 0. The run started on round 0, so any
            increase is a completed level-clear -> next transition.
  WRAP      current_level == round_number % N_LEVELS, with round_number
            at or past N_LEVELS. run_level computes the level index that
            way ($BBE0's increment_round_number wraps
            current_level_number_1up at 15 while round_number keeps
            climbing), so the identity holding ACROSS the boundary is
            the wrap.

The identity is the strong half: it holds at every round, so a run that
overshoots proves nothing less than one that lands exactly on 15.
Observed: round 0x0F -> level 0, and round 0x16 (22) -> level 7.

LOAD SENSITIVITY, stated because it is real: how many levels a run gets
through depends on how fast the host draws them. Measured on an idle
machine at roughly one level per 1.3 s, so the sleep below reaches round
~30 with a wide margin over the 15 needed. Under heavy parallel load it
could fall short, and the failure message says so rather than leaving
someone to guess. This is the same hazard documented for
test-midgame-brick-replay in notes/testing.md.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, test_floppy

TEST_FLOPPY = Path(test_floppy())
OUT = Path("build/test_level_advance")
N_LEVELS = 15
SLEEP_S = 55


def source_guard() -> None:
    src = Path("src/main.cpp").read_text()
    for needle, why in (
        ("#define N_LEVELS   15",
         "the wrap modulus this gate computes against"),
        ("BATTY_REPLAY_CLEAR_BRICKS", "the knob that reaches the transition"),
        ("if (live_bricks_remaining() == 0) {",
         "the level-clear branch under test"),
    ):
        if needle not in src:
            raise SystemExit(f"FAIL: `{needle}` is gone — {why}")


def run() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update({
        "BATTY_START_LEVEL": "1",
        "BATTY_REPLAY_PROBE": "1",
        "BATTY_FRAME_PROBE": "40",
        "BATTY_REPLAY_CLEAR_BRICKS": "1",
        "BATTY_NOSOUND": "1",
    })
    TEST_FLOPPY.unlink(missing_ok=True)
    subprocess.run(["make", str(TEST_FLOPPY)], check=True, env=env)
    subprocess.run(["mdel", "-i", str(TEST_FLOPPY), "::PROBE.TXT"],
                   check=False, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    run_qemu(TEST_FLOPPY,
             ["SLEEP 8.0", "sendkey ret", f"SLEEP {SLEEP_S}.0"],
             OUT / "qemu.log")
    probe = OUT / "PROBE.TXT"
    probe.unlink(missing_ok=True)
    subprocess.run(["mcopy", "-i", str(TEST_FLOPPY), "::PROBE.TXT", str(probe)],
                   check=False, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not probe.exists():
        raise SystemExit("FAIL: no PROBE.TXT — the run never wrote one, so "
                         "the port did not reach a level at all")
    return dict(l.split("=", 1) for l in probe.read_text().splitlines()
                if "=" in l)


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    source_guard()
    p = run()

    try:
        rnd = int(p["round_number"], 16)
        lvl = int(p["current_level"], 16)
    except (KeyError, ValueError) as e:
        raise SystemExit(f"FAIL: PROBE.TXT lacks a usable round/level ({e}); "
                         f"keys present: {sorted(p)}")

    if rnd == 0:
        raise SystemExit(
            "FAIL: round_number is still 0 — no level-clear -> next "
            "transition happened. BATTY_REPLAY_CLEAR_BRICKS should empty "
            "the grid at entry; check it reached DOS (the AUTOEXEC_T "
            "passthrough) and that it does not clear indestructible cells, "
            "which stay live by design.")

    if lvl != rnd % N_LEVELS:
        raise SystemExit(
            f"FAIL: round {rnd} gives level {lvl}, expected {rnd % N_LEVELS}. "
            f"run_level derives the level as round_number % {N_LEVELS}; if "
            f"that is no longer true the wrap is broken.")

    if rnd < N_LEVELS:
        raise SystemExit(
            f"FAIL: reached only round {rnd} in {SLEEP_S}s, so the wrap at "
            f"{N_LEVELS} was never crossed and only the advance is proven. "
            f"This is the load-sensitive half of the gate — re-run alone, or "
            f"raise SLEEP_S. It is not a port regression unless the advance "
            f"assertion above also failed.")

    print(f"PASS level_advance: reached round {rnd} (level {lvl}) — "
          f"{rnd} level-clear transitions and {rnd // N_LEVELS} wrap(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
