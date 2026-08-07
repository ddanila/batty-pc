# The video engine (`src/zxvga.c`)

The ZX Spectrum display model — and its defining artefact, colour clash —
lives in `src/zxvga.{c,h}`, extracted from `src/main.c` on 2026-08-07.
Game content (bricks, bat, HUD, background tiles, level assets) stays in
`main.c` and talks to the engine only through `scr_buff` / `attr_buff`
and the dirty marks.

## Why clash is a feature here, not a bug

The Spectrum stores colour at 1/64th of pixel resolution: 256x192 1-bit
pixels in one plane, 32x24 attribute bytes in another. Every pixel in an
8x8 cell must be either that cell's ink or that cell's paper. Batty leans
on this everywhere — a bird flying over the brick band renders in the
BRICKS' colours, because the original's `print_obj_to_buff` ($B82C) blits
pixels and never touches attributes.

So the port reproduces the model rather than rendering sprites in their
"own" colour. Two functions carry that split:

| | writes pixels | writes attrs |
|---|---|---|
| `blit_masked_to_scr_buff_ptr` | yes | **never** — this is what clashes |
| `blit_sprite_attrs_to_buff_clipped` | no | yes, in whole 8x8 cells |

The "never" is the invariant that known-bugs #7 and the enemy fly-over
residue (#2) were both violations of. It is now gated three ways:
`test-sprite-attr-parity` and `test-enemy-attr-parity` (QEMU, against the
original's behaviour) and `make test-video` (host, exhaustive).

## Layout

`src/zxvga.c` is one file in six sections:

1. **VGA surface** — mode set, DAC palette upload, rectangle fill
2. **Attribute model** — attr byte -> ink/paper -> the expansion table
3. **ZX framebuffer** — `scr_buff` (6144 B) / `attr_buff` (768 B)
4. **Clash blit to VGA** — `buff_to_vga`, `buff_to_vga_rect_bytes`
5. **Dirty rectangles** — `mark_dirty_*`, `flush_dirty_to_vga`
6. **Masked blits** — the two above

**Flash is not emulated.** The expansion table is indexed by `attr & 0x7F`,
dropping bit 7, so a flashing attribute renders as its steady state. The
game's own blink effects work by rewriting attributes instead.

## It is #included, not linked

`main.c` does `#include "zxvga.c"`, so the two are one translation unit and
every symbol stays `static`. The Makefile lists `src/zxvga.c` in `HEADERS`
so edits trigger a rebuild.

Making it a separately-compiled `.obj` would mean promoting ~30 statics to
externals. Not done yet — the tests below make it a cheap change whenever
an enforced boundary is wanted.

## Host-native tests: `make test-video`

A single `#ifdef __WATCOMC__` picks the VGA surface — real hardware, or a
plain array. So `tests/test_zxvga.c` compiles the **real engine source**
natively, the shipping expansion table and blit logic rather than a model
of them, and runs in milliseconds instead of a 10 s QEMU boot. That buys
exhaustive coverage:

- every one of the 256 attributes -> ink/paper
- every (attribute, pixel byte) pair vs an **independent ULA reference**
  written from the hardware spec (deliberately not reusing `ink_pal` /
  `paper_pal`, or a bug would agree with itself)
- `attr` and `attr | 0x80` render identically (flash not emulated)
- rect flush == full repaint inside the rect, no-op outside it
- dirty flush == full repaint, over random edit sets — the contract every
  frame depends on
- pixel blits never touch `attr_buff` (the clash invariant), including
  from off-playfield positions where clipping runs
- after arbitrary pixel blitting, **every** pixel in a cell is that cell's
  ink or paper and nothing else — the positive form of the same invariant
- attribute writes take whole cells, and clip correctly
- a real 6912-byte screen from the original game (`original/Batty.scr`)
  expands to the expected pixels

Wired into `make parity-check`. It needs no emulator, so unlike the QEMU
gates it is viable on hosted CI (see testing.md on why the QEMU suite is
not).

## Verifying the extraction changed nothing

Worth reusing for any future code-motion refactor of this file:

1. **Preprocessed token multiset.** Preprocess both revisions, strip blank
   lines, normalise whitespace, `sort`, `diff`. For pure code motion this
   is **empty** — it was, for every build variant. That proves no statement
   was added, dropped, or altered; only order changed, which is
   semantically neutral for file-scope definitions with constant
   initialisers. See `notes/toolchain.md` for the exact commands.
2. **EXE size**, as a cross-check on the above.
3. **The gates.** `make test` (5 states, pixel-identical) plus
   `make parity-check-parallel`.

Note the EXEs are **not** byte-identical, and can't be: moving a function
shifts every address after it. Size + token multiset + gates is the
achievable proof. (A clean rebuild at a fixed revision *is* byte-identical
— only `build/main.obj` carries 2 timestamp bytes — so EXE comparison is a
valid tool whenever the source order is untouched.)
