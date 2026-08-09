#!/usr/bin/env python3
"""The multiball spawn must read the primary's DIR BYTE (known-bugs #14).

The original's LA67B_8 (`$A67B`) reads `(IY+$06)` — the primary ball's
dir byte — and splits it: `AND $0F` picks which pair of relative angles
to use, `AND $30` carries the quadrant.

The port passed `delta_to_dir(ball.dx, ball.dy)` instead: a dir
RECONSTRUCTED from the {-1,0,+1} sign cache. `delta_to_dir` chooses its
angle with `abs(dx) >= BALL_SPEED`, and a sign is never >= 2, so the low
nibble was `$04` every time and the port always took the first of the
three branches.

### Why this is a source gate

The full 59-gate QEMU suite passed both before and after the fix. No
scenario reaches a multiball spawn from a primary whose low nibble is
not `$04`, so nothing observes the difference — the bug and its fix are
invisible to every pixel and probe gate there is. `extra_ball_dirs`
itself is well covered by host tests, and always was: it was the faithful
half.

So the thing worth guarding is the CALL, and two properties of it:

  - `apply_multi_ball_bonus` passes `objects[OBJ_BALL_1].dir`
  - `delta_to_dir` has no production caller at all

The second is the sharper one. `delta_to_dir` now exists only for its
host tests; if a call to it reappears in `src/`, that is the round trip
coming back, whatever it is called from.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"


def body_of(text: str, signature: str) -> str:
    start = text.index(signature)
    depth = 0
    i = text.index("{", start)
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    raise SystemExit(f"FAIL: could not find the end of {signature}")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def main() -> int:
    main_src = (SRC / "main.cpp").read_text()

    try:
        body = strip_comments(
            body_of(main_src, "static void apply_multi_ball_bonus(void) {"))
    except ValueError:
        raise SystemExit("FAIL: apply_multi_ball_bonus is gone; if the "
                         "multiball spawn moved, point this gate at it")

    if "extra_ball_dirs(objects[OBJ_BALL_1].dir)" not in "".join(body.split()):
        raise SystemExit(
            "FAIL: the multiball spawn no longer passes the primary's dir "
            "byte to extra_ball_dirs. The original reads (IY+$06) directly "
            "(LA67B_8) — anything derived from the sign cache collapses the "
            "low nibble to $04 and makes every spawn take the same branch. "
            "known-bugs #14. No QEMU gate can see this.")
    print("PASS multiball_reads_dir_byte: extra_ball_dirs gets "
          "objects[OBJ_BALL_1].dir")

    # delta_to_dir must remain caller-free in production.
    callers = []
    for f in sorted(SRC.glob("*.cpp")):
        if f.name == "physics.cpp":
            continue                      # its own definition lives there
        for n, line in enumerate(strip_comments(f.read_text()).split("\n"), 1):
            if re.search(r"\bdelta_to_dir\s*\(", line):
                callers.append(f"{f.name}:{n}")
    if callers:
        raise SystemExit(
            "FAIL: delta_to_dir is called from production code at "
            + ", ".join(callers)
            + ".\nIt reconstructs a direction from SIGNS and loses the low "
              "nibble, which is what known-bugs #14 was. It is kept only "
              "for its host tests. If you need a direction, read the "
              "object's dir byte.")
    print("PASS delta_to_dir_unused: no production caller reconstructs a "
          "direction from signs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
