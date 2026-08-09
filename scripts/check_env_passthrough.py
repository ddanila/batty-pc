#!/usr/bin/env python3
"""Every BATTY_* knob must actually reach DOS on the test floppy.

This is the gate that would have saved a run. `BATTY_REPLAY_LIVES` was
added to `src/main.cpp`, set in a gate's environment, and did nothing —
because the test floppy bakes a HAND-MAINTAINED list of `SET` lines into
`AUTOEXEC.BAT`, and a variable missing from that list never crosses into
the emulated machine.

That failure mode is nastier than it sounds. The knob is not reported
missing; it is simply absent, so the gate quietly runs a DIFFERENT
SCENARIO than the one it names. Whether that is caught depends entirely
on what the gate asserts. A gate checking "did the port not crash" or
"is the ball where I left it" would pass while testing nothing it claims
to. The one that caught it did so only because it asserted on screen
content that could not possibly appear in the wrong scenario.

So this checks both directions:

  MISSING  — a knob `src` reads that the floppy does not pass through.
             Gates using it silently test the wrong thing.
  ORPHANED — a `SET` line for a knob `src` no longer reads. Harmless at
             runtime, but it is a passthrough someone will copy as a
             template, and it makes the list look maintained when it is
             not.

Knob names are taken from every `"BATTY_..."` STRING LITERAL in every
`src/*.cpp`, not from `getenv(...)` call sites. Several are read indirectly —
`replay_env_ints("BATTY_REPLAY_BOMB", v, 2)` and
`replay_apply_object("BATTY_REPLAY_BALL_OBJECT", ...)` both reach
`getenv` one level down, and scanning only `getenv(` reported nine
false orphans on the first attempt.

Scope is the TEST floppy. The main floppy's list is deliberately shorter:
it is the one a person boots, not the one gates drive.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "src"
MAKEFILE = ROOT / "Makefile"

# Knobs that intentionally do not reach the test floppy. Add a REASON, or
# the next person cannot tell a decision from an oversight.
EXEMPT: dict[str, str] = {}


def knobs_in_source() -> set[str]:
    """Every .cpp under src/, not just main.cpp.

    Scanning main.cpp alone was right until the replay overrides moved to
    src/replay.cpp, at which point three knobs read there were reported
    as ORPHANED. The gate was correct about its own scope being wrong,
    which is the useful failure — a gate that had scanned for GETENV call
    sites instead would have gone quiet."""
    found: set[str] = set()
    for f in sorted(SRC_DIR.glob("*.cpp")):
        found |= set(re.findall(r'"(BATTY_[A-Z_0-9]+)"', f.read_text()))
    return found


def knobs_on_test_floppy() -> set[str]:
    found: set[str] = set()
    for line in MAKEFILE.read_text().split("\n"):
        if "AUTOEXEC_T" in line:
            found |= set(re.findall(r"SET (BATTY_[A-Z_0-9]*)=", line))
    return found


def main() -> int:
    src = knobs_in_source()
    floppy = knobs_on_test_floppy()

    if not src or not floppy:
        raise SystemExit(
            f"FAIL: found {len(src)} knobs in src and {len(floppy)} on the "
            f"test floppy — one of the two scans matched nothing, so this "
            f"gate is not checking anything. Did the Makefile's AUTOEXEC_T "
            f"block or the BATTY_ naming change?")

    missing = sorted(k for k in src - floppy if k not in EXEMPT)
    orphaned = sorted(floppy - src)

    if missing or orphaned:
        print("FAIL: the test floppy's env passthrough is out of sync "
              "with the source\n")
        for k in missing:
            print(f"  MISSING  {k}")
            print(f"           src reads it, the floppy does not pass it. A "
                  f"gate setting it runs a DIFFERENT scenario, silently.")
            print(f"           -> add a `SET {k}` line to the AUTOEXEC_T "
                  f"block in the Makefile,")
            print(f"              or add it to EXEMPT here with a reason.")
        for k in orphaned:
            print(f"  ORPHANED {k}")
            print(f"           the floppy passes it through, no source "
                  f"literal names it. Remove the SET line, or fix the name "
                  f"if it was a typo.")
        print(f"\n{len(src)} knobs in src, {len(floppy)} on the test floppy.")
        return 1

    note = f", {len(EXEMPT)} exempt" if EXEMPT else ""
    print(f"PASS env_passthrough: all {len(src)} BATTY_* knobs reach the "
          f"test floppy{note}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
