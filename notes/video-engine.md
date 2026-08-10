# The video engine (`src/zxvga.cpp`)

The ZX Spectrum display model — and its defining artefact, colour clash —
lives in `src/zxvga.{cpp,h}`. Game content (bricks, bat, HUD, background
tiles, level assets) stays in `main.cpp` and talks to the engine only through
`scr_buff` / `attr_buff` and the dirty marks. The pipeline those two planes
sit in is `notes/blitter-port.md`.

## Why clash is a feature here

The Spectrum stores colour at 1/64th of pixel resolution: 256x192 1-bit
pixels in one plane, 32x24 attribute bytes in another, so every pixel in an
8x8 cell must be that cell's ink or its paper. Batty leans on this — a bird
flying over the brick band renders in the BRICKS' colours, because the
original's `print_obj_to_buff` ($B82C) blits pixels and never touches
attributes.

So the port reproduces the model rather than rendering sprites in their "own"
colour. Two functions carry the split:

| | writes pixels | writes attrs |
|---|---|---|
| `blit_masked_to_scr_buff` | yes | **never** — this is what clashes |
| `blit_sprite_attrs_to_buff_clipped` | no | yes, in whole 8x8 cells |

The "never" is the invariant that known-bugs #7 and the enemy fly-over
residue were both violations of. It is gated three ways:
`test-sprite-attr-parity` and `test-enemy-attr-parity` (QEMU, against the
original's behaviour) and `make test-video` (host, exhaustive).

**Flash is not emulated.** The expansion table is indexed by `attr & 0x7F`,
dropping bit 7, so a flashing attribute renders as its steady state. The
game's own blink effects work by rewriting attributes instead
(`notes/menu.md`).

## Layout and API

`src/zxvga.cpp` is one file in six sections: the VGA surface (mode set, DAC
palette upload, rect fill); the attribute model (attr byte -> ink/paper ->
the expansion table); the ZX framebuffer; the clash blit to VGA
(`buff_to_vga`, `buff_to_vga_rect_bytes`); dirty rectangles
(`mark_dirty_*`, `flush_dirty_to_vga`); and the two masked blits.

`zxvga.h` declares the whole API — 28 symbols: the two planes, the blits,
the dirty marks, `ZxDisplay`, and `vga` itself. Everything else (the palette,
the mode and DAC setters, the expansion table, `flush_dirty_slot_to_vga`) is
private, and the **linker** enforces that rather than convention doing it.

## Style — the reference for the rest of the port

- **`ZX_STATIC_ASSERT` for anything the comments used to merely claim** —
  buffer sizes against the geometry, `BYTES_PER_ROW == ATTR_COLS`, the blit's
  4-byte alignment. Change `BORDER_X` to an odd multiple and the BUILD fails
  instead of the screen corrupting at runtime.
- **Named layout accessors** (`scr_row`, `attr_row`, `vga_at`) so the
  addressing arithmetic is written once.
- **`Sprite`** wraps the raw `[w][h][(mask,pixel)...]` blob, so call sites
  read `sprite.height()` rather than `src[1]`. It converts implicitly from
  `const u8 *`, which retired the old `_ptr`-suffixed twin of every blit —
  one name, two overloads.
- **RAII for paired operations.** `ZxDisplay` sets mode 13h and the palette
  on construction and restores text mode on destruction, so no return path
  can leave the display in a graphics mode.
- **`inline` functions instead of function-like macros** (`emit_byte`,
  `apply_mask`, `attr_ink`, `attr_paper`).
- **`u8` / `u32` aliases** in anything the host build also compiles.
  Watcom's 32-bit `long` is 4 bytes and a 64-bit host's is 8, so a cast
  through the wrong one silently doubles a store's width. That bug happened;
  `make test-video` caught it.

`static_assert` needs `-zastd=c++0x` and the host build is strict C++98, so
`ZX_STATIC_ASSERT` falls back to a negative-array-size typedef there
(`notes/toolchain.md`).

## Host-native tests: `make test-video`

A single `#ifdef __WATCOMC__` picks the VGA surface — real hardware, or a
plain array. So `tests/test_zxvga.cpp` compiles the **real engine source**
natively, the shipping expansion table and blit logic rather than a model of
them, and runs in milliseconds instead of a 10 s QEMU boot. That buys
exhaustive coverage:

- every one of the 256 attributes -> ink/paper;
- every (attribute, pixel byte) pair against an **independent ULA
  reference** written from the hardware spec — deliberately not reusing
  `attr_ink` / `attr_paper`, or a bug would agree with itself;
- `attr` and `attr | 0x80` render identically (flash not emulated);
- rect flush == full repaint inside the rect, no-op outside it;
- dirty flush == full repaint over random edit sets — the contract every
  frame depends on;
- pixel blits never touch `attr_buff`, including from off-playfield
  positions where clipping runs;
- after arbitrary blitting, **every** pixel in a cell is that cell's ink or
  paper and nothing else — the positive form of the same invariant;
- attribute writes take whole cells, and clip correctly;
- a real 6912-byte screen from the original (`original/Batty.scr`) expands
  to the expected pixels.

It needs no emulator, so unlike the QEMU gates it is viable on hosted CI.

## Verifying that a code-motion refactor changed nothing

Worth reusing for any future extraction of this kind: the preprocessed token
multiset (commands in `notes/toolchain.md`) is **empty** for pure code motion,
which proves no statement was added, dropped or altered; EXE size is a
cross-check; `make test` plus `make parity-check-parallel` is the third leg.

The EXEs are **not** byte-identical and cannot be — moving a function shifts
every address after it. A clean rebuild at a fixed revision *is*
byte-identical, since only `build/main.obj` carries two timestamp bytes, so EXE
comparison is a valid tool whenever the source order is untouched.
