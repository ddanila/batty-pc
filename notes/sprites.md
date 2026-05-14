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

## Phase A1 milestone — the screen sprite blitter

Live trace via `scripts/trace_blitter.py` (loads snap3 as a `.sna`,
sets `MRA=E800H` watchpoint, resumes the CPU) trapped at PC 0xB775 —
the `di` ending a double-`halt` frame-sync immediately followed by
`call sub_ad8fh`. That call's body is the brick-field blitter:

### `sub_adbch` at 0xADBC — innermost 16×8 sprite blit

```
adbc: bit 7, (iy+0) / ret nz       ; bit 7 of descriptor = end / skip
adc1: bit 4, (iy+0) / ret nz       ; bit 4 = transparent / skip
adc6: ld e, (ix+0) / ld d, (ix+1)  ; DE = source pointer pulled from
                                   ; (IX) — a 2-byte handle into the
                                   ; pre-shifted sprite cache
adcc: ld (0xADDE), sp              ; SMC patch: save SP into the
                                   ; `ld sp, 0` at 0xADDD's operand
add0: ex de, hl / ld sp, hl / ex de, hl   ; SP := source (= use pop
                                          ; for fast 2-byte-aligned reads)
add3: ld b, 8                      ; 8 pixel-rows per chunk
add5: pop de                       ; DE = 2 source bytes (SP += 2)
      ld (hl), e / inc l           ; col N
      ld (hl), d / dec l           ; col N+1
      inc h                        ; next pixel-row WITHIN the char cell
                                   ; (= +0x100 in ZX VRAM addressing)
      djnz add5                    ; 8 rows
addd: ld sp, 0x0000                ; restored from SMC at 0xADCC
ade0: ret
```

So each call paints **16 px × 8 px = a 2-cell-wide, 1-cell-tall
rectangle** of VRAM, sourced from a pre-shifted sprite bitmap in the
cache. `IX` walks a table of source-pointer pairs. `IY` walks a
parallel descriptor list with skip / end flags (bits 4 and 7).

### `sub_ad8fh` at 0xAD8F — the brick-field wrapper

```
ad8f: ld iy, (l9789h)              ; live brick descriptor list
ad93: ld hl, 0x4081                ; VRAM start = pixel (8, 16)
ad96: ld b, 12                     ; 12 outer rows
ad98: push bc / push hl
ad9a: call sub_adach               ; one row = 15 columns
ad9d: pop hl
ad9e: ld a, 0x20 / add a, l        ; HL += 0x20 (next ZX pixel-row
ada0: ld l, a / jr nc, ...         ; in the same screen-third), with
ada4: ld a, 0x08 / add a, h        ; wrap into the next third when l
ada7: ld h, a                      ; overflows
ada8: pop bc
ada9: djnz ad98 / ret              ; 12 rows
```

`sub_adach` is the inner 15-col walker, calling `sub_adbch` 15
times and advancing HL by +2 each col + IY by +1.

So the brick-field area covered by this routine:
- **VRAM origin** `0x4081` = pixel `(8, 16)`
- **Width**   = 15 × 2 bytes = **240 px**
- **Height**  = 12 × 8 px    =  **96 px**
- That's a 240 × 96 rectangle in the **top half of the playfield** —
  exactly where the bricks live in Batty.

### Frame-sync patterns

Two distinct frame-sync points found:

| Address | Pattern              | Meaning                      |
|---------|----------------------|------------------------------|
| `0xBB37`/`0xBB38` | `halt / di`     | Single-frame wait — main 50 Hz tick |
| `0xB771..0xB775`  | `ei/halt/ei/halt/di` | Two-frame wait — sub_ad8fh's outer cadence |

So the brick-field redraw via sub_ad8fh runs every **2 frames** =
25 Hz, not 50 Hz. Sprite (bat / ball) blits via sub_adbch likely run
every frame from a different caller.

### Cache layout — implications

3584 B cache at `0xE400..0xF1FF`, blitter consumes 16 B per sprite
chunk. With 7 pre-shifted copies per logical sprite, each takes 112
B → cache fits ~32 logical sprites. Or 16×8 = 128 B (incl. shift 0)
→ 28 sprites. Decode TBD: dump the cache from snap3 and visually
inspect at width 2 bytes (= 16 px wide blocks).

### Not yet found

- The **cache builder** — what fills `0xE400..0xF1FF` at level start.
  No write watchpoint trapped on first attempt; needs follow-up
  (likely runs ONCE during the level-init transition, not per frame).
- The **bat / ball / power-up blitters**. Probably also call
  `sub_adbch` but from different higher-level wrappers tied to
  per-entity state. Trace 0xBB39+ (after the main frame-sync) to map.
