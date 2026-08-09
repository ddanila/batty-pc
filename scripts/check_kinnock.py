#!/usr/bin/env python3
"""The Kinnock easter egg says what the tape says, where the tape says it.

`kinnock` is one byte at $B973 holding $01, and the disassembly's own
comment is the documentation: "если сюда записать ноль, то перед игрой
будет надпись про Киннока". POKE 47475,0 and you get

    print_kinnock:
      LD A,(kinnock) / AND A / RET NZ
      LD DE,txt_kinnock / LD B,$02 / CALL print_message
      LD D,$00 / CALL pause_short
      JP clear_screen_attrib

### Why this is a source gate and not a screendump

`pause_short` with D=0 is 256 iterations of a 255-step inner loop —
about 1.05M T-states, ~0.30 s at 3.5 MHz. (The disassembly's own
arithmetic agrees: `LD B,$04 / CALL pause_long` is annotated "Пауза 1,2
сек. (4*0.3)".) Catching a third of a second between boot and the level
flush with a timed QEMU screendump would be luck, and a gate that
depends on luck is worse than none — see known-bugs #17.

So this checks the things a screendump could not tell you anyway: that
the glyph codes still spell the right words, that the coordinates come
from txt_kinnock's own headers with the bottom-anchor conversion
applied, and that it runs where LB9E8_1 runs it.

The expected bytes are PARSED from original/disasm/txt/txt_kinnock.asm
rather than copied here. A gate carrying its own transcription agrees
with a wrong transcription in the port as long as both are wrong the
same way, and a single bad nibble is invisible in review and unreadable
on screen for the 0.3 s it is up.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"

# notes/encoding.md: 0x00..0x09 digits, 0x0A..0x23 = A..Z, 0x24 = '.',
# 0x26 = space.
def decode(codes):
    out = []
    for c in codes:
        if c <= 0x09:
            out.append(chr(ord("0") + c))
        elif 0x0A <= c <= 0x23:
            out.append(chr(ord("A") + c - 0x0A))
        elif c == 0x24:
            out.append(".")
        elif c == 0x26:
            out.append(" ")
        else:
            out.append(f"<{c:02X}>")
    return "".join(out)


TXT = ROOT / "original/disasm/txt/txt_kinnock.asm"


def read_original():
    """Parse txt_kinnock.asm into [(x, y, attr, codes), ...].

    Derived, not transcribed. An earlier draft of this gate carried its
    own copy of the bytes and the expected strings, which would have
    made it agree with a wrong transcription in the port as long as both
    were wrong the same way — the exact failure mode that keeps turning
    up in this repo's stale lists.

    print_message is called with B=$02, so there are two lines, each a
    4-byte header (x, y, attr, len) followed by `len` glyph codes.
    """
    raw = []
    for line in TXT.read_text().splitlines():
        line = line.split(";")[0]
        if "DEFB" not in line:
            continue
        raw += [int(h, 16) for h in re.findall(r"\$([0-9A-Fa-f]{2})", line)]
    lines, i = [], 0
    while i < len(raw):
        x, y, attr, n = raw[i:i + 4]
        i += 4
        lines.append((x, y, attr, raw[i:i + n]))
        i += n
    if len(lines) != 2:
        raise SystemExit(f"FAIL: {TXT.name} parsed to {len(lines)} lines, "
                         f"not the 2 that print_message's B=$02 asks for")
    for x, y, attr, codes in lines:
        if not codes:
            raise SystemExit(f"FAIL: {TXT.name} parsed to an empty line — "
                             f"the header/length walk is wrong")
    return lines


ORIGINAL = read_original()
EXPECT = [decode(codes) for _, _, _, codes in ORIGINAL]
EXPECT_XY = [(x, y) for x, y, _, _ in ORIGINAL]


def body_of(src: str, signature: str) -> str:
    start = src.index(signature)
    depth = 0
    i = src.index("{", start)
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise SystemExit(f"FAIL: could not find the end of {signature}")


def main() -> int:
    src = SRC.read_text()
    try:
        body = body_of(src, "static void print_kinnock(void) {")
    except ValueError:
        raise SystemExit("FAIL: print_kinnock is gone. If the easter egg "
                         "moved, point this gate at it; if it was removed, "
                         "PLAN.md WS9 item 1 goes back to open.")

    arrays = re.findall(r"line[12]\[\]\s*=\s*\{(.*?)\}", body, re.S)
    if len(arrays) != 2:
        raise SystemExit(f"FAIL: expected two text lines, found "
                         f"{len(arrays)}. print_message is called with "
                         f"B=$02 — two lines.")

    for i, (raw, want) in enumerate(zip(arrays, EXPECT), start=1):
        codes = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", raw)]
        got = decode(codes)
        if got != want:
            raise SystemExit(
                f"FAIL: line {i} decodes to {got!r}, not {want!r}. The "
                f"expected text is parsed from {TXT.name}; one wrong "
                f"nibble is invisible in review and unreadable on screen "
                f"for the 0.3 s it is up.")
    print(f"PASS kinnock_text: both lines decode to "
          f"{EXPECT[0]!r} / {EXPECT[1]!r}")

    # Coordinates: straight from the txt headers, with the -5
    # bottom-anchor conversion. Using the raw y is the mistake the round
    # banner already made once (notes/menu.md, and the comment at
    # show_window_round_number).
    for i, (x, y) in enumerate(EXPECT_XY, start=1):
        want = f"BORDER_X + 0x{x:02X}, BORDER_Y + 0x{y:02X} - 5"
        if "".join(want.split()) not in "".join(body.split()):
            raise SystemExit(
                f"FAIL: line {i} is not drawn at txt_kinnock's own "
                f"coordinates. Expected `{want}`. The y byte is "
                f"BOTTOM-anchored — screen_addr_calc treats it as the "
                f"glyph's lowest row and print_line draws upward — so the "
                f"top row is y-5. Using the raw byte jams the text 5px "
                f"low, which is exactly what the round banner did before "
                f"it was fixed.")
    print("PASS kinnock_coords: both lines use the txt headers with the "
          "bottom-anchor conversion")

    # Off unless asked for, and running where LB9E8_1 runs it.
    if 'getenv("BATTY_KINNOCK")' not in src:
        raise SystemExit("FAIL: BATTY_KINNOCK no longer enables the egg")
    if "unsigned char kinnock;" not in src or "!dbg.kinnock" not in body:
        raise SystemExit("FAIL: print_kinnock no longer checks its switch — "
                         "the egg would fire in every normal game")
    print("PASS kinnock_opt_in: BATTY_KINNOCK gates it, default off")

    enter = body_of(src, "static bool enter_level(unsigned char lvl_idx) {")
    if "print_kinnock();" not in enter:
        raise SystemExit("FAIL: print_kinnock is not called from enter_level")
    if enter.index("print_kinnock();") > enter.index("render_level_screen("):
        raise SystemExit(
            "FAIL: print_kinnock runs after render_level_screen. LB9E8_1 "
            "calls it while the level is still in the buffer and the "
            "attributes are cleared, BEFORE buff_to_screen_pixs — so it "
            "shows over a blank screen, not over the level.")
    print("PASS kinnock_placement: it runs before the level is flushed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
