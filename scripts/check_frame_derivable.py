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

### Scope

y 0..7 across all 32 byte-columns (256/cycle), the two 1-byte side
columns over y 24..191 (336/cycle), and the attribute cells those
sprites carry (74/cycle): **2664 of `frame_l1.bin`'s 4968**.

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

    checked = 0
    bad = []
    for cyc in range(N_CYCLES):
        x_byte = 0
        for name, (w, h, px) in zip(SEQUENCE, sprites):
            for row in range(h):
                y = BASE_Y - row               # print_sprite_pix goes UP
                for bx in range(w):
                    want = px[row * w + bx]
                    got = frame[cyc * per + y * 32 + x_byte + bx]
                    checked += 1
                    if got != want:
                        bad.append((cyc, name, row, bx, want, got))
            x_byte += w

    if bad:
        print(f"FAIL: {len(bad)} of {checked} top-strip bytes do not match "
              f"the tape's sprites.\n")
        for cyc, name, row, bx, want, got in bad[:12]:
            print(f"  cycle {cyc} {name} row {row} byte {bx}: "
                  f"want {want:02X} got {got:02X}")
        if len(bad) > 12:
            print(f"  ... and {len(bad) - 12} more")
        print("\nEither set_border_horizontal's order changed, or "
              "frame_l1.bin was re-extracted from a different capture. "
              "Note the rows go UPWARD from y=$07 — laying them out "
              "top-down matches 58 of 256.")
        return 1

    # --- the side strips ---------------------------------------------
    # extract_frame.py's layout, per cycle: 768 top pixels, 96 top attrs,
    # 168 left pixels (y 24..191, one byte wide), 21 left attrs, then the
    # same for the right.
    LEFT_OFF, RIGHT_OFF = 768 + 96, 768 + 96 + 168 + 21
    bold_l, bold_r = block(asm, "6B3F"), block(asm, "6B67")
    thin_l, thin_r = block(asm, "6B8F"), block(asm, "6BAE")
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

    # --- the attribute rows ------------------------------------------
    # Each sprite carries its own attr block after the pixels: (aw, ah)
    # then aw*ah bytes, written by print_sprite_attrib. Those stack
    # UPWARD as well — laying them downward matches 48 of 168.
    TOP_A = 768
    LEFT_A = 768 + 96 + 168
    RIGHT_A = 768 + 96 + 168 + 21 + 168
    def attr_block(b):
        w, h = b[0], b[1]
        rest = b[2 + w * h:]
        aw, ah = rest[0], rest[1]
        return aw, ah, rest[2:2 + aw * ah]

    attr_checked = 0
    for cyc in range(N_CYCLES):
        # Top: char row 0 only. Rows 1 and 2 of this block are the HUD's
        # label and score-digit rows, which render_hud_to_buff draws —
        # not frame work, and they do not match these sprites (26 and 13
        # of 128).
        # SEQUENCE, not a second list of addresses. The first version
        # carried one, and mutating its last entry SURVIVED — because
        # right_bold and right_edge happen to have identical attr blocks,
        # so the duplicate list was both redundant AND unable to justify
        # itself. The pixel pass above already names all eight.
        x = 0
        for name in SEQUENCE:
            aw, _, av = attr_block(raw_sprite(asm, name))
            for i in range(aw):
                want = av[i]
                got = frame[cyc * per + TOP_A + x + i]
                attr_checked += 1
                if got != want:
                    bad.append((cyc, f"top-attr {addr}", 0, x + i, want, got))
            x += aw
        for base, ls, rs in placements:
            _, ah, lav = attr_block(ls)
            _, _, rav = attr_block(rs)
            for i in range(ah):
                cr = base // 8 - i
                if not (3 <= cr <= 23):
                    continue
                for off, av in ((LEFT_A, lav), (RIGHT_A, rav)):
                    want = av[i]
                    got = frame[cyc * per + off + (cr - 3)]
                    attr_checked += 1
                    if got != want:
                        bad.append((cyc, "side-attr", i, cr, want, got))

    if bad:
        print(f"FAIL: {len(bad)} frame attribute bytes do not match.\n")
        for cyc, name, row, bx, want, got in bad[:12]:
            print(f"  cycle {cyc} {name} row {row} byte {bx}: "
                  f"want {want:02X} got {got:02X}")
        return 1

    total = checked + side_checked + attr_checked
    print(f"PASS frame_derivable: all {total} frame bytes "
          f"({checked} top px + {side_checked} side px + {attr_checked} "
          f"attrs, {N_CYCLES} cycles) are set_border_horizontal and the "
          f"bold/thin side pair, drawn upward")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
