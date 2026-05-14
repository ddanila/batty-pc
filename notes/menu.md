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
so neither has yet exercised the dynamic transition. A future
"after-A-press" snapshot would confirm.

## Unresolved

- The hi-score table source (where the "current" entries live before
  `sub_926bh` composes them into the render buffer at `0x8FD1`). The
  source pointer is `0xBF00` but the format there hasn't been parsed
  yet.
- The 1 unknown branch (`jp (hl)`) at `0xC0B7` from the trace tool —
  almost certainly a dispatch table somewhere in the menu's choose-option
  chain.
