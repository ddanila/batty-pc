#!/usr/bin/env python3
"""No `#define`d constant in src/ may be unused.

A constant nobody reads is not harmless: it is a claim about the program
that the program does not make. `BAT_X_MIN`/`BAT_X_MAX` sat in
`src/main.cpp` reading 8 and 216 while the actual bat clamp is
`check_left_margin`/`check_right_margin`, which the enemy-margin work
(known-bugs #16) had just been through — anyone reaching for those names
would have been clamping to the wrong thing and thinking they matched
the original.

Eleven were found on the first run:

    BALL_X_MAX  BAT_X_MAX  BAT_X_MIN  BG_TILE_W_PX  BRICK_ATTR_ROW_BASE
    INDICATOR_ROW_BYTES  LEVEL_TIMEOUT_TICKS  ROCKET_W_PX
    SC_ENTER  SC_ESC  SC_P

`LEVEL_TIMEOUT_TICKS` is the other flavour: "~2.2 s per level in the
cycle" describes an attract-mode behaviour the port does not have.

Scope is `src/`, and a use anywhere in `src/`, `tests/` or `scripts/`
counts — a constant read only by a gate is doing its job.
"""

from __future__ import annotations

import ast
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFINE = re.compile(r"^#define\s+([A-Z][A-Z0-9_]{3,})\b", re.M)


def strip_py_prose(text: str) -> str:
    """Remove docstrings and `#` comments from Python source."""
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return text
    drop = set()
    for node in ast.walk(tree):
        body = getattr(node, "body", None)
        if isinstance(body, list) and body \
                and isinstance(body[0], ast.Expr) \
                and isinstance(body[0].value, ast.Constant) \
                and isinstance(body[0].value.value, str):
            for n in range(body[0].lineno - 1, body[0].end_lineno):
                drop.add(n)
    lines = text.split("\n")
    return "\n".join(re.sub(r"#.*$", "", l)
                     for i, l in enumerate(lines) if i not in drop)


def main() -> int:
    defined = {}
    for f in sorted((ROOT / "src").glob("*.cpp")) + \
             sorted((ROOT / "src").glob("*.h")):
        for m in DEFINE.finditer(f.read_text()):
            defined.setdefault(m.group(1), f.name)
    if not defined:
        raise SystemExit("FAIL: found no #define constants in src/ — the "
                         "pattern changed and this gate checks nothing")

    hay = []
    for pat in ("src/*.cpp", "src/*.h", "tests/*.cpp", "scripts/*.py"):
        for f in sorted(ROOT.glob(pat)):
            text = f.read_text(errors="ignore")
            # Drop the definitions themselves so a constant does not
            # count as its own user.
            text = DEFINE.sub("", text)
            if f.suffix == ".py":
                # And drop Python prose. A gate that MENTIONS a constant
                # in its docstring is not using it — this file lists the
                # eleven it first found, which made every one of them
                # look alive and let a reintroduced BAT_X_MAX survive.
                text = strip_py_prose(text)
            hay.append(text)
    blob = "\n".join(hay)

    dead = [n for n in sorted(defined) if not re.search(r"\b" + n + r"\b", blob)]
    if dead:
        print(f"FAIL: {len(dead)} constant(s) defined and never used\n")
        for n in dead:
            print(f"  {n}  ({defined[n]})")
        print("\nA constant nobody reads is a claim the program does not "
              "make. Delete it, or use it.")
        return 1

    print(f"PASS no_dead_constants: all {len(defined)} #define constants in "
          f"src/ are used")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
