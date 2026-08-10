#!/usr/bin/env python3
"""The in-place HUD patch must cover every row the score digits occupy.

When the score changes on a magnet-free level the HUD strip is patched
rather than rebuilt: `update_static_hud_top` repaints the top, copies it
into the background cache and marks it dirty. That strip was
FRAME_TOP_H_PX (24) rows tall. The digits are printed at HUD_SCORE_Y
($15 = 21) and are HUD_DIGIT_H_PX (8) rows tall, so they reach row 28 —
five rows below the frame. Those rows were neither re-cached nor
flushed, and kept the PREVIOUS score's pixels until something else forced
a full rebuild. known-bugs.md #22.

WHY THIS IS A SOURCE GATE, and it is not a preference.

The visual-test executable is built with -dBATTY_SCORELESS_HUD
(Makefile's build/%-test.obj rule) because the ground-truth capture
pipeline NOPs the original's score block. `render_hud_to_buff` compiles
to an empty function there. So every QEMU gate in this repo — all of
them run on the test floppy — is looking at a screen with NO SCORE
DIGITS ON IT. A screendump gate for this defect cannot be written
without first giving the test build a HUD, and that would change what
every existing visual gate compares against.

That is worth stating plainly rather than leaving as a shrug: the score
digits, one of the few things on screen the player reads rather than
watches, have no visual coverage whatsoever. This gate holds the
arithmetic instead, which is the part that was actually wrong.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


def define(text: str, name: str) -> int:
    m = re.search(rf"^#define\s+{name}\s+(\S+)", text, re.M)
    if not m:
        raise SystemExit(f"FAIL: #define {name} is gone; this gate cannot "
                         f"check an extent that is no longer written down")
    return int(m.group(1), 0)


def main() -> int:
    text = SRC.read_text()

    score_y = define(text, "HUD_SCORE_Y")
    digit_h = define(text, "HUD_DIGIT_H_PX")
    frame_h = define(text, "FRAME_TOP_H_PX")

    m = re.search(r"^#define\s+HUD_PATCH_H_PX\s+\(([^)]*)\)", text, re.M)
    if not m:
        raise SystemExit("FAIL: HUD_PATCH_H_PX is gone or no longer a derived "
                         "expression. It must stay DERIVED from the digit "
                         "geometry — a hand-written 29 is the same mistake as "
                         "the hand-written 24 it replaced, one round later.")
    patch_h = score_y + digit_h        # what the expression must come to
    if m.group(1).replace(" ", "") != "HUD_SCORE_Y+HUD_DIGIT_H_PX":
        raise SystemExit(f"FAIL: HUD_PATCH_H_PX is `{m.group(1).strip()}`, "
                         f"expected HUD_SCORE_Y + HUD_DIGIT_H_PX")

    if patch_h <= frame_h:
        raise SystemExit(
            f"FAIL: the digits end at row {patch_h - 1} and the patched strip "
            f"is {patch_h} rows, which is no deeper than the {frame_h}-row top "
            f"frame. Either the digits moved or this gate is checking the "
            f"wrong thing — it exists because those two numbers were assumed "
            f"equal and are not.")

    body = re.search(r"static void update_static_hud_top\(unsigned char "
                     r"level_idx\) \{(.*?)\n\}", text, re.S)
    if not body:
        raise SystemExit("FAIL: update_static_hud_top not found")
    if "y < HUD_PATCH_H_PX" not in body.group(1):
        raise SystemExit(
            "FAIL: update_static_hud_top's pixel copy is not bounded by "
            "HUD_PATCH_H_PX. If it is back to FRAME_TOP_H_PX, the bottom "
            f"{patch_h - frame_h} rows of every score digit go stale in the "
            "background cache again.")

    if not re.search(r"mark_dirty_bytes\(0,\s*HUD_PATCH_H_PX,\s*0,\s*31\)",
                     text):
        raise SystemExit(
            "FAIL: the patched strip's dirty mark is not HUD_PATCH_H_PX tall. "
            "The cache can be right and the screen still stale — the copy and "
            "the mark have to agree, and they are 40 lines apart.")

    scoreless = (ROOT / "Makefile").read_text()
    if "-dBATTY_SCORELESS_HUD" not in scoreless:
        raise SystemExit(
            "FAIL: the test build no longer defines BATTY_SCORELESS_HUD. If "
            "the visual executable now DRAWS the score, this defect is "
            "reachable from a screendump and deserves a real visual gate "
            "instead of this one — see the module docstring.")

    print(f"PASS hud_patch_extent: digits span rows {score_y}..{patch_h - 1}, "
          f"the patch covers {patch_h} rows (top frame is {frame_h})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
