# Debug and play switches

Every `BATTY_*` switch's default is declared in `DebugSwitches` in
`src/main.cpp` and checked against the initialiser by
`test-switch-defaults`; `test-env-passthrough` checks that each one
actually reaches DOS through the test floppy's `AUTOEXEC.BAT`. **Add a new
knob to the passthrough list or it silently does nothing** — a gate that
reads no keys at all can still print a plausible number.

This file covers the switches that need more than a line. The replay
seeding knobs (`BATTY_REPLAY_*`) are in `notes/replay-harness.md`.

## Captured assets: repaid (WS7)

This file used to open with two shortcuts — `assets/frame_l1.bin` and
`assets/level_attrs.bin` shipping captured bytes. Both are closed. The port
builds the frame ornament from the tape's `spr_bord_*` primitives and the
attribute band from `assets/levels.bin` plus the tape's `briks_colors`, at
runtime, and all 13 loaded assets derive from `original/blocks/`
(`test-asset-provenance`, `test-frame-derivable`,
`test-level-attrs-derivable`, `test-bg-tile-derivable`).

The two `.bin` files still exist in `assets/`, but as REFERENCE blobs for
those derivability gates rather than as anything the floppy carries. See
`notes/levels.md`.

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

Two things about it are not what you would guess. **It is not once per
game:** `print_kinnock` is called from `LB9E8_1`, the per-LEVEL entry
(reached from `LBBFB`/`LBC10` as well as from `game_restart`), so with the
byte poked it shows at the start of every level. And **it is up for about a
third of a second:** `pause_short` with `D=0` is 256 iterations of a
255-step inner loop, ~1.05M T-states, ~0.30 s at 3.5 MHz. The
disassembly's own arithmetic agrees — `LD B,$04 / CALL pause_long` is
annotated "Пауза 1,2 сек. (4*0.3)". It is a flash, not a screen you read.

It also runs at a specific moment: after the level has been drawn into the
BUFFER and the attributes cleared, but before `buff_to_screen_pixs` flushes
it. So it appears over a blank screen and the level arrives immediately
after.

The port reproduces all of that, off by default. Coordinates come from
`txt_kinnock`'s own headers — ($38,$37) and ($50,$47) — with the
bottom-anchor conversion (`y - 5`), the same one the round banner needs
(`notes/menu.md`).

`test-kinnock` PARSES the expected text out of
`original/disasm/txt/txt_kinnock.asm` rather than carrying a copy: a gate
with its own transcription agrees with a wrong transcription in the port as
long as both are wrong the same way. It is a source gate because a timed
QEMU screendump would have to land inside a 0.3 s window, and a gate that
depends on luck is worse than none (known-bugs #17).

## `BATTY_FAST_HOLDS=1` — cut the game-over hold to 2 frames

`play_game_over` waits 178 PIT frames (~3.5 s), the port's count of the
original's `pause_long B=$0C`. Inside a capture window that is enough to end
the run before anything downstream happens, which is why the 2-player
`LBC10_7` hand-over could not be gated: `PROBE.TXT` still held the
level-ENTRY write from before the death, and every counter read 0 —
including one incremented on the line immediately before the hold.

It is deliberately NOT the same knob as `BATTY_HOLD_GAME_OVER`, which makes
the hold wait for a keypress instead of a timer. That one exists so a visual
gate can screendump the screen while it is up; this one exists so a gate can
get PAST it. Opposite needs, and merging them would give one knob two
contradictory meanings.

The 178 literal stays in the source either way — `test-game-over` pins it,
and the knob picks between `2UL` and `178UL` rather than replacing the
constant.

## `BATTY_INFINITE_LIVES=1` — play without dying out

For playing the port by hand. Every other knob here serves a gate; this one
serves a person who wants to reach level 12 without three deaths sending
them back to the title.

It suppresses the DECREMENT and nothing else. The bat still explodes, the
ball still respawns, the death sound still plays, and the lives indicators
still show the same two bats they show at level entry — because a build that
skipped the death sequence would be a different game from the one the port
ships, and the point of a manual-testing build is to meet the shipped one.

Both life-taking paths route through `take_a_life` for this: the ball loss
at `lose_a_life`, and a bomb landing on the bat in `step_bomb`, which
decrements on its own rather than calling `lose_a_life` (it mirrors the
original's $A69D, which zeroes `balls_quantity` and lets `LBC10` do the rest
next frame). A switch guarding only the first would have worked everywhere
except deaths by bomb — the least likely death to be watching for when you
have just set a switch that says lives are infinite.

Two things it does NOT do. **It does not make the game unloseable in 2
players:** the turn still changes hands on each life loss
(`pending_turn_change`), because that is the life-LOSS path, not the
counter, so with both players immortal the turn simply alternates for ever.
And **it does not reach the game-over screen**, which is the whole idea, so
it is useless for exercising anything downstream. Use `BATTY_REPLAY_LIVES=1`
for that — it seeds a LOWER count and is the opposite tool.

Unlike the other knobs here it is passed through by the RELEASE floppy's
`AUTOEXEC.BAT` as well as the test floppy's, since a knob for manual play
that only reached gates would be exactly backwards. `BATTY_KINNOCK` joined
it there for the same reason: it had been readable by the EXE and
unreachable from `make run` since the day it landed.

## Debug switch defaults

`use_laffc` and `rng_perframe` default **ON** — they select the accurate
model, and the switch exists to get the old one back for an A/B
(`BATTY_LEGACY_COLLISION`, `BATTY_RNG_PERFRAME=0`). Everything else defaults
off: `BATTY_AUTO_FIRE`, `BATTY_FULL_BAND_REBUILD`,
`BATTY_FORCE_BAT_FULL_REDRAW`, `BATTY_FORCE_BALL_FULL_REDRAW`,
`BATTY_FORCE_FULL_FLUSH_EACH_FRAME`, `BATTY_SUPPRESS_NO_BALL_DEATH`,
`BATTY_PROFILE_AUTO_FRAMES`, `BATTY_KINNOCK`, `BATTY_FAST_HOLDS`,
`BATTY_INFINITE_LIVES`.

The three FORCE_*_REDRAW knobs are the A/B baselines the dirty-redraw gates
compare against, and they are not interchangeable:
`FORCE_BALL_FULL_REDRAW` forces the full COMPOSE while still flushing by
dirty ranges, and `FORCE_FULL_FLUSH_EACH_FRAME` pins VGA == buffers every
frame. Only the second can see a stale-VGA defect, because with the first
both sides flush identically and identical staleness cancels out
(`notes/performance.md`).
