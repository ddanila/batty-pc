# Menu loop

## Polling loop entry: `0x9282` ("l9282h")

The deep idle loop the snapshots caught:

```
l927fh:
    call sub_926bh         ; menu refresh (text render + state copy)
l9282h:
    ld a, 0xEF             ; keyboard row 0xEF (keys 6..0)
    call sub_97a7h         ; sub_97a7h = "read keyboard half-row" helper
    rra
    ret c                  ; first bit → some exit
    xor a
    call sub_97a7h
    jp nz, 0x93F8          ; non-zero → another exit
    ld a, 0x80             ; 0x80 ticks busy-wait
l9292h:                    ; ← snap1 caught here (PC = 0x9292)
    dec a
    jr nz, l9292h
    ld hl, (l8d46h)
    inc hl
    ld (l8d46h), hl        ; 0x8D46 = frame counter (verified: ↑ per snap)
    bit 6, h               ; ≈ 16 384 frames timeout
    jp nz, 0x93F8
    jr l9282h
```

State addresses involved:

| Addr    | Role                          | Verified across snapshots |
|---------|-------------------------------|---------------------------|
| `0x8D46` | frame counter (16-bit)       | yes — different every snap (14971 / 5900 / 13849) |
| `0x8D48` | PRNG state                   | bumped each frame by `sub_8eb4h` |
| `0x8D4A` | PRNG read-pointer            | walks ROM/RAM, `AND H,0x9F` keeps it in 0..0x9FFF |
| `0xB7E5` | candidate menu state         | all zero in our 3 snapshots — *not* the state |

## Menu refresh: `sub_926bh`

```
sub_926bh:
    call sub_9231h         ; setup
    ld hl, 0xBF00          ; HL = live game state source
    ld de, 0x922E          ; DE = scratch / transform dest
    call sub_b61ch         ; HL→DE composer
    ld de, 0x8FD1          ; DE = render buffer
    ld b, 0x16             ; B = 22 rows
    jp sub_b796h           ; tail-call the text renderer
```

`sub_b796h` is the **text renderer** that walks the markup buffer (see
[encoding.md](encoding.md)) row-by-row.

## Main menu key handler: `0x9451`

Caught in snap2 (PC = 0x9451). Reads keyboard row `0xF7` (keys 1..5) via
`sub_97a7h`, then bit-tests the result:

| Key | bit tested | Action when set                            |
|-----|-----------|--------------------------------------------|
| 1   | bit 0      | `LD HL, 0x9571`; jump to common dispatch  |
| 2   | bit 1      | branch to `l94B1h`                         |
| 3   | bit 2      | `LD HL, 0x9592`; same dispatch             |
| …   | …          | …                                          |

`0x9571`, `0x9592` are sub-menu option config structs. State of which
option was last picked is held in `E` register loaded from `(0xB7E5)`
at entry, written back at exit — so `0xB7E5` *is* used as state, but
the snapshots happened to be at idle (default 0).

## Helper routines (referenced but not yet expanded)

| Address       | Inferred role                                |
|---------------|----------------------------------------------|
| `sub_8eb4h`   | PRNG seed update (uses `0x8D48` + `0x8D4A`)  |
| `sub_92a3h`   | 3-byte BCD/nibble unpack (rotates A right 4× to extract nibble) |
| `sub_9231h`   | menu setup pre-pass (called from `sub_926bh`) |
| `sub_97a7h`   | keyboard half-row read (`IN A,(0xFE)`, masked) |
| `sub_b796h`   | text renderer — walks markup buffer          |
| `sub_b61ch`   | live-state → render-buffer composer          |

## Player-indicator overlay (drawn outside the markup buffer)

After rendering the menu from the markup buffer (records 0x9571..0x961F),
two ~6×8-px glyph clusters remain unrendered when comparing to snap2:

- **playfield y=18..23**: non-bright white (`224,224,224`) glyph runs
  starting at `x≈16` (col 2). 8 cells worth of text.
- **playfield y=100..105**: bright white (`255,255,255`) glyph runs
  starting at `x≈40` (col 5). Right at the **KEYBOARD** row.

Neither y is matched by any record in `0x9000..0x9700`. These pixels
are painted by **`sub_b5f8h`** (the redraw helper the A/B handlers
call) directly to VRAM — *not* through the markup pipeline. That's
the **player-1 / player-2 indicator overlay**: digits or markers
showing which input device each player currently has selected.

When implementing in C:
- The overlay needs its own draw path (separate from `render_markup`).
- Position is per-player and per-current-state-byte.
- For the snap2 default (both players = KEYBOARD), the indicators
  cluster at the KEYBOARD row (y=100..105).

## Input-device state bytes (verified from disassembly)

A and B keys cycle each player's input device through 4 options
(KEYBOARD → KEMPSTON → CURSOR → INTERFACE II → wrap). Routines found
at `0x94BD` (A) and `0x9502` (B):

| Byte    | Cycled by | Range | Trigger                                |
|---------|-----------|-------|----------------------------------------|
| `0xB7EF` | A key     | 0..3  | `LD A, 0FDh; CALL sub_97a7h; RRA` — bit 0 = A |
| `0xB7F7` | B key     | 0..3  | `LD A, 07Fh; CALL sub_97a7h; AND 010h` — bit 4 = B |
| `0x938A` | debounce  | —     | decremented each frame; prevents auto-repeat |

After updating the byte each handler calls `sub_b5f8h` (the in-place
redraw routine) — pressing A or B just patches the one option label
on screen, not a full menu redraw.

All three current snapshots have these bytes = 0 (KEYBOARD/default),
so neither has yet exercised the A/B transition. A future
"after-A-press" snapshot would confirm.

## Selected-option blink — `sub_961c` at `0x961C`

Pressing 1 / 2 / 3 selects a play mode; the selected option's text
portion blinks white ↔ invisible. The mechanism is a per-frame
attribute-area rewrite (no hardware FLASH bit involved):

- `sub_961c` writes **11 attribute bytes** to the screen attribute area
  for the currently selected option's row, covering cols 14..24
  (= text after the "N - " prefix at cols 10..13). 11 cells is fixed
  width — wider than "1 PLAYER" / "2 PLAYERS", just enough to span
  "DOUBLE PLAY".
- The value comes from a 16-entry table at `0x9643`:
  `{0x00 ×8, 0x47 ×8}`. 8 phases of attr `0x00` (black ink on black
  paper = invisible) then 8 phases of attr `0x47` (bright white).
- Phase advances when `(frame_counter & 0x1F) == 0` — every 32 menu
  frames.
- The default green from the markup attr is **overwritten every frame**
  for the selected row; you never actually see green on a selected
  option.

### Boot default

`(0xB7E5) = 0` at menu entry, which maps to mode "1 PLAYER" in the
dispatch at `0x9494`. So the menu enters with "1 - 1 PLAYER" already
selected and blinking, before any key is pressed.

snap2 corroborates: row 5 (= "1 - 1 PLAYER", `Y=0x2F`) text cells
(`0x58AE..0x58B8`) are all `0x00`, captured during the BLACK half of
that initial blink. The "1 - " prefix at cols 10..13 keeps its markup
attr `0x44` (bright green) — `sub_961c` doesn't touch the prefix.

### Attribute-area layout, verified against snap2

For the three option rows in snap2 (cols 10..24):

| char_row | Content       | Y       | cells 10..13 | cells 14..24                 |
|----------|---------------|---------|--------------|------------------------------|
| 5        | 1 PLAYER      | 0x2F=47 | 0x44 (green) | 0x00 (BLACK blink half)       |
| 7        | 2 PLAYERS     | 0x3F=63 | 0x44 (green) | 0x44 (cols 14..22), 0x47 elsewhere |
| 9        | DOUBLE PLAY   | 0x4F=79 | 0x44 (green) | 0x44 (green)                  |

Char-row from `Y/8`. Gap rows 6 & 8 sit at 0x47 (white) but contain
no pixel bits, so they render as black paper — irrelevant to the
visible diff. The white attrs beyond text on row 7 (cols 23..24) are
likewise off-text and invisible.

Conclusion: nothing in snap2's attribute area contradicts the
"selected row blinks via `sub_961c`, everything else uses markup
attr" model. There's no separate "highlight neighbour" path.

## Round-intro window (`show_window_round_number`, $8F60) — coords are BOTTOM-anchored

The "PLAYER 1 / ROUND XX" window shown ~1.2s at each level entry
(drawn by `game_restart` after `game_screen_draw_to_buffer`, held by
`pause_long` -> `pause_short` busy-wait at $97D3, then erased by
`win_bg_recovery`). Two gotchas, both about the coordinate system:

- **`screen_addr_calc` ($B5xx) takes a pixel (x=L, y=H) coordinate**,
  not a packed ZX address — H is the literal pixel row. BUT the
  window and its text are drawn **UPWARD** from that row (the box loop
  uses `dec_scr_line`; `print_line` draws the glyph rows upward too).
  So **the coordinate byte is the BOTTOM row of what's drawn**, not
  the top.
  - Window: $A458 -> anchor (x=88, y=164), 32px tall drawn up = box
    occupies **y=133..164**.
  - PLAYER text `txt_player_x` = $8F60 -> (x=96, y=143 bottom); 6px ink
    lands at **y=138..143**.
  - ROUND text `txt_round_xx` = $9E60 -> (x=96, y=158 bottom); ink at
    **y=153..158**. Net: text vertically centred, 5px margin top, 6px
    bottom.

- **Port bug (fixed 2026-06-18, user-reported):** `show_round_banner`
  in `src/main.cpp` used the raw bytes 143 / 158 as the *top* Y of
  `draw_text` (which is top-anchored), so both lines sat 5px low —
  jammed against the box bottom (1px gap) instead of centred. Fix:
  pass `BORDER_Y+138` / `BORDER_Y+153`. Verified byte-exact against the
  original (text rows 138..143 / 153..158 in both).
  - `scripts/test_round_banner_border.py` had encoded the bug: its
    `TOP_BAND_H=8` assumed an 8px black top margin, but the real
    original only has **5** (text starts at 138). Corrected to 5.
  - Original capture for the diff:
    `scripts/capture_round_banner_original.py` (ZEsarUX: tape-load,
    tap "0" to start, read ULA $4000 while PC sits in the pause_short
    busy-wait — a breakpoint can't be used here, arming one parks the
    emulator in a cpu-step state where injected keys never register).


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
the complement a SET bit means PRESSED, and `AND $01 / RET NZ` is "key 0
is down". The Russian comment in the disassembly says so
("запуск игры, если нажат 0"); the instruction alone does not.

Bit 0 of the `$EFFE` row is the **0** key. There is no ENTER anywhere in
this routine.

The other exit is the attract timeout at the TOP of the loop
(`L93F8_0`): `counter_misc` is incremented and `BIT 6,H` jumps to
`disp_hs_table_and_wait_keys`. That is a free-running counter, not an
idle timer — pressing keys does not postpone it in the original, though
the port's `run_menu` does reset its own `last_input` on A/B/1-3.

### What the port does (2026-08-09)

`run_menu` returns `ST_LEVEL` on '0', which runs `new_game_reset` and
enters the round-1 banner. Before this it returned `ST_HISCORE` for
"0 / ENTER / other" under a comment saying it "would start a game".

ENTER is kept, deliberately, as the port's own attract-chain step. That
is not cosmetic: `test-visual` walks title -> menu -> hi-score -> level
by sending ENTER at each state. Had ENTER been made to start a game, its
`state3_hiscore` checkpoint would have captured a level instead — and
because it diffs each checkpoint against its OWN reference, the failure
would have read as a rendering regression on the wrong screen rather
than as a navigation change. `test-menu-start` pins both halves.

Modes 2 and 3 remain inert, so choosing one and pressing 0 starts a
1-player game (PLAN.md WS2/WS3).


## Two-player alternation, traced (2026-08-09)

WS2 stage 2 left one question open: the original stores only
`briks_quantity` per player, not the grid, so what does a returning
player's brick field look like? Answer: **the grid is preserved**, and
the routine that does it is not the one it looks like.

### `game_mode` is 0-based, and the port's `selected_mode` is not

    0  1 Player
    1  2 Players (alternating)
    2  Double Play

Read off `LBC10`: `CP $02 / CALL Z,print_txt_players_1_and_2` prints
"PLAYERS 1 AND 2" for mode 2, and the life-loss path's `LD A,(game_mode)
/ DEC A / CALL Z,...` runs the alternation for mode 1. The port's
`selected_mode` is 1..3 (`k - '0'` in `run_menu`), so any dispatch on it
has to subtract one. Worth having written down before WS2 stage 3 wires
it.

### The life-loss path

    LD A,(lives_1up) / DEC A / LD (lives_1up),A
    JR Z,LBC10_6                    ; out of lives -> the game-over path
    LD A,(game_mode) / DEC A
    CALL Z,current_level_2up_copier ; mode 1 only
    JP LB9E8_1                      ; per-level entry

### `current_level_2up_copier` falls through into `players_swap`

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

So one call does both halves of a turn change: the live grid is written
back into the departing player's level slot and the arriving player's is
loaded out of theirs, and then the counters swap. The level table doubles
as per-player grid storage — there is no separate save area.

The `RET Z` guard covers both halves too: with `lives_2up == 0` nothing
swaps, so a solo player keeps playing.

**This is why `players_swap` looked absent from the alternation.** It is
called by name only from `game_restart` and the game-over path; on an
ordinary life loss it is reached by fallthrough. Reading the two
routines separately makes 2-player mode look like it never changes turns.

### What it means for the port

`briks_quantity` per player is not enough — WS2 stage 3 needs a
per-player copy of the 180-cell grid, swapped with `live_level` at the
same moment the counters swap. That is a real chunk of state, which is
why stage 2 deliberately stopped short of guessing it.
