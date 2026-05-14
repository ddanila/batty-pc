# Level data — static, fully decoded

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

## Cache mapping

Naive `chunk_offset = cell_value * 16` works! Verified by rendering
all 15 levels through this lookup against snap3's cache
(`assets/levels/all_levels.png`):

- L1: sparse opener (matches snap3)
- L2: arch
- L3: "S5"-shape
- L6: maze-text
- L10: nested concentric rectangles
- **L15: literally spells "ELITE"** (the publisher's name)

Implications:
- Cache slots `0..47` (= 8 brick types × 6 slots, sparse) hold the
  base brick sprites at slot-id = brick-type-id.
- The 8 brick base sprites are populated at game-start, not per
  level (snap3 was on L1 yet the cache has the L15-only
  `0x2B..0x2E` slots fully populated).
- The remaining 224 - 8 = 216 cache chunks hold pre-shifted
  variants, the bat, ball, status, power-ups, etc. Phase A2.5
  TBD.

## Renderer

`scripts/render_levels.py` produces:
- `assets/levels/level01.png` .. `level15.png` (240×96 each)
- `assets/levels/all_levels.png` (3×5 grid, 3× zoom)

Pure-Python; uses the snap3 cache as the sprite source.

## What this unlocks

- **Ship as data**: dump the 2700 B and 30 B pointer table → drop
  into the DOS port as `assets/levels.bin`. Pixel-identical level
  rendering possible immediately, no Z80 runtime needed.
- **Validation ground truth**: kspatching the original (`(0xB7EA)`
  := N, jump to level-init, snapshot) gives per-level reference
  PNGs at 1:1 ZX colour fidelity — drives regression tests for
  every level once we wire it in.
- **Brick palette extracted**: 8 chunks at slots 6, 7, 8, 9, 0x2B,
  0x2C, 0x2D, 0x2E in snap3's cache = the 8 brick sprite types.
