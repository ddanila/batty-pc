#!/usr/bin/env python3
"""Nothing may TIME anything with bios_ticks().

known-bugs.md #15: `bios_ticks()` does not advance during gameplay.
Measured, not assumed — a probe latching both clocks at the first
gameplay frame and again at a checkpoint reported `dbios0_dpit678`, i.e.
678 PIT frames (~13.6 s at 50 Hz) with the BIOS counter frozen.

Two things had been built on it and both were broken in ways nobody had
noticed, because a player always presses a key: the game-over screen
waited forever, and the name-entry cursor never blinked. Both now count
PIT frames.

The BIOS counter is deliberately still read. `run_title`, `run_menu` and
`run_hiscore` seed a variable from it and test that variable with
TIMED_OUT, which `auto_advance` keeps permanently false. Those are kept
as a record of the cycle the original's screens WOULD use, and they are
harmless precisely because nothing acts on the result.

So the rule is not "never call bios_ticks". It is: **never compute with
it**. A bare assignment is inert. An arithmetic or comparison is a live
timing dependency on a clock that does not move, and that is exactly the
shape #15 was.

Allowed uses:
  - the forward declaration and the definition
  - the TIMED_OUT macro itself
  - a plain `x = bios_ticks();` assignment
  - write_replay_probe, which reports the counter as a MEASUREMENT — the
    instrument that settled #15 and the one place computing a delta is
    the entire point

Anything else fails, with #15 quoted at whoever wrote it.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "main.cpp"

ASSIGN = re.compile(r"^\s*[\w.]+\s*=\s*bios_ticks\(\);\s*$")
DECL = re.compile(r"^\s*static unsigned long bios_ticks\(void\)")
MACRO = re.compile(r"^\s*#define\s+TIMED_OUT")


def strip_comments(text: str) -> str:
    """Blank out comments, KEEPING line numbers so reports point at real
    lines. Comments discuss bios_ticks constantly — this file's own
    explanations would trip the check otherwise."""
    out = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"),
                 text, flags=re.S)
    return re.sub(r"//[^\n]*", "", out)


def probe_span(lines: list[str]) -> tuple[int, int]:
    """Line range of write_replay_probe, the one sanctioned computer of
    bios deltas."""
    for i, l in enumerate(lines):
        if l.startswith("static void write_replay_probe(void) {"):
            for j in range(i, len(lines)):
                if lines[j] == "}":
                    return i, j
    return -1, -1


def main() -> int:
    raw = SRC.read_text()
    lines = strip_comments(raw).split("\n")
    lo, hi = probe_span(lines)
    if lo < 0:
        raise SystemExit("FAIL: write_replay_probe not found; this gate "
                         "cannot tell the instrument from a real use")

    bad = []
    seen = 0
    for n, line in enumerate(lines):
        if "bios_ticks(" not in line:
            continue
        seen += 1
        if DECL.match(line) or MACRO.match(line) or ASSIGN.match(line):
            continue
        if line.strip().startswith("static unsigned long bios_ticks(void);"):
            continue
        if lo <= n <= hi:
            continue
        bad.append((n + 1, line.strip()))

    if seen == 0:
        raise SystemExit(
            "FAIL: no bios_ticks( occurrences at all. If the function was "
            "removed, delete this gate and the #15 note with it; if it was "
            "renamed, update both.")

    if bad:
        print("FAIL: something computes with bios_ticks(), which does not "
              "advance during gameplay\n")
        for n, line in bad:
            print(f"  src/main.cpp:{n}")
            print(f"    {line}")
        print("\nSee notes/known-bugs.md #15. A bare `x = bios_ticks();` is "
              "inert — the TIMED_OUT screens do that and act on nothing, "
              "since auto_advance is never assigned. Subtracting or "
              "comparing is a live dependency on a frozen clock: that is "
              "how the game-over hold became infinite and blink_phase() "
              "became a constant. Count PIT frames instead (pit_ticks(), "
              "~50 Hz): 18.2 BIOS ticks per second, so multiply by 2.747.")
        return 1

    print(f"PASS frozen_clock: {seen} bios_ticks references, none of them "
          f"timing anything")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
