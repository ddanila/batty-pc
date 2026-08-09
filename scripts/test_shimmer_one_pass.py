#!/usr/bin/env python3
"""The metal-brick shimmer plays ONE pass, not a loop (known-bugs #3).

Hitting an undestructible brick registers it in one of five `briks_data`
slots (`LAFFC_34`); `metal_brik_anim` ($B6A9) then cycles it through
`anim_brik`'s 8 sprites, 2 ticks each, for ONE ~15-tick pass. The
counter is `(c + 1) & $0F`, and the wrap to 0 is what FREES the slot —
0 is the free marker, which is why `fill_briks_data` skips it.

Bug #3 was that the port looped it forever. Fixed 2026-06-11.

WHY THIS IS A SOURCE GATE. Nothing prevented the bug returning:
mutating the mask from `0x0F` to `0x1F` — the slot never frees, so the
shimmer runs twice as long and keeps going — leaves ALL 59 QEMU GATES
GREEN. Verified by running the full suite against the mutated build.

A pixel gate could catch it in principle, but it needs a ball to strike
an undestructible brick and then ~30 frames of observation on that exact
cell. No existing scenario does that, and `test-brik-anim-pace` covers a
different thing: the animation's PACING (8 frames, 2 ticks each) and
that a keypress cannot abort it (bug #4), neither of which changes when
the slot stops freeing itself.

So this pins the two facts that make it one pass, the same way
test-stuck-ball-offset and test-ball-sign-cache-owner pin invariants no
capture reaches:

  - the counter wraps with `& 0x0F`, so a slot frees after 15 ticks
  - 0 means FREE — the liveness check treats a zero tick count as an
    empty slot, so the wrap is what ends the animation
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


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
        step = body_of(src, "static void step_brick_hit_anim(void) {")
    except ValueError:
        raise SystemExit("FAIL: step_brick_hit_anim is gone; if the shimmer "
                         "moved, point this gate at its new home")

    compact = "".join(re.sub(r"/\*.*?\*/", " ", step, flags=re.S).split())

    if "&0x0F" not in compact:
        raise SystemExit(
            "FAIL: the shimmer counter no longer wraps with & 0x0F, so the "
            "slot never frees and the animation loops — known-bugs #3, "
            "which was user-reported and fixed on 2026-06-11. All 59 QEMU "
            "gates pass with this broken; nothing else guards it.")
    print("PASS shimmer_one_pass_wrap: the counter wraps with & 0x0F")

    live = body_of(src, "static int any_brick_hit_anim(void) {")
    if "brick_hit_anim_ticks[i]" not in live:
        raise SystemExit(
            "FAIL: the liveness check no longer reads the tick counter, so "
            "0 may not mean FREE any more — the wrap only ends the "
            "animation because a zero count reads as an empty slot.")
    print("PASS shimmer_one_pass_free: a zero tick count marks the slot free")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
