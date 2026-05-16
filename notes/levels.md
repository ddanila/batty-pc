# Level data — pointer table, layout, cell vocabulary

Decoding reference for the 15 levels at
`original/blocks/03_DATA_headless.dat.bin` (= our
`assets/levels.bin`, 2700 B = 15 levels × 180 B). The runtime
brick collision + rendering logic lives in `src/main.c`
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

- `bit 7` (`0x80`) — skip / end (from `sub_adbch:adbc`)
- `bit 4` (`0x10`) — skip (from `sub_adbch:adc1`)
- the remaining 6 bits encode the **brick type id**

Cells where `(byte & 0x90) != 0` render as background (paper). All
others get blitted as a 16×8-px sprite at `(col*16, row*8)`.

## Cell-value vocabulary (across all 15 levels, 2700 cells)

| Value | Count | Pct  | Kind       |
|-------|-------|------|------------|
| `0xC0` | 1494 | 55.3% | skip / empty (`bit 7|6`) |
| `0x15` |  178 |  6.6% | skip (`bit 4` set) |
| `0x13` |   90 |  3.3% | skip |
| `0x12` |   86 |  3.2% | skip |
| `0x14` |   67 |  2.5% | skip |
| `0x11` |   54 |  2.0% | skip |
| `0x07` |  216 |  8.0% | brick type 7 |
| `0x06` |  109 |  4.0% | brick type 6 |
| `0x09` |  109 |  4.0% | brick type 9 |
| `0x2E` |   92 |  3.4% | brick type 0x2E |
| `0x2B` |   93 |  3.4% | brick type 0x2B |
| `0x08` |   50 |  1.9% | brick type 8 |
| `0x2C` |   54 |  2.0% | brick type 0x2C |
| `0x2D` |    8 |  0.3% | brick type 0x2D |

**8 distinct brick types** — `{0x06, 0x07, 0x08, 0x09, 0x2B, 0x2C,
0x2D, 0x2E}`. The 6 "skip" variants probably encode background
metadata (sound on hit? something else?) — visually all transparent.
