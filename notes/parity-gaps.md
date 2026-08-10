# Original parity gaps

Where the port is deliberately or knowingly not the original. Separate
from `known-bugs.md`: none of these is a defect, and each is a next target
if fidelity is being tightened. What IS matched is in
`notes/parity-status.md`.

## 1. The bit-gated bat-resize state machine

Big-bat resize timing is matched visually rather than being a literal port
of the original's `(IX+$15)` machine.

**This item also owns a SOUND divergence**, because the two share
`bonus_flag`:

- `handling_bat`'s transform paths write it — `$C0` at `$AA0E`, `$41` at
  `$AA19`, and A on the `check_bat_increase_size` / `normal_bat` exits;
- `get_bonus` writes it too (`$80` when a bonus lands on a machine-gun bat,
  `$01` when the machine gun is caught, `LA67B_2`);
- `handling_bat` reads bit 7 of it to pick the gun sprite;
- and `play_sound_bat_resize_1` ($C200) opens
  `LD A,(bonus_flag) / AND A / RET NZ` — with the flag set it skips the
  beep AND leaves its counter alone, so the shrink sweep PAUSES while a
  bonus transition is in flight.

The port has no `bonus_flag` at all, so the sound guard cannot be ported
alone. Whoever takes this closes both — see `notes/sound.md`.

## 2. Sound durations quantise to 20 ms

The effect ids, slot count, pitches and envelope ARITHMETIC are faithful
(`test-sound-ids`). What remains is that the sound clock is the 50 Hz frame
counter, so every duration rounds to a frame against the original's 3-9 ms.

Closing it is a DESIGN call, not a finer clock: the original's beeper
BLOCKS, and the main loop branches on whether the queue fitted inside one
interrupt. Both options are in `notes/sound.md`; PLAN.md WS5 holds the
decision. Cycle-exact timbre stays a non-goal.

## 3. The GAME OVER screen's layout is the port's own

The original's message is exactly two lines, one `print_message` with
`B=$02`:

    txt_game_over:  DEFB $60,$4F,$47,$09 ; "GAME OVER"
    txt_player_0:   DEFB $60,$67,$47,$09 ; "PLAYER  n"

both at x=$60, 24 px apart, with the digit at `txt_player_0+$0C` patched
from `player_number`. No score, no high score — those stay on the HUD
underneath.

The port draws four lines — GAME OVER, PLAYER n, SCORE nnnnnn, HIGH nnnnnn
— on a cleared screen at BORDER_Y + 70 / 82 / 95 / 110. The SCORE and HIGH
lines are additions and the spacing is the port's; 24 px would put PLAYER
on top of SCORE. A deliberate divergence, but a divergence:
`test-game-over-visual` pins the current bands, so a faithful pass has to
move them on purpose.

## 4. Enemy sprite facing (cosmetic, and there is nothing to port)

The bird does not visually face its travel direction, and neither does the
original: `LAA02`'s mirror is dead code. Recorded here only because the
gap was once believed to exist. `notes/bird-render-parity.md`.

## 5. Infra: QEMU on CI

The full QEMU suite runs locally (`make parity-check-parallel`), not in CI.
The reasons and the current measurement are in `notes/testing.md`; PLAN.md
WS8.1 holds it.
