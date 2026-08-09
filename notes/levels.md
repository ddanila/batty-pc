# Level data — pointer table, layout, cell vocabulary

Decoding reference for the 15 levels at
`original/blocks/03_DATA_headless.dat.bin` (= our
`assets/levels.bin`, 2700 B = 15 levels × 180 B). The runtime
brick collision + rendering logic lives in `src/main.cpp`
(`brick_collision`, `print_one_brik_buf_c`) and is documented in
[`blitter-port.md`](blitter-port.md).

## Pointer table at `0x6CBD`

Array of 15 little-endian 16-bit pointers. Indexed by `(0xB7EA)`
(the level number, 0-based), accessed by `sub_9779h`:

```
sub_9779h:                 ; A = (0xB7EA) on entry
    ld hl, 0x6CBD
    add a, a               ; index *= 2
    ld e, a / ld d, 0
    add hl, de             ; HL = &table[A]
    ld e, (hl) / inc hl / ld d, (hl)   ; DE = *HL
    ex de, hl
    ld (l9789h), hl        ; install as live brick-list pointer
    ret
```

Result installed at `(0x9789)`, which is what `ld iy, (0x9789)` in
`sub_ad8fh` reads as the **brick descriptor list IY** for the
per-frame brick blit.

## The pointers

| Level | Pointer  |
|-------|----------|
|  1    | `0x6CDB` |
|  2    | `0x6D8F` |
|  3    | `0x6E43` |
|  4    | `0x6EF7` |
|  5    | `0x6FAB` |
|  6    | `0x705F` |
|  7    | `0x7113` |
|  8    | `0x71C7` |
|  9    | `0x727B` |
| 10    | `0x732F` |
| 11    | `0x73E3` |
| 12    | `0x7497` |
| 13    | `0x754B` |
| 14    | `0x75FF` |
| 15    | `0x76B3` |

Delta between consecutive levels = exactly `0xB4` = 180 bytes. Total
level-data span: `0x6CDB..0x7766` = **2700 B = 15 × 180**.

## Cell format

Each level is **180 raw bytes**, one byte per cell in a **12-row ×
15-col** grid (row-major). Per cell:

- `bit 7` (`0x80`) — no brick / destroyed at runtime (skip rendering)
- `bit 6` (`0x40`) — present only as `0xC0` (= bit 7 + 6) = "empty
  sentinel" (never had a brick; level_attrs still applies a strip
  attr there).
- `bit 5` (`0x20`) — undestructible (renders, bounces, no destruction
  — e.g. `$2B`, `$2C`, `$2D`, `$2E`).
- `bit 4` (`0x10`) — "this hit destroys": 1-hit destructible OR a
  multi-hit brick already damaged once and primed for the next hit
  to take it out (e.g. `$11`..`$15`).
- low nibble (`0x0F`) — brick TYPE / COLOUR index into
  `briks_colors[]` (`src/main.cpp`).

Cells with bit 7 set are skipped during rendering. All others get
blitted as a 16×8-px sprite at `(1 + 2*col byte_col, 4 + row char_row)`
via `print_one_brik_buf_c`.

## Cell-value vocabulary (across all 15 levels, 2700 cells)

| Value | Count | Pct   | Kind                                   |
|-------|-------|-------|----------------------------------------|
| `$C0` | 1494  | 55.3% | empty sentinel (bit 7+6)               |
| `$15` |  178  |  6.6% | brick `$15` → briks_colors[5] (= `$70`) |
| `$13` |   90  |  3.3% | brick `$13` → briks_colors[3] (= `$5F`) |
| `$12` |   86  |  3.2% | brick `$12` → briks_colors[2] (= `$4F`) |
| `$14` |   67  |  2.5% | brick `$14` → briks_colors[4] (= `$20`) |
| `$11` |   54  |  2.0% | brick `$11` → briks_colors[1] (= `$57`) |
| `$07` |  216  |  8.0% | hard `$07` → briks_colors[7] (= `$57`) |
| `$06` |  109  |  4.0% | hard `$06` → briks_colors[6] (= `$47`) |
| `$09` |  109  |  4.0% | hard `$09` → briks_colors[9] (= `$4F`) |
| `$2E` |   92  |  3.4% | undestructible `$2E` → briks_colors[14] (= `$5F`) |
| `$2B` |   93  |  3.4% | undestructible `$2B` → briks_colors[11] (= `$47`) |
| `$08` |   50  |  1.9% | hard `$08` → briks_colors[8] (= `$5F`) |
| `$2C` |   54  |  2.0% | undestructible `$2C` → briks_colors[12] (= `$57`) |
| `$2D` |    8  |  0.3% | undestructible `$2D` → briks_colors[13] (= `$4F`) |

Three categories:
- `$11..$15` (low nibble 1..5, bit 4 set) = **1-hit destructible**.
- `$06..$09` (low nibble 6..9, bit 4 clear) = **multi-hit** (= 2 hits
  to destroy; first hit sets bit 4 → matches `$16..$19` → next hit
  destroys).
- `$2B..$2E` (low nibble 11..14, bit 5 set) = **undestructible metal**.
- `$C0` = empty (no brick here in this level layout).


## How much of `level_attrs.bin` is actually data (2026-08-09)

`assets/level_attrs.bin` is 15 x 768 = 11520 bytes of ZX attribute
cells, extracted from emulator captures. PLAN.md WS7 wants it gone.
Measured, before porting any of the writer:

| region | bytes | share | status |
|---|---|---|---|
| live-brick cells | 2412 | 20.9% | **derivable exactly** |
| rest of the brick zone (empty cells) | 2988 | 26.0% | side-strip / bg colours, not yet derived |
| HUD rows, side strips, bottom | 6120 | 53.1% | not yet derived |

The derivation for the first row of that table is two rules:

    attr = briks_colors[cell & 0x0F]      both halves of the cell
    attr &= 0xBF  at char column 1        the border's drop shadow

`briks_colors` is the 16-byte table at `$AEEC`. The second rule is
`print_border_shadow` ($BFCF), which `game_screen_draw_to_buffer` calls
AFTER `print_briks`:

    print_border_shadow:
      LD HL,attr_buff+$21 / LD B,$17 / LD DE,$0020
      LBFCF_0: RES 6,(HL) / ADD HL,DE / DJNZ LBFCF_0   ; col 1, rows 1..23
      LD HL,attr_buff+$22 / LD B,$1D
      LBFCF_1: RES 6,(HL) / INC L / DJNZ LBFCF_1       ; row 1, cols 2..30

Running after the bricks is the whole point: a brick in field column 0
gets the bright bit taken back off its LEFT char. Without that rule the
match is 2374/2412, and every one of the 38 misses differs by exactly
$40. With it, 2412/2412.

I went looking for that discrepancy expecting a runtime bug — that
`repaint_row_attrs` re-brightens char column 1 and loses the shadow.
It does write there, but `render_brick_band_rows` calls
`dim_border_shadow_column(cr0, cr1)` afterwards, and `cr1` is exactly
row `r1+1`'s cell row, so the repaint is always covered. No bug; the
port had `print_border_shadow_c` all along.

`test-level-attrs-derivable` locks this in. It is not only WS7 evidence:
the extraction is manual and off the build graph (see the Makefile note
on `frame_l1.bin`'s twin problem), so nothing else would notice a
re-extraction that no longer matches `assets/levels.bin`.
