#!/usr/bin/env python3
"""Every host test suite must actually run.

`tests/test_replay_parse.cpp` had seven tests and a working make target,
and `make test-fast` never ran it. It was reachable only from
`parity-check`, the full QEMU suite — so the seven tests guarding the
BATTY_REPLAY_* value formats ran once every six minutes instead of once
every few seconds, and nothing said so.

The cause is two hand-maintained lists of the same thing that drifted:
`test-fast`'s prerequisites and `parity-check`'s recipe. When
`test_replay.cpp` was added it went into the first and not the second;
`test_replay_parse.cpp` was in the second and not the first. Each list
looked complete on its own.

A suite that exists but does not run is the same defect as a knob that
does not reach DOS (`check_env_passthrough.py`): nothing errors, the
green tick still appears, and the coverage silently is not there.

Naming is not mechanical here, so the aliases below are explicit rather
than guessed. `tests/test_zxvga.cpp` is `make test-video`, `test_hud.cpp`
is `test-hud-unit`. Any suite whose target cannot be resolved is
reported, not assumed fine.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests"
MAKEFILE = ROOT / "Makefile"

# tests/test_<stem>.cpp -> make target, where the two differ.
ALIASES = {
    "zxvga": "test-video",
    "hud": "test-hud-unit",
}


def target_for(stem: str) -> str:
    return ALIASES.get(stem, "test-" + stem.replace("_", "-"))


def expand_vars(text: str, line: str) -> str:
    """Substitute one level of $(NAME) from the Makefile's own definitions.

    The prerequisite list moved into $(HOST_TEST_TARGETS) so that test-asan
    could share it instead of keeping a second copy — and a second copy is
    exactly the drift this gate exists to catch, which it would have missed
    because it reads test-fast's rule and only that rule.

    One level is deliberate. It is enough for a list-of-targets variable,
    and a general Make expander is a project of its own. If a definition is
    ever missing, the $(NAME) simply survives into the output, matches no
    `test-` target, and the gate fails loudly rather than silently passing.
    """
    for name in set(re.findall(r"\$\(([A-Z_][A-Z0-9_]*)\)", line)):
        m = re.search(rf"^{name}\s*[:?]?=(.*?)(?<!\\)\n", text, re.S | re.M)
        if m:
            line = line.replace(f"$({name})", m.group(1))
    return line


def test_fast_prereqs(text: str) -> set[str]:
    m = re.search(r"^test-fast:(.*?)\n\t", text, re.S | re.M)
    if not m:
        raise SystemExit("FAIL: no test-fast rule found; this gate reads its "
                         "prerequisite list and cannot work without it")
    line = expand_vars(text, m.group(1)).replace("\\\n", " ")
    return set(re.findall(r"test-[a-z0-9-]+", line))


def main() -> int:
    text = MAKEFILE.read_text()
    prereqs = test_fast_prereqs(text)
    suites = sorted(p.stem[len("test_"):] for p in TESTS.glob("test_*.cpp"))
    if not suites:
        raise SystemExit("FAIL: no tests/test_*.cpp found at all")

    missing, no_target = [], []
    for stem in suites:
        target = target_for(stem)
        if not re.search(r"^" + re.escape(target) + r":", text, re.M):
            no_target.append((stem, target))
        elif target not in prereqs:
            missing.append((stem, target))

    if missing or no_target:
        print("FAIL: a host test suite does not run under `make test-fast`\n")
        for stem, target in no_target:
            print(f"  tests/test_{stem}.cpp has NO make target")
            print(f"    expected `{target}:` — add the rule, or add the real "
                  f"name to ALIASES in this gate.")
        for stem, target in missing:
            print(f"  tests/test_{stem}.cpp builds via `{target}` but that is "
                  f"not a test-fast prerequisite")
            print(f"    -> add {target} to the test-fast rule. Until then the "
                  f"suite runs only if some slower target happens to invoke "
                  f"it, which is how test_replay_parse.cpp went unrun.")
        return 1

    print(f"PASS host_tests_wired: all {len(suites)} suites run under "
          f"test-fast")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
