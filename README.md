# batty-pc

MS-DOS / 8086 re-creation of **Batty** (Elite Software / Hit-Pak, 1987,
ZX Spectrum 48K). VGA mode 13h (320×200×256), 1:1 pixel-faithful inside
the 256×192 playfield, border around it. Built with Open Watcom v2,
boots in QEMU.

The toolchain pattern mirrors two sibling projects:

- [`generaly`](https://github.com/ddanila/generaly) — 1:1 Z80 reimpl
  of a different Speccy game.
- [`adlib-rng`](https://github.com/ddanila/adlib-rng) — Open Watcom
  + `mtools` + QEMU + AdLib boilerplate for 16-bit DOS work.

## Status

Hi-score screen is now reproduced by *our* C renderer pixel-for-pixel
against the original — driven from the same byte-level markup buffer
the original walks, using the same 43-glyph font we extracted from the
tape blob. `make test` headlessly diffs three rendered screens
against ZEsarUX snapshots and currently passes pixel-identical:

```
PASS state1_hiscore_static   (snap1 blit          → snap1 reference)
PASS state2_hiscore_rendered (markup + font → C  → snap1 reference)
PASS state3_mainmenu_static  (snap2 blit          → snap2 reference)
```

| Stage                                | State |
|--------------------------------------|-------|
| Mode-13h toolchain + QEMU            | ✓ working |
| `.SCR` loading-screen decoder        | ✓ shipped |
| TAP parser + block extraction        | ✓ shipped |
| z80dasm-based recursive trace        | ✓ shipped |
| Snapshot diff (3 snapshots)          | ✓ shipped |
| Init chain decoded                   | ✓ `notes/init.md` |
| Menu refresh chain                   | ✓ `notes/menu.md` |
| Text/markup encoding cracked         | ✓ `notes/encoding.md` |
| 43-glyph font extracted              | ✓ `assets/font.bin` |
| Sprite-data base address found       | ✓ `0x6B13` (right after the font) |
| Hi-score C renderer (markup-driven)  | ✓ shipped — pixel-identical |
| Main menu (Phase A static blit)      | ✓ shipped |
| Headless visual regression test      | ✓ `make test` (`notes/testing.md`) |
| Main menu Phase B (markup-driven)    | open |
| Sprite blitter (the real game one)   | open |
| Game logic / main loop / dynamic     | open |
| Replay-driven gameplay testing       | open |

## Quick build / run

```sh
brew install mtools qemu z80dasm srecord
# vendor/ is symlinked to the sibling repos (generaly, adlib-rng) —
# see "Toolchain" below if you don't have them.
make floppy       # builds build/batty.exe + LOADING.BIN, packs floppy
make run          # boots the recreation in QEMU
```

For the reverse-engineering side:

```sh
make regions      # static signature scan of the main blob
make candidates   # render byte regions as PNG strips (assets/candidates/)
make run-original # boot the original .tap in ZEsarUX (ZRCP on :10000)
make snapshot     # dump RAM+screen from running ZEsarUX
make test         # headless visual regression vs ZEsarUX snapshots
```

See [`scripts/`](scripts/) for the rest (TAP parser, basic detokenizer,
recursive-descent trace, snapshot analyzer, sprite renderer, VRAM
reverse-search).

## Repo layout

```
src/                Our re-implementation (C, Open Watcom v2 16-bit)
  main.c            mode 13h init + loading-screen blit
scripts/            Python tooling (asset extraction + RE + tests)
  extract_scr.py        ZX .SCR → flat 8bpp bitmap
  extract_tap.py        TAP container → per-block dumps
  extract_font.py       Pull 43-glyph font from 0x6A15 → assets/font.bin
  detokenize_basic.py   ZX BASIC detokenizer
  scan_regions.py       Static data/code signature scan
  trace_entry.py        Recursive-descent reachability walker
  render_candidates.py  Sprite-candidate PNG renderer
  analyze_snapshots.py  RAM diff across snapshots (tape/static/dynamic)
  vram_back_to_ram.py   Reverse-search VRAM → RAM source addresses
  render_field_cache.py De-interleave 0xE400 region as 256×112 PNG
  render_sprites.py     Render 4 sprite regions identified via sub_6853h
  match_sprites_in_vram.py  Match sprite bytes to VRAM snapshots
  find_font.py          Recover font from displayed glyphs via VRAM
  snapshot_ram.py       ZEsarUX ZRCP snapshot capture
  scan_text.py          ASCII/text scan across RAM
  test_visual.py        Headless QEMU + pixel-diff regression test
  zrcp.py               (symlink) ZEsarUX remote-control client
notes/              Reverse engineering findings — read these first
  init.md           Tape loader, shift table at 0xF200, sub_6853h, XOR unpacker
  menu.md           Menu polling loop, sub_926bh, state addrs
  encoding.md       Char/markup encoding, 43-glyph font, sprite-base @ 0x6B13
  testing.md        How `make test` works + what it does (and doesn't) cover
original/           The original game (see "Original assets" below)
  batty.tap, BATTY.TZX  Tape images
  Batty.scr             Loading-screen framebuffer
  Batty.txt             Instructions card (text)
  blocks/               TAP-decoded per-block dumps + 03_main.asm
assets/             Extracted / derived assets
  loading.bin           Decoded loading screen (49 152 B, 256×192×8bpp)
  candidates/           Per-region PNG sprite candidates
  sprites_v2/           Sprite region renders with correct width×height
  sprite_cache/         0xE400..0xF1FF strips (runtime cache, multi-width)
  field_snap*.png       De-interleaved field region from each snapshot
build/              Build outputs + analysis (most gitignored)
  snapshots/            Captured RAM + screen + register state from ZEsarUX
vendor/             Symlinks: openwatcom-v2 → adlib-rng, msdos → adlib-rng,
                            (zrcp.py in scripts/ → generaly)
```

## Toolchain

The build needs **Open Watcom v2** (16-bit, 8086) and an **MS-DOS 4.0
boot floppy** for QEMU. Both are vendored in
[`adlib-rng`](https://github.com/ddanila/adlib-rng); this repo's
`vendor/` is just symlinks into that sibling checkout so we don't
duplicate ~100 MB. If you don't have `adlib-rng` next to this checkout,
either clone it or copy `vendor/openwatcom-v2/` and `vendor/msdos/` from
its `vendor/` into this repo's `vendor/`.

ZEsarUX (the Speccy emulator used for `make run-original` /
`make snapshot`) is vendored similarly in
[`generaly`](https://github.com/ddanila/generaly).

## Approach

The plan is the same one [`generaly`](https://github.com/ddanila/generaly)
followed for its ZX game: **don't bulk-disassemble and guess; trace
from the entry point**. Each routine gets understood, named, and
re-implemented in our target language (here: C with inline asm in
hot paths). For SMC and computed jumps, snapshots + ZRCP single-step
let us pin runtime behaviour that static analysis can't.

Three snapshots (hi-score, main menu, gameplay) give a per-byte map of
RAM across (loaded-from-tape / built-at-init / live-game-state) classes
— see `notes/init.md` for the verified memory map.

## Original assets

`original/batty.tap`, `original/BATTY.TZX`, `original/Batty.scr`,
`original/Batty.txt` are © Elite Systems / Hit-Pak, 1987. Vendored for
personal reverse-engineering reference only — the same pattern
[`generaly`](https://github.com/ddanila/generaly) uses with its
`original/generals.trd`.

## License

The MS-DOS recreation (everything under `src/`, `scripts/`, `notes/`,
the `Makefile`, etc.) is MIT.
