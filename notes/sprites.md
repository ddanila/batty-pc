# Sprite system — partial trace (Phase A in progress)

## What we know

### Shift table at `0xF200..0xFFFF` — boot-time, sub_6800

Built by the init routine starting at `0x6800` (the program entry). 8
output bytes per (input_byte × shift_count) pair, organised as 7
shift counts × 2 pages each:

```
6800: di
6801: ld sp, 0x6000
6804: out (0xfe), a       ; border colour 0
6807: ld hl, 0xf200       ; <-- table base
680a: ld b, 0x01          ; outer: shift count 1..7
680c:   ld c, 0x00        ; inner: byte value 0..255
680e:   ld d, c
        ld e, b
        xor a             ; A = shifted-out bits accumulator
6811:   srl d / rra / dec e / jr nz $-3   ; shift D right E bits;
                                          ; bits land in A
6817:   inc h / ld (hl), a / dec h        ; write low byte one page up
        ld (hl), d        ; write high byte
        inc hl / inc c / jr nz $-?        ; next byte value
        inc h ...                         ; next page (next shift)
```

For each `(byte, shift)`, the table stores `(byte >> shift, low_bits
<< (8-shift))` — the two halves of a sub-byte shifted byte that
needs to be OR'd into two consecutive VRAM bytes. Classic pre-shift
LUT for fast byte-aligned blits at sub-byte X offsets.

7 shift counts × 2 pages × 256 entries = 3584 B = exactly
`0xF200..0xFFFF`.

### Boot sprite pre-shifter — `sub_6853h`

Called once from `0x6825` immediately after the shift table builds.
Walks a recipe table at `0x68D7` (5 bytes per entry: src_lo, src_hi,
shifts, dst_lo, dst_hi). Terminator: src=0.

Recipe (4 entries):

| idx | src    | shifts | dst    |
|-----|--------|--------|--------|
| 0   | 0x7B16 | 255    | 0x7B48 |
| 1   | 0x7E38 |  16    | 0x7D4E |
| 2   | 0x7F42 |  16    | 0x828A |
| 3   | 0x8188 |  16    | 0x81F2 |

These pre-shift sprites used by the **menu/HUD layer** (matched by
`scripts/match_sprites_in_vram.py`). They are NOT what populates
`0xE400..0xF1FF`.

### Runtime cache at `0xE400..0xF1FF` — gameplay-only

Verified across snapshots:

| Snapshot          | Non-zero bytes / 3584 |
|-------------------|----------------------|
| snap1 hi-score    | 0                    |
| snap2 main menu   | 0                    |
| snap3 level 1     | 3534 (98.6%)         |

So `0xE400..0xF1FF` is populated only during gameplay. Builder
unidentified yet — not the boot-time `sub_6853h` path (none of its
4 recipe entries write there). Likely a level-start routine builds
it from level data.

### Game start = '0' key in the menu poll

The menu polling loop at `l9282h` reads keyboard row `0xEF` (keys
0/9/8/7/6) and immediately does `ret c` if bit 0 (= key '0') is set.
So pressing '0' RETURNS from `sub_926bh` to its caller — that's
where game-start happens.

The relevant caller is at PC `0x91a1`:

```
9184: ld bc, 0x0006 / ldir       ; copy 6 bytes (initial state)
918b..0x919a: set up IX-relative state
919b: call sub_97adh
919e: call sub_97bch
91a1: call sub_926bh             ; <-- menu poll; returns on '0'
91a4: ld a, (lb7e6h) / inc a / ld (0x90f5), a
91ab: ld de, 0x90e8 / call lb4ech (twice)
91b4: ... IX manipulation ...
91be: ld b, 5 / l91c0h: ld c, 0x0a / push bc
91c3: call sub_a1dbh             ; <-- inner per-frame game routine
91c6..0x91d4: check 0x8ED9 bits → loop back or exit
```

`sub_a1dbh` is the per-frame game body — read input, update state,
render. Next step: walk it and find what reads from `0xE400..0xF1FF`
(= the blitter) and what writes to it (= the cache builder).

There are TWO other callers of `sub_926bh` (at 0x9221 and 0x927f),
suggesting the menu loop is re-entered during gameplay too — for
between-level transitions or pause? TBD.

## What we don't know yet (Phase A2+)

- Cache format inside `0xE400..0xF1FF`. `scripts/render_sprite_cache.py`
  has been rendering at multiple byte-widths to eyeball it; once we
  identify the builder we'll know the row-stride directly.
- The actual blitter (cache → VRAM 0x4000..0x57FF). Most likely
  inside `sub_a1dbh`'s call tree.
- Sprite metadata: position/velocity tables for bat, ball, bricks.
- How the cache is *re-used* across frames: rebuilt every frame, only
  on level start, or partially patched as sprites move?

## Tools we already have

- `scripts/render_sprite_cache.py` — renders the 3.5 KB cache at
  widths 1..8 px. Run on snap3:
  `python3 scripts/render_sprite_cache.py build/snapshots/20260513T202101Z/ram_4000_FFFF.bin`
- `scripts/match_sprites_in_vram.py` — fuzzes raw + pre-shifted
  sprite sources against VRAM to locate sprite draw positions.
- `scripts/render_field_cache.py` — adjacent purpose; named separately
  so probably for the level-data / brick layout cache. TBD.

## Next concrete steps

1. **Walk `sub_a1dbh`**. Identify the main per-frame structure.
2. **Watchpoint on cache writes**. Live in ZEsarUX, navigate to
   level-1 start, break on writes to `0xE401`. PC at trap = cache
   builder.
3. **Watchpoint on cache reads**. Same setup, but break on read.
   PC at trap = (somewhere in) the blitter call chain.
4. Decode cache layout once we see how it's written.
