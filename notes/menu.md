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

