#!/usr/bin/env python3
"""Each debug switch's documented default must match the initialiser.

The RNG-model note opened with "OFF by default: the port advances the
RNG on demand" for two months after the default flipped to ON — while
the same comment said "Now the DEFAULT (2026-06-05)" ten lines lower and
the initialiser agreed with the second. A reader who stopped at the
opening line, which is what an opening line is for, got the wrong
answer. notes/parity-gaps.md then listed the resulting "gap" as its TOP
PRIORITY for two months.

That is not a typo class. A wrong default sends someone to fix code that
is already correct, or to trust behaviour the port does not have.

Each field in DebugSwitches carries `default=0` or `default=1`. This
compares those against the `dbg = { ... }` initialiser positionally, so
adding a field without a default, or reordering the initialiser, fails.

It cannot check the long prose notes elsewhere in the file — those are
where the RNG claim actually lived. What it can do is make the STRUCT
the single place a default is written down, so prose that disagrees has
something adjacent and checked to disagree with.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"


def main() -> int:
    text = SRC.read_text()
    m = re.search(r"struct DebugSwitches \{(.*?)\n\};", text, re.S)
    if not m:
        raise SystemExit("FAIL: struct DebugSwitches not found; if it was "
                         "renamed, point this gate at the new name")
    fields = []
    for line in m.group(1).split("\n"):
        fm = re.match(r"\s*unsigned \w+\s+(\w+);\s*/\*(.*)\*/", line)
        if not fm:
            continue
        dm = re.search(r"default=([01])", fm.group(2))
        fields.append((fm.group(1), None if not dm else int(dm.group(1))))

    im = re.search(r"DebugSwitches dbg\s*=\s*\{([^}]*)\}", text)
    if not im:
        raise SystemExit("FAIL: the dbg initialiser was not found")
    values = [int(v.strip()) for v in im.group(1).split(",") if v.strip()]

    bad = []
    if len(fields) != len(values):
        bad.append(f"{len(fields)} fields but {len(values)} initialiser "
                   f"values — one was added without the other")
    else:
        for (name, want), got in zip(fields, values):
            if want is None:
                bad.append(f"{name} has no `default=` in its comment, so "
                           f"nothing states what it should be")
            elif want != got:
                bad.append(f"{name} is documented default={want} but the "
                           f"initialiser sets {got}")

    if bad:
        print("FAIL: a debug switch's documented default is wrong\n")
        for b in bad:
            print(f"  - {b}")
        print("\nA wrong default sends someone to fix code that is already "
              "correct — see notes/parity-gaps.md's top priority, which "
              "was closed for two months.")
        return 1

    print(f"PASS switch_defaults: {len(fields)} switches match the "
          f"initialiser")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
