# Shortcuts — technical debt to repay

Running list of places where we ship captured asset bytes instead
of porting the logic that produced them. Each shortcut should be
repaid before its area becomes dynamic.

## Unresolved

### `assets/frame_l1.bin` — frame ornament pixels

The per-level frame ornament (HUD strip + side strips) is shipped
as a single 24 KB blob captured from the GT — 4 cycles × (top
24 px + 2 side strips). `paint_frame_to_buff` blits it directly.

The original computes the ornament at runtime from
`spr_bord_horiz_*` and `spr_bord_left/right_*` sprite primitives
(see `original/disasm/batty.asm` around line 6940). Porting that
would drop the captured-asset dependency and make the frame fully
deterministic from sprite data.

Status: not blocking anything visible; the captured blob renders
pixel-identical against the GT for L1..L4 cycles. Repay when we
need per-level frame variations the captured blob doesn't cover.

### `assets/level_attrs.bin` — per-level brick / frame attrs

The 15 × 768 B attr bands (one per level) are extracted from the
GT captures and copied wholesale into `attr_buff` for char rows
3..16. The original computes brick attrs dynamically via
`briks_colors[]` and `brik_shadow_c` (both ported); the only
piece still asset-shipped is the frame-strip cols within the
brick band, and the pre-dimmed magenta shadow attrs at the
shadow row of each brick.

Status: partial repayment. `print_one_brik_buf_c` writes brick
body attrs at runtime; destroyed cells reset to bg_attr. The
non-brick cells (frame strips, shadow rows in between bricks)
still use the captured values.


## `BATTY_KINNOCK=1` — the easter egg

The original hides a message behind a single byte:

    kinnock:
      DEFB $01   ; "если сюда записать ноль, то перед игрой будет
                 ;  надпись про Киннока"

`POKE 47475,0` ($B973) and `print_kinnock` fires:

    print_kinnock:
      LD A,(kinnock) / AND A / RET NZ
      LD DE,txt_kinnock / LD B,$02 / CALL print_message
      LD D,$00 / CALL pause_short
      JP clear_screen_attrib

"KINNOCK COULDNT RUN / A YOUTH CLUB." — a 1987 dig at Neil Kinnock, then
Leader of the Opposition.

Two things about it are not what you would guess:

**It is not once per game.** `print_kinnock` is called from `LB9E8_1`,
which is the per-LEVEL entry (reached from `LBBFB`/`LBC10` as well as
from `game_restart`), so with the byte poked it shows at the start of
every level.

**It is up for about a third of a second.** `pause_short` with `D=0` is
256 iterations of a 255-step inner loop, ~1.05M T-states, ~0.30 s at
3.5 MHz. The disassembly's own arithmetic agrees — `LD B,$04 / CALL
pause_long` is annotated "Пауза 1,2 сек. (4*0.3)". It is a flash, not a
screen you read.

It also runs at a specific moment: after the level has been drawn into
the BUFFER and the attributes cleared, but before `buff_to_screen_pixs`
flushes it. So it appears over a blank screen, and the level arrives
immediately after.

The port reproduces all of that behind `BATTY_KINNOCK=1`, off by
default. Coordinates come from `txt_kinnock`'s own headers — `($38,$37)`
and `($50,$47)` — with the bottom-anchor conversion (`y - 5`), the same
one the round banner needs.

`test-kinnock` gates it, and PARSES the expected text out of
`original/disasm/txt/txt_kinnock.asm` rather than carrying a copy: a
gate with its own transcription agrees with a wrong transcription in the
port as long as both are wrong the same way. It is a source gate because
a timed QEMU screendump would have to land inside a 0.3 s window, and a
gate that depends on luck is worse than none (known-bugs #17).


## `BATTY_FAST_HOLDS=1` — cut the game-over hold to 2 frames

`play_game_over` waits 178 PIT frames (~3.5 s), the port's count of the
original's `pause_long B=$0C`. Inside a capture window that is enough to
end the run before anything downstream happens, which is why the
2-player `LBC10_7` hand-over could not be gated: `PROBE.TXT` still held
the level-ENTRY write from before the death, and every counter read 0 —
including one incremented on the line immediately before the hold.

`BATTY_FAST_HOLDS=1` makes the wait 2 frames.

It is deliberately NOT the same knob as `BATTY_HOLD_GAME_OVER`, which
makes the hold wait for a keypress instead of a timer. That one exists
so a visual gate can screendump the screen while it is up; this one
exists so a gate can get PAST it. Opposite needs, and merging them would
give one knob two contradictory meanings.

The 178 literal stays in the source either way — `test-game-over` pins
it, and the knob picks between `2UL` and `178UL` rather than replacing
the constant.


## `BATTY_INFINITE_LIVES=1` — play without dying out

For playing the port by hand. Every other knob here serves a gate; this
one serves a person who wants to reach level 12 without three deaths
sending them back to the title.

It suppresses the DECREMENT and nothing else. The bat still explodes,
the ball still respawns, the death sound still plays, and the lives
indicators still show the same two bats they show at level entry —
because a build that skipped the death sequence would be a different
game from the one the port ships, and the point of a manual-testing
build is to meet the shipped one.

Both life-taking paths route through `take_a_life` for this: the ball
loss at `lose_a_life`, and a bomb landing on the bat in `step_bomb`,
which decrements on its own rather than calling `lose_a_life` (it
mirrors the original's $A69D, which zeroes `balls_quantity` and lets
LBC10 do the rest on the next frame). A switch guarding only the first
would have worked everywhere except deaths by bomb — the least likely
death to be watching for when you have just set a switch that says
lives are infinite.

Two things it does NOT do:

- **It does not make the game unloseable in 2 players.** The turn still
  changes hands on each life loss (`pending_turn_change`), because that
  is the life-LOSS path, not the counter. With both players immortal
  the turn simply alternates for ever.
- **It does not reach the game-over screen**, which is the whole idea,
  so it is useless for exercising anything downstream of it. Use
  `BATTY_REPLAY_LIVES=1` for that — it seeds a LOWER count and is the
  opposite tool.

Unlike the other knobs here it is passed through by the RELEASE floppy's
`AUTOEXEC.BAT` as well as the test floppy's, since a knob for manual
play that only reaches gates would be exactly backwards.
`BATTY_KINNOCK` joined it there for the same reason: it had been
readable by the EXE and unreachable from `make run` since the day it
landed.
