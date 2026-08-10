#!/usr/bin/env python3
"""After a death, the player RESPAWNS — bat re-centred, fresh ball, bonuses gone.

WS4 lists four game-flow transitions. Three were gated:
`test-level-advance` (level-clear -> next level, and the L15 wrap),
`test-name-entry-visual` (game-over -> initials), `test-game-over` (the
sequence's source shape). The fourth, life-loss -> respawn, was gated
only on its FIRST half: `test-life-loss` counts life indicators, so it
proves a life was taken and says nothing about what the player gets
back.

`respawn_primary_ball` does eleven separate resets. None of them was
pinned by anything.

### Why a displaced, bonus-carrying bat

The hard part of gating a respawn is that a fresh LEVEL ENTRY produces
almost the same state, so a run that never died still looks correct.
The discriminator is ordering: `BATTY_REPLAY_BAT_OBJECT` is applied
AFTER level entry, so it can put the bat somewhere entry never would.

  seeded    bat at $C0 carrying bonus $83
  respawn   must drag it back to BAT_X_INIT $74 with bonus $FF

A gate that seeded nothing would pass against a `respawn_primary_ball`
whose body had been deleted entirely.

### The A/B

Both runs are mode 0, three lives, `BATTY_HIDE_BALL=1` so
`handle_no_ball_death` fires on the first frame — the same trick
test-life-loss uses. HIDE_BALL applies once at entry, not per frame, so
`BALL_SHOW()` inside the respawn ends the sequence at exactly one death.

  died        -> everything back to its life-start values, lives 3 -> 2
  suppressed  -> BATTY_SUPPRESS_NO_BALL_DEATH=1: the seed stands, lives 3

The control is what makes the subject mean anything: it shows the seeded
values really do survive a run of the same length, so the subject's
reset values came from the respawn and not from the game never having
honoured the seed in the first place.

### Frame budget, and why BATTY_SERIAL_PROBE is forced on

A respawned ball auto-launches after 192 ticks (test-stuck-auto-launch)
and could then fall and die a second time, which would reset the same
values again and hide a broken respawn behind a working one. 40 frames
is comfortably inside that window.

The gate sets `BATTY_SERIAL_PROBE=1` itself rather than inheriting it
from run_gates_parallel. `play_bat_explosion` is a BLOCKING inner loop
— it runs the spark animation and then LBC10's 45-tick pause without
ever reaching `visual_checkpoint_tick` — so a death adds about a second
that a wall-clock frame budget does not account for. Run standalone
without the serial marker, every checkpoint was missed and PROBE.TXT
still held the level-entry write: `probe_phase=init`, every value
exactly what the seed put there, and a gate that read them would have
passed while proving nothing. That failure is silent, which is why the
gate does not leave the choice to its caller.

### What this does NOT prove, established by mutation

Five assertions are live — each was mutation-tested and the mutant
died. Three more were written and then REMOVED, because mutating the
line each was supposed to pin left the gate green:

  `ball.speed_ramp = 0`, `bat1.extra_target`, `ball.big_ticks` — all
  three are already 0 in this scenario, so asserting they are 0 after
  the death asserts nothing. Seeding them is not cheap: the bat's +14
  bonus byte alone does not start a resize (tried: seeding $00 =
  BIG_BAT leaves xtgt at 0), because the width is driven by the
  `(IX+$15)` state machine that PLAN.md lists as an accepted residual.
  A PASS line that cannot fail is worse than no line, so they are gone.

Two mutants survived and are recorded rather than chased:

  `BALL_Y = BAT_Y - BALL_H_PX` in the respawn is redundant. While the
  ball is stuck, `rest_ball_on_bat` recomputes the same $A6 every
  frame, so the respawn's own assignment is dead by the time any
  checkpoint can look. It is observable only at frame 0.

  `ball.stuck_bat = OBJ_BAT_1` is equivalent in mode 0, where bat 2 is
  inactive. Catching it needs a Double Play scenario in which bat 2
  held the ball before the death — which is what the line exists for.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-respawn.img")
FRAME = int(os.environ.get("BATTY_RESPAWN_FRAME", "40"))

# The shared 22-byte bat seed, with x (+02) and prev_x (+0E) moved from
# $74 to $C0. Its +14 is $83 — a bonus code, not $FF — which is what the
# respawn has to clear.
BAT_SEED_X = 0xC0
BAT = ("0101" f"{BAT_SEED_X:02X}" "00AD000000040DEFAE1C0A"
       f"{BAT_SEED_X:02X}" "AD040DF0008380")

O_X, O_Y, O_BONUS = 0x02, 0x04, 0x14      # Object offsets, src/objects.h

BAT_X_INIT = 0x74
BAT_Y_PX = 0xAD
BALL_H_PX = 7
BALL_X_OFFSET_ON_BAT = 16
NO_BONUS = 0xFF

WANT_BALL_X = (BAT_X_INIT + BALL_X_OFFSET_ON_BAT) & 0xFF   # $84
WANT_BALL_Y = (BAT_Y_PX - BALL_H_PX) & 0xFF                # $A6


def probe(suppress: bool):
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    out = ROOT / "build/PROBE_respawn.txt"
    out.unlink(missing_ok=True)
    env = (
        f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 BATTY_LEVEL=1 "
        f"BATTY_GAME_MODE=0 BATTY_NOSOUND=1 BATTY_REPLAY_PROBE=1 "
        f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
        f"BATTY_REPLAY_LIVES=3 BATTY_HIDE_BALL=1 BATTY_SERIAL_PROBE=1 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT} "
        f"{'BATTY_SUPPRESS_NO_BALL_DEATH=1 ' if suppress else ''}"
        f"BATTY_VISUAL_PROBE_FRAMES={FRAME}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(FRAME), "--wait-key",
                    "--out", "build/tl_respawn"], cwd=ROOT,
                   env=dict(os.environ, BATTY_SERIAL_PROBE="1"),
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(out)],
                   cwd=ROOT, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    if not out.exists():
        return None
    t = out.read_text()
    bat = re.search(r"object_bat_1=([0-9A-Fa-f]+)", t)
    ball = re.search(r"object_ball_1=([0-9A-Fa-f]+)", t)
    eff = re.search(r"effects_state=b2[0-9A-Fa-f]{2}_b3[0-9A-Fa-f]{2}"
                    r"_xtgt([0-9A-Fa-f]{2})_bball([0-9A-Fa-f]{2})"
                    r"_lives([0-9A-Fa-f]{2})", t)
    ramp = re.search(r"speed_ramp_state=ramp([0-9A-Fa-f]{4})", t)
    if not bat or not ball or not eff or not ramp:
        raise SystemExit("FAIL: PROBE.TXT is missing object_bat_1, "
                         "object_ball_1, effects_state or speed_ramp_state")
    b, q = bytes.fromhex(bat.group(1)), bytes.fromhex(ball.group(1))
    return {
        "bat_x": b[O_X], "bat_bonus": b[O_BONUS],
        "ball_x": q[O_X], "ball_y": q[O_Y],
        "xtgt": int(eff.group(1), 16), "bigball": int(eff.group(2), 16),
        "lives": int(eff.group(3), 16), "ramp": int(ramp.group(1), 16),
    }


def check(name: str, got, want, note: str) -> bool:
    ok = got == want
    print(f"    {name:<12} ${got:02X} (want ${want:02X}) "
          f"[{'PASS' if ok else 'FAIL'}] — {note}")
    return ok


def main() -> int:
    ok = True

    ctl = probe(suppress=True)
    if ctl is None:
        print("  control: NO PROBE.TXT [FAIL]")
        return 1
    print("  control — BATTY_SUPPRESS_NO_BALL_DEATH=1, so nobody dies:")
    ok &= check("bat x", ctl["bat_x"], BAT_SEED_X,
                "the seed stands; a run of this length does not move it")
    ok &= check("bat bonus", ctl["bat_bonus"], 0x83,
                "and nothing clears it either")
    ok &= check("lives", ctl["lives"], 3, "no life taken")

    sub = probe(suppress=False)
    if sub is None:
        print("  died: NO PROBE.TXT [FAIL]")
        return 1
    print("  subject — the ball is hidden, so the first frame kills it:")
    ok &= check("lives", sub["lives"], 2, "one life gone")
    ok &= check("bat x", sub["bat_x"], BAT_X_INIT,
                "re-centred (all_var_init's LDIR), not left at $C0")
    ok &= check("bat bonus", sub["bat_bonus"], NO_BONUS,
                "the dead life's bonus does not carry into the new one")
    ok &= check("ball x", sub["ball_x"], WANT_BALL_X,
                f"bat x + {BALL_X_OFFSET_ON_BAT}, back on the bat")
    ok &= check("ball y", sub["ball_y"], WANT_BALL_Y,
                "LA27E_15's $A6 — the ball's bottom row on the bat's top")

    if ok:
        print("PASS life_respawn: the death gives the player back a centred "
              "bat, a fresh ball and no leftover bonuses")
        return 0
    print("FAIL life_respawn")
    return 1


if __name__ == "__main__":
    sys.exit(main())
