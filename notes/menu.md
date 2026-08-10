# Menu, and two-player turn alternation

## Leaving the menu: key 0, and nothing else

`main_menu`'s tail, after the A/B device rows and the mode rows:

    L93F8_10:
      LD DE,current_game_mode_line_prop
      CALL fill_color_current_game_mode
      LD A,$EF
      CALL in_a_fe          ; IN A,($EFFE) — the 6 7 8 9 0 row
      AND $01
      RET NZ                ; key 0 pressed -> leave, and start the game
      JP L93F8_0            ; anything else -> back round the poll loop

The `RET NZ` reads backwards until you look at `in_a_fe`:

    in_a_fe:
      IN A,($FE) / CPL / AND $1F / RET

The `CPL` is the whole story. ZX keyboard reads are active LOW, so after
the complement a SET bit means PRESSED, and `AND $01 / RET NZ` is "key 0 is
down". Bit 0 of the `$EFFE` row is the **0** key; there is no ENTER
anywhere in this routine.

The other exit is the attract timeout at the TOP of the loop: `counter_misc`
is incremented and `BIT 6,H` jumps to `disp_hs_table_and_wait_keys` after
~16384 frames. That is a free-running counter, not an idle timer — pressing
keys does not postpone it in the original, though the port's `run_menu`
resets its own `last_input` on A/B/1-3.

**What the port does.** `run_menu` returns `ST_LEVEL` on '0', which runs
`new_game_reset` and enters the round-1 banner. ENTER is kept, deliberately,
as the port's own attract-chain step: `test-visual` walks title -> menu ->
hi-score -> level by sending ENTER at each state, so making ENTER start a
game would have made `state3_hiscore` capture a level — and because each
checkpoint diffs against its OWN reference, the failure would have read as a
rendering regression on the wrong screen rather than as a navigation change.
`test-menu-start` pins both halves.

## Menu structure

The idle poll loop reads keyboard row `$EF` (keys 6..0) via the half-row
helper, busy-waits `$80` ticks, increments the frame counter at `$8D46`, and
loops. `sub_926bh` is the refresh: setup, compose the live game state at
`$BF00` into the render buffer at `$8FD1`, then tail-call the markup text
renderer with `B = 22` rows (see `notes/encoding.md`).

Addresses worth knowing:

| addr | role |
|---|---|
| `$8D46` | frame counter (16-bit) |
| `$8D48` | `random_number` (see `notes/rng-model.md`) |
| `$8D4A` | `random_seed`, the ROM read pointer |
| `$B7E5` | selected play mode; 0 = "1 PLAYER" at boot |
| `$B7EF` | player 1's input device, 0..3, cycled by A |
| `$B7F7` | player 2's input device, 0..3, cycled by B |

The mode keys 1/2/3 are read from row `$F7` and dispatch through
per-option config structs at `$9571`/`$9592`.

### Input devices

A and B cycle each player's device through KEYBOARD -> KEMPSTON -> CURSOR
-> INTERFACE II -> wrap, with a debounce counter at `$938A` preventing
auto-repeat. Each handler then calls the in-place redraw helper, so pressing
A or B patches the one option label rather than redrawing the menu.

That helper also paints the player-1 / player-2 indicator glyph clusters
DIRECTLY to VRAM, outside the markup pipeline — playfield y=18..23
(non-bright white, from x≈16) and y=100..105 (bright white, from x≈40, the
KEYBOARD row). Neither y is matched by any markup record, which is why they
need their own draw path in the port.

The device byte selects nothing in the port; PLAN.md WS1 holds the decision
about what to do with a Spectrum joystick list on a PC.

### The selected option blinks by attribute rewrite

No hardware FLASH bit is involved. `sub_961c` ($961C) writes **11
attribute bytes** for the selected option's row, covering cols 14..24 — the
text after the "N - " prefix at cols 10..13. Eleven cells is a fixed width,
wider than "1 PLAYER" but just enough to span "DOUBLE PLAY". The value comes
from a 16-entry table at `$9643`: `{$00 x8, $47 x8}` — eight phases of
invisible (black ink on black paper) then eight of bright white. The phase
advances when `(frame_counter & $1F) == 0`.

So the markup's default green is overwritten every frame for the selected
row; you never actually see green on a selected option. The prefix keeps its
markup attr `$44`, since `sub_961c` does not touch it. There is no separate
"highlight neighbour" path.

## The round-intro window — coordinates are BOTTOM-anchored

`show_window_round_number` ($8F60) draws the "PLAYER 1 / ROUND XX" window
held ~1.2 s at each level entry.

**`screen_addr_calc` takes a pixel (x=L, y=H) coordinate**, not a packed ZX
address — but the window and its text are drawn **UPWARD** from that row
(the box loop uses `dec_scr_line`, and `print_line` stacks glyph rows
upward). So the coordinate byte is the BOTTOM row of what gets drawn:

| thing | anchor | occupies |
|---|---|---|
| window | $A458 = (x=88, y=164), 32 px tall | y=133..164 |
| PLAYER text | `txt_player_x` = (96, 143) | ink at y=138..143 |
| ROUND text | `txt_round_xx` = (96, 158) | ink at y=153..158 |

Net: text vertically centred, 5 px margin top, 6 px bottom.

The port passed the raw 143 / 158 as the TOP y of `draw_text`, which is
top-anchored, so both lines sat 5 px low, jammed against the box bottom. It
passes `BORDER_Y+138` / `BORDER_Y+153` now.
`scripts/test_round_banner_border.py` had ENCODED the bug — its
`TOP_BAND_H` assumed an 8 px black top margin where the original has 5.

**Sprites are drawn bottom-up throughout.** `print_sprite_pix` moves to the
PREVIOUS buffer line each row, so the first data row lands at y and the rest
stack above. The round banner, the Kinnock egg and the frame's top strip all
anchor at the bottom.

Capturing the original's banner needs
`scripts/capture_round_banner_original.py`, which reads ULA `$4000` while PC
sits in the `pause_short` busy-wait. A breakpoint cannot be used: arming one
parks the emulator in a cpu-step state where injected keys never register.

## Two-player turn alternation

`game_mode` is 0-based — 0 = 1 Player, 1 = 2 Players, 2 = Double Play —
read off `LBC10`, where `CP $02 / CALL Z,print_txt_players_1_and_2` prints
"PLAYERS 1 AND 2" for mode 2 and the life-loss path's
`LD A,(game_mode) / DEC A / CALL Z` runs the alternation for mode 1. The
port's `selected_mode` is 1..3 (`k - '0'`), so any dispatch subtracts one.

The life-loss path:

    LD A,(lives_1up) / DEC A / LD (lives_1up),A
    JR Z,LBC10_6                    ; out of lives -> the game-over path
    LD A,(game_mode) / DEC A
    CALL Z,current_level_2up_copier ; mode 1 only
    JP LB9E8_1                      ; per-level entry

### `current_level_2up_copier` FALLS THROUGH into `players_swap`

    current_level_2up_copier:
      LD A,(lives_2up) / AND A / RET Z    ; no player 2 -> neither half runs
      LD DE,(current_level_addr)          ; the ACTIVE player's level slot
      PUSH DE
      LD A,(current_level_number_2up)
      CALL level_addr_calc_a              ; HL = player 2's level slot
      POP DE
      LD BC,current_level_copy            ; the live 180-cell grid
      LD A,$B4                            ; 180 = 12 x 15 cells
    LBE0C_0:
      ...  (HL) <- live grid,  live grid <- (DE)  ...
      DEC A / JR NZ,LBE0C_0
    players_swap:                         ; <-- NO RET. Falls straight in.
      ... swaps 8 bytes at lives_1up and 10 at score_1up_in_game,
      ... then toggles player_number

So ONE call does both halves of a turn change: the live grid is written back
into the departing player's level slot, the arriving player's is loaded out
of theirs, and then the counters swap. **The level table doubles as
per-player grid storage** — there is no separate save area, which answers
the question "the original stores only `briks_quantity` per player, so what
does a returning player's brick field look like?" It is preserved.

The `RET Z` guard covers both halves, so a solo player keeps playing.

**This is why `players_swap` looked absent from the alternation.** It is
called by name only from `game_restart` and the game-over path; on an
ordinary life loss it is reached by fallthrough, so reading the two routines
separately makes 2-player mode look like it never changes turns.

Ported, with a per-player copy of the 180-cell grid swapped with
`live_level` at the same moment the counters swap. Gated by
`test-two-player-turn` and `test-two-player-state`.
