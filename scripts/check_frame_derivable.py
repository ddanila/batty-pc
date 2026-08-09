#!/usr/bin/env python3
"""The frame's top and side strips are tape sprites, not captured pixels.

`assets/frame_l1.bin` is the perimeter ornament, extracted from emulator
screens (`scripts/extract_frame.py`), and PLAN.md WS7 item 1 wants it
replaced by a port of the `LBE8B` compositor. This gate is the evidence
that the compositor's top arm is reproducible: it lays out the eight
sprites `set_border_horizontal` names and asserts the captured bytes
agree, for all four colour cycles.

    set_border_horizontal:
      DEFW spr_bord_horiz_left_edge   / left_thin / left_bold / left_thin
      DEFW spr_bord_horiz_right_thin  / right_bold / right_thin / right_edge

    LD HL,set_border_horizontal / EXX / LD HL,$0700
    LBE8B_6:
      EXX / LD E,(HL) / INC HL / LD D,(HL) / INC HL / PUSH DE
      EXX / POP DE / CALL print_sprite_pix / CALL print_sprite_attrib
      LD A,$20 / ADD A,L / LD L,A / JR NC,LBE8B_6

so eight 4-byte-wide sprites at x = 0, $20, $40 ... $E0, y = $07.

### print_sprite_pix draws UPWARD, and that is the whole trick

    print_sprite_pix_2:
      LD A,(DE) / LD (HL),A / INC DE / INC L / DJNZ print_sprite_pix_2
      DEC L / LD A,L
    sprite_next_line:
      SUB $00            ; patched with width + $1F
      LD L,A / JP NC,... / DEC H

It is a plain unmasked copy — no mask, no OR — and each row moves to the
PREVIOUS buffer line. So the first data row lands at the given y and
later rows stack ABOVE it: a sprite at y=$07 occupies y=0..7 with its
rows reversed.

Laying the same data out top-down instead matches 58 bytes of 256. That
was the first attempt here, and it looked like the strip simply was not
derivable.

### The sides: two sprites, alternating, stepping up 56 rows

    LD HL,$9F00 / LD DE,spr_bord_left_thin
    EXX
    LD HL,$BF00 / LD DE,spr_bord_left_bold / LD B,$07
    LBE8B_1:
      LD L,$00 / CALL print_sprite_pix / CALL print_sprite_attrib
      LD L,$F8 / CALL print_sprite_pix / CALL print_sprite_attrib
      LD A,$C8 / ADD A,H / LD H,A        ; -56 rows
      EXX
      DJNZ LBE8B_1

Two register sets, swapped every iteration, so it alternates bold (32
rows, from y=$BF) and thin (24 rows, from y=$9F), each stepping up 56.
Seven iterations; the seventh runs off the top of the strip and is
covered by the top border.

The RIGHT-hand variant is never named. `print_sprite_pix` walks DE
through the sprite it draws, and `print_sprite_attrib` walks it further,
so by the second call DE points at the NEXT block in memory — which is
why the disassembly says of each pair "следующие два спрайта должны идти
строго друг за другом", these two sprites must come strictly one after
the other.

### Scope: all of it

The 24-row top block is GENERATED — background texture, the seventh side
placement, `LBE8B_2`'s first inner-outline band, the eight top sprites,
then the addon strip, in `LBE8B`'s order — and compared whole:
3072 bytes. The side strips add 1344. That is **every byte of
`frame_l1.bin`**, all four cycles, from the tape.

Plus the 1110 attribute cells in `level_attrs.bin` those sprites' attr
blocks produce, over all 15 levels.

Checking the pieces separately reached 2986 of 3072 and left a residue
in byte columns 1 and 30. The missing piece was ORDERING, not data:
`LBE8B_2`'s band runs BEFORE the top border, which then overwrites rows
0..7 of what it did. A generator gets that for free; a predicate over
positions cannot.

The rest of that blob is not frame work at all. `extract_frame.py`'s own
comment says so: of its 24 top pixel rows, y 0..7 are the ornament and
y 8..23 are the HUD's labels and score digits — which the port draws
itself in `render_hud_to_buff`. The same goes for top attr rows 1 and 2
(they match these sprites 26 and 13 times out of 128, i.e. not at all).

So everything in `frame_l1.bin` is either ornament that derives from the
tape or HUD the port already generates. Retiring the blob needs the
`LBE8B` port, not more data.

Not covered: the `border_horizontal_addon` AND-strip at `scr_buff+$101`,
which modifies pixels after the sprites are laid down.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GFX = ROOT / "original/disasm/gfx/sprites_color_1.asm"
FRAME = ROOT / "assets/frame_l1.bin"
ATTRS = ROOT / "assets/level_attrs.bin"
N_LEVELS = 15

# set_border_horizontal's eight entries, in order.
SEQUENCE = [
    "spr_bord_horiz_left_edge",
    "spr_bord_horiz_left_thin",
    "spr_bord_horiz_left_bold",
    "spr_bord_horiz_left_thin",
    "spr_bord_horiz_right_thin",
    "spr_bord_horiz_right_bold",
    "spr_bord_horiz_right_thin",
    "spr_bord_horiz_right_edge",
]
BASE_Y = 0x07          # LD HL,$0700 — H is the y, L the x
N_CYCLES = 4
ASM = ROOT / "original/disasm/batty.asm"
TILE = ROOT / "assets/bg_tile.bin"


def block(asm: str, addr: str):
    """The sprite block whose `; Data block at NNNN` header says addr.

    The right-hand side sprites carry no label of their own — the
    original reaches them by letting DE walk off the end of the left
    one — so they can only be named by address.
    """
    # The `:?` is not decoration: the header for $6C95 in the
    # disassembly reads "; Data block at 6C95:" with a stray colon, and
    # a strict pattern silently drops the last top-border sprite.
    m = re.search(r"; Data block at " + addr + r":?\n(?:;[^\n]*\n)*"
                  r"(?:\w+:\n)?((?:\s*DEFB[^\n]*\n)+)", asm)
    if not m:
        raise SystemExit(f"FAIL: no data block at ${addr} in {GFX.name}")
    return [int(h, 16) for h in re.findall(r"\$([0-9A-Fa-f]{2})", m.group(1))]


def raw_sprite(asm: str, name: str):
    """Every DEFB byte of the named sprite's block, header included."""
    try:
        i = asm.index(name + ":")
    except ValueError:
        raise SystemExit(f"FAIL: {name} is not in {GFX.name} — if the "
                         f"border sprites moved, point this gate at them")
    nxt = asm.find("; Data block", i)
    body = asm[i:nxt if nxt > 0 else len(asm)]
    return [int(h, 16) for h in re.findall(r"\$([0-9A-Fa-f]{2})", body)]


def sprite(asm: str, name: str):
    """(width, height, pixel bytes) straight from the disassembly.

    Parsed, not transcribed: a gate carrying its own copy of the bytes
    agrees with a wrong copy in the port as long as both are wrong the
    same way.
    """
    b = raw_sprite(asm, name)
    w, h = b[0], b[1]
    if len(b) < 2 + w * h:
        raise SystemExit(f"FAIL: {name} declares {w}x{h} but carries "
                         f"{len(b) - 2} bytes")
    return w, h, b[2:2 + w * h]


def main() -> int:
    if not FRAME.exists():
        raise SystemExit("FAIL: assets/frame_l1.bin is missing")
    asm = GFX.read_text()
    sprites = [sprite(asm, n) for n in SEQUENCE]

    frame = FRAME.read_bytes()
    if len(frame) % N_CYCLES:
        raise SystemExit(f"FAIL: frame_l1.bin is {len(frame)} bytes, not a "
                         f"multiple of {N_CYCLES} cycles")
    per = len(frame) // N_CYCLES

    bad = []

    # --- GENERATE the whole 24-row top block --------------------------
    # LBE8B's passes, in its order. Piecewise checks got this to
    # 2986/3072 and left a residue in byte columns 1 and 30; generating
    # the block instead closes it, because the missing piece was an
    # ORDERING one — see the inner-border band below.
    bold_l, bold_r = block(asm, "6B3F"), block(asm, "6B67")
    thin_l, thin_r = block(asm, "6B8F"), block(asm, "6BAE")
    addon_m = re.search(r"border_horizontal_addon:\n((?:\s*DEFB[^\n]*\n)+)",
                        ASM.read_text())
    if not addon_m:
        raise SystemExit("FAIL: border_horizontal_addon is not in batty.asm")
    addon = [int(h, 16)
             for h in re.findall(r"\$([0-9A-Fa-f]{2})", addon_m.group(1))]
    if len(addon) != 30:
        raise SystemExit(f"FAIL: border_horizontal_addon is {len(addon)} "
                         f"bytes, expected the $1E the loop counts")
    tile = TILE.read_bytes()
    tsz = len(tile) // N_CYCLES

    checked = 0
    for cyc in range(N_CYCLES):
        t = tile[cyc * tsz:(cyc + 1) * tsz]
        buf = bytearray(24 * 32)

        # 1. the level's background texture, which LBE8B paints first
        for y in range(24):
            ty = (y & 15) * 2
            t0, t1 = t[ty], t[ty + 1]
            for bx in range(32):
                buf[y * 32 + bx] = t0 if bx % 2 == 0 else t1

        # 2. LBE8B_1's SEVENTH side placement, bold from y=$17. The
        #    other six live below this block; this one fills the sides
        #    of the HUD band.
        for r in range(bold_l[1]):
            y = 0x17 - r
            if 0 <= y < 24:
                buf[y * 32 + 0] = bold_l[2 + r]
                buf[y * 32 + 31] = bold_r[2 + r]

        # 3. LBE8B_2's FIRST inner-outline band. The loop runs A=$04
        #    bands of 28 rows, 56 rows apart; the port's
        #    inner_border_line_c has only the lower THREE (y0 = 50, 106,
        #    162) because this one's effect is already baked into
        #    frame_l1.bin. It starts 56 rows above 50, i.e. y=-6, so
        #    rows 0..21 of this block get bit 7 of byte 1 and bit 0 of
        #    byte 30 cleared.
        #
        #    Order matters and is the whole reason the piecewise version
        #    fell short: this runs BEFORE the top border, which then
        #    overwrites rows 0..7.
        for y in range(0, 22):
            buf[y * 32 + 1] &= 0x7F
            buf[y * 32 + 30] &= 0xFE

        # 4. the top border, eight sprites drawn UPWARD from y=$07
        x = 0
        for name in SEQUENCE:
            w, h, px = sprite(asm, name)
            for r in range(h):
                y = BASE_Y - r
                for bx in range(w):
                    buf[y * 32 + x + bx] = px[r * w + bx]
            x += w

        # 5. border_horizontal_addon, ANDed into row 8 bytes 1..30
        for i in range(30):
            buf[8 * 32 + 1 + i] &= addon[i]

        for k in range(24 * 32):
            checked += 1
            if buf[k] != frame[cyc * per + k]:
                bad.append((cyc, "top block", k // 32, k % 32,
                            buf[k], frame[cyc * per + k]))

    # --- the side strips ---------------------------------------------
    # extract_frame.py's layout, per cycle, since the attrs were
    # dropped: 768 top pixels, 168 left (y 24..191, one byte wide), 168
    # right. Derived from the constants rather than written as literals,
    # so a layout change shows up as a FAIL and not an IndexError — which
    # is what the stale `768 + 96` gave when the attrs went.
    TOP_PX, SIDE_PX = 24 * 32, 168
    LEFT_OFF, RIGHT_OFF = TOP_PX, TOP_PX + SIDE_PX
    if per != TOP_PX + 2 * SIDE_PX:
        raise SystemExit(
            f"FAIL: frame_l1.bin is {per} bytes per cycle, expected "
            f"{TOP_PX + 2 * SIDE_PX} (24 top rows + two 168-row side "
            f"columns, pixels only). If the layout changed, update this "
            f"gate and extract_frame.py together.")
    # LBE8B_1: bold from y=$BF, thin from y=$9F, each -56 per turn.
    placements = []
    y_bold, y_thin = 0xBF, 0x9F
    for i in range(7):
        if i % 2 == 0:
            placements.append((y_bold, bold_l, bold_r)); y_bold -= 56
        else:
            placements.append((y_thin, thin_l, thin_r)); y_thin -= 56

    side_checked = 0
    for cyc in range(N_CYCLES):
        for base, ls, rs in placements:
            for r in range(ls[1]):
                y = base - r
                if not (24 <= y <= 191):
                    continue          # the 7th placement runs off the top
                for off, spr in ((LEFT_OFF, ls), (RIGHT_OFF, rs)):
                    want = spr[2 + r]
                    got = frame[cyc * per + off + (y - 24)]
                    side_checked += 1
                    if got != want:
                        bad.append((cyc, "side", r, y, want, got))

    if bad:
        print(f"FAIL: {len(bad)} of {checked + side_checked} frame bytes do "
              f"not match the tape's sprites.\n")
        for cyc, name, row, bx, want, got in bad[:12]:
            print(f"  cycle {cyc} {name} row {row} byte {bx}: "
                  f"want {want:02X} got {got:02X}")
        if len(bad) > 12:
            print(f"  ... and {len(bad) - 12} more")
        print("\nEither the sprite order changed, or frame_l1.bin was "
              "re-extracted from a different capture. Note the rows go "
              "UPWARD from the given y.")
        return 1

    # --- the attribute cells ------------------------------------------
    # Each sprite carries its own attr block after the pixels: (aw, ah)
    # then aw*ah bytes, written by print_sprite_attrib. Those stack
    # UPWARD as well — laying them downward matched 48 of 168 when
    # frame_l1.bin still carried its own copy.
    #
    # It no longer does: those 552 bytes were dead payload and were
    # dropped on 2026-08-09. The recording that survives is the one the
    # port actually reads.
    def attr_block(b):
        w, h = b[0], b[1]
        rest = b[2 + w * h:]
        aw, ah = rest[0], rest[1]
        return aw, ah, rest[2:2 + aw * ah]

    # --- the attrs the PORT actually uses --------------------------
    # paint_frame_to_buff takes its pixels from frame_l1.bin but its
    # ATTRS from level_attrs.bin — all three paint_strip_to_buff calls
    # pass `lattr`. The frame blob's own attr sections are loaded and
    # never read (138 bytes per cycle, 552 in all; see PLAN.md WS7).
    #
    # So the cells that matter are level_attrs.bin's: char row 0 across
    # the top, and columns 0 and 31 down char rows 3..23. Those come
    # from the same sprites, and this is the half of the check that
    # constrains what the game draws.
    la = ATTRS.read_bytes()
    if len(la) != N_LEVELS * 768:
        raise SystemExit(f"FAIL: level_attrs.bin is {len(la)} bytes, "
                         f"expected {N_LEVELS * 768}")
    used_checked = 0
    for lvl in range(N_LEVELS):
        band = la[lvl * 768:(lvl + 1) * 768]
        x = 0
        for name in SEQUENCE:
            aw, _, av = attr_block(raw_sprite(asm, name))
            for i in range(aw):
                want, got = av[i], band[0 * 32 + x + i]
                used_checked += 1
                if got != want:
                    bad.append((lvl, "level_attrs top", 0, x + i, want, got))
            x += aw
        # All seven placements, char rows 0..23. The seventh (bold from
        # y=$17) supplies rows 0..2's frame columns — the HUD band's —
        # which nothing else does.
        for base, ls, rs in placements:
            _, ah, lav = attr_block(ls)
            _, _, rav = attr_block(rs)
            for i in range(ah):
                cr = base // 8 - i
                if not (0 <= cr <= 23):
                    continue
                for col, av in ((0, lav), (31, rav)):
                    want, got = av[i], band[cr * 32 + col]
                    used_checked += 1
                    if got != want:
                        bad.append((lvl, "level_attrs side", cr, col,
                                    want, got))

    if bad:
        print(f"FAIL: {len(bad)} frame attribute bytes do not match.\n")
        for lvl, name, row, bx, want, got in bad[:12]:
            print(f"  {name} lvl/cycle {lvl} row {row} col {bx}: "
                  f"want {want:02X} got {got:02X}")
        return 1

    total = checked + side_checked + used_checked
    print(f"PASS frame_derivable: all {total} frame bytes "
          f"({checked} generated top-block px + {side_checked} side px "
          f"+ {used_checked} level_attrs cells over "
          f"{N_LEVELS} levels) are set_border_horizontal and the "
          f"bold/thin side pair, drawn upward")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
