#!/usr/bin/env python3
"""Inline arrays copied from the tape must still equal the tape.

`check_asset_provenance.py` covers the data the port LOADS: every .BIN
must be built from `original/blocks/*.dat.bin`. This is the other half —
the tables and sprites transcribed straight into the source, where
nothing rebuilt them and nothing compared them to anything.

That gap was not theoretical. Diffing these by hand found three wrong
addresses in one pass (bonus_table_first documented at $9E5A when it is
at $9E4A, border_horizontal_addon at $BFFB when it is at $BFF7, and the
whole prop_* block attributed to $9F27, which is only prop_x_coord).
The BYTES were right in all three cases; the provenance was not. A wrong
address reads as provenance and sends the next person to neighbouring
data that looks like a plausible table too.

Three kinds of inline data exist, and only two are checkable here:

  EXACT      a byte-for-byte slice of the tape. Most of them.
  DERIVED    the tape's bytes plus a stated transformation — a
             synthesized placeholder, a re-index, BCD to decimal. The
             transform is written out below as code, so the gate proves
             the RELATIONSHIP rather than just asserting the result.
  PORT-OWN   the port's own invention (the VGA palette, the glyph-code
             text for screens the original does not have). Nothing to
             check; deliberately absent from the table below.

Arrays that move OUT of the source into assets/ simply drop off this
list — check_asset_provenance.py picks them up on the other side, so
between the two gates every byte of original data has a checked origin.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BLOCK = ROOT / "original" / "blocks" / "03_DATA_headless.dat.bin"
BASE = 0x6800          # block 03 loads at $6800

# name -> (source file, tape address, transform)
#
# `transform` maps the TAPE bytes to what the array should hold. `None`
# means an exact copy. Anything else states the divergence as code, which
# is the point: a comment claiming "byte-exact apart from X" is not
# checkable, and this is.
EXACT = None

def drop_placeholder(b: bytes) -> bytes:
    """briks_colors: the original is indexed 1..14 off `briks_colors-$01`;
    the port indexes it 0-based, so slot [0] is a synthesized filler and
    the tape's 15 bytes shift up by one."""
    return b"\x00" + b[:15]

CASES = [
    # spr_bomb ($786A), spr_magnet_circle_off ($78AC) and _on ($7938) were
    # here until they moved into assets/sprites_low.bin. They are checked
    # on the other side now, by check_asset_provenance.py.
    # --- exact slices -------------------------------------------------
    ("bonus_table_first",   "src/main.cpp",    0x9E4A,  32, EXACT),
    # NOT $9E6A: the tape holds `first` twice, at $9E4A and $9E6A, so a
    # read there returns a convincing copy of the wrong table.
    ("bonus_table_second",  "src/main.cpp",    0x9E8A,  32, EXACT),
    ("border_addon",        "src/main.cpp",    0xBFF7,  30, EXACT),
    ("prop_x_coord",        "src/main.cpp",    0x9F27,   4, EXACT),
    ("prop_uneven",         "src/main.cpp",    0x9F2B,   6, EXACT),
    ("prop_even",           "src/main.cpp",    0x9F31,   6, EXACT),
    ("run_dot_mask",        "src/main.cpp",    0xB969,   8, EXACT),
    ("brik_anim_sprites",   "src/main.cpp",    0xAEFF, 112, EXACT),
    ("spr_brik_1",          "src/bricks.cpp",  0xAEFF,  16, EXACT),
    ("dir_sin_tbl",         "src/physics.cpp", 0xAD58,  17, EXACT),
    ("zone_tbl_normal",     "src/physics.cpp", 0xABEE,  14, EXACT),
    ("zone_tbl_big",        "src/physics.cpp", 0xABFC,  14, EXACT),
    ("deflect_tbl",         "src/physics.cpp", 0xAC0A,  24, EXACT),
    # --- derived ------------------------------------------------------
    ("briks_colors",        "src/bricks.cpp",  0xAEEC,  15, drop_placeholder),
]


def read_array(path: Path, name: str) -> bytes:
    """The initialiser of `name`, as bytes.

    Comments are stripped BEFORE the numbers are scanned. Several of
    these arrays carry per-row notes containing digits — `/* L1 */`,
    `/* [11..14] indestructible */` — and a scan that ran first would
    silently fold those into the data and compare garbage.
    """
    text = path.read_text()
    m = re.search(re.escape(name) + r"[^=;{]*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise LookupError(f"no initialiser for `{name}` in {path.name}")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    return bytes(int(t, 16) if t.lower().startswith("0x") else int(t)
                 for t in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", body))


def main() -> int:
    if not BLOCK.exists():
        print(f"SKIP inline_tape_data: {BLOCK.relative_to(ROOT)} absent "
              f"(the `original` submodule is not checked out)")
        return 0

    blk = BLOCK.read_bytes()
    bad: list[str] = []

    for name, rel, addr, length, transform in CASES:
        path = ROOT / rel
        off = addr - BASE
        if off < 0 or off + length > len(blk):
            bad.append(f"{name}: ${addr:04X} is outside block 03 "
                       f"(${BASE:04X}..${BASE + len(blk):04X})")
            continue
        want = blk[off:off + length]
        if transform is not EXACT:
            want = transform(want)
        try:
            got = read_array(path, name)
        except LookupError as e:
            bad.append(f"{name}: {e} — renamed or deleted? Update this gate.")
            continue
        if got == want:
            continue
        if len(got) != len(want):
            bad.append(f"{name} ({rel}): array is {len(got)} bytes, "
                       f"${addr:04X} gives {len(want)}")
            continue
        diff = [i for i in range(len(got)) if got[i] != want[i]]
        near = [a for a in range(BASE, BASE + len(blk) - len(got))
                if blk[a - BASE:a - BASE + len(got)] == got]
        hint = (f" — those exact bytes ARE in the tape at "
                f"{', '.join(f'${a:04X}' for a in near[:3])}, so the ADDRESS "
                f"is probably what is wrong" if near and transform is EXACT
                else "")
        bad.append(f"{name} ({rel}) does not match ${addr:04X}: "
                   f"{len(diff)}/{len(got)} bytes differ, first at +{diff[0]} "
                   f"(source {got[diff[0]]:#04x}, tape {want[diff[0]]:#04x})"
                   + hint)

    if bad:
        print("FAIL: inline data disagrees with the tape\n")
        for b in bad:
            print(f"  - {b}")
        print("\nEither the array was mistyped, or the address documenting "
              "it is wrong. Check which before editing: the bytes are load-"
              "bearing, the address is documentation, and they fail the "
              "same way here.")
        return 1

    exact = sum(1 for c in CASES if c[4] is EXACT)
    print(f"PASS inline_tape_data: {len(CASES)} inline arrays match block 03 "
          f"({exact} exact slices, {len(CASES) - exact} derived)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
