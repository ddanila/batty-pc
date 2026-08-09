#!/usr/bin/env python3
"""Run a QEMU gate at every counter phase and report whether it cares.

    scripts/phase_sweep.py test-enemy-anim [more-gates...]

A REPORT, not a gate. Run it on a new QEMU gate before trusting it.

### The problem it exists for

`pit_frame_counter` — the port's stand-in for the original's
`counter_misc` — free-runs from boot, and several cadences key off its
low bits: the enemy steer (`& 3`), the ball speed ramp (`& 7`). How long
boot took therefore decides which phase a probe frame lands on. A gate
whose expectations depend on that phase passes or fails by luck.

`test-enemy-descend` did exactly this and failed about two runs in three
(known-bugs #17). Running it a few times is how it was FOUND, but
repetition is a weak instrument: it only samples whatever phases the
machine happened to produce, and a gate can pass three times and still
be a coin flip on the fourth.

This varies the phase deliberately instead. `BATTY_REPLAY_COUNTER` pins
the counter at the aligned start (`pin_replay_frame_counter`, called
from `enter_level`), so running a gate at phases 0..3 covers every case
`& 3` can produce. A gate that passes at all four does not depend on the
phase. A gate that fails at some was passing by luck.

### Gates that pin the counter themselves

Those set `BATTY_REPLAY_COUNTER` in their own env string, which wins
over the outer environment — so sweeping them would test the same phase
four times and report a confident, meaningless "phase-independent".
That is detected and reported instead of run: a check that cannot fail
is worse than no check, which is a lesson this repo has had to learn
more than once.
"""

from __future__ import annotations

import ast
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PHASES = ("0", "1", "2", "3")


def pins_counter(script: Path) -> bool:
    """Does the gate SET the pin, as opposed to merely writing about it?

    The first version of this searched the whole file, and so skipped
    any gate whose docstring merely explains BATTY_REPLAY_COUNTER —
    which the gates that need explaining all do. It reported "SKIPPED"
    where it should have swept: a false negative wearing the costume of
    a decision. Docstrings and comments are stripped first.
    """
    text = script.read_text()
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return "BATTY_REPLAY_COUNTER=" in text
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.AsyncFunctionDef,
                             ast.ClassDef)):
            body = getattr(node, "body", [])
            if body and isinstance(body[0], ast.Expr) \
                    and isinstance(body[0].value, ast.Constant) \
                    and isinstance(body[0].value.value, str):
                docstrings.add(id(body[0]))
    lines = text.split("\n")
    drop = set()
    for node in ast.walk(tree):
        if id(node) in docstrings:
            for n in range(node.lineno - 1, node.end_lineno):
                drop.add(n)
    code = "\n".join(re.sub(r"#.*$", "", ln)
                     for i, ln in enumerate(lines) if i not in drop)
    return "BATTY_REPLAY_COUNTER=" in code


def script_for(target: str):
    """The script a make target runs, if it is a one-line python gate."""
    mk = (ROOT / "Makefile").read_text()
    m = re.search(r"^" + re.escape(target) + r":.*?\n((?:\t.*\n)*)", mk, re.M)
    if not m:
        return None
    s = re.search(r"python3 (scripts/\S+\.py)", m.group(1))
    return ROOT / s.group(1) if s else None


def main() -> int:
    targets = sys.argv[1:]
    if not targets:
        print(__doc__)
        return 0

    for target in targets:
        script = script_for(target)
        if script is not None and script.exists() and pins_counter(script):
            print(f"{target}: SKIPPED — it sets BATTY_REPLAY_COUNTER in its "
                  f"own env, which overrides this sweep. Every phase here "
                  f"would run the same pin and the result would mean "
                  f"nothing.")
            continue

        results = []
        for phase in PHASES:
            env = dict(os.environ, BATTY_REPLAY_COUNTER=phase)
            r = subprocess.run(["make", "-s", target], cwd=ROOT, env=env,
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
            results.append((phase, r.returncode == 0))

        bad = [p for p, ok in results if not ok]
        detail = " ".join(f"pin{p}={'pass' if ok else 'FAIL'}"
                          for p, ok in results)
        if bad:
            print(f"{target}: PHASE-DEPENDENT at pin {', '.join(bad)} "
                  f"-- {detail}")
            print(f"    Either pin the counter in the gate's own env "
                  f"(BATTY_REPLAY_COUNTER=0), or assert something the "
                  f"phase cannot move. See known-bugs #17.")
        else:
            print(f"{target}: phase-independent -- {detail}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
