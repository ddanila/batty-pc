#!/usr/bin/env python3
"""Run independent (QEMU-only) gate targets concurrently, each on its OWN
floppy image, to cut the boot-dominated serial suite wall-clock by ~N.

The gates are unchanged — same builds, same assertions — they just run in
parallel. Isolation: each gate gets BATTY_TEST_FLOPPY=build/batty-test-<i>.img;
the Makefile's TEST_FLOPPY_OUT honours that env var and derives a unique
AUTOEXEC scratch, so concurrent floppy assembly never collides. The shared
TEST_EXE is pre-built once up front so the workers don't race on it.

ZEsarUX gates are EXCLUDED: they drive a single ZRCP port (10000) and a
shared snapshot, so they can't run concurrently — run those serially via the
normal `make parity-check-full`. The fast core (`parity-check`) is entirely
QEMU-only and fully parallelizable.

    make parity-check-parallel J=8
    python3 scripts/run_gates_parallel.py --jobs 8 [--full]

CONTENTION IS NOT FAILURE. A gate is not one boot: test-ball-no-tunnel
boots dozens of times, test-levels-sweep fifteen. Run enough of those at
once and QEMU falls below real time, the wall-clock waits land early, and
gates report things like "NO bonus_state in PROBE.TXT" or abort in 0.1s.
Those look exactly like regressions and are not.

Two defences, because a suite nobody trusts is a suite nobody runs:

  * --full halves the default job count, since it is the run that pulls in
    the multi-boot gates (a quarter was tried and is too slow to finish);
  * ANY failure is retried once, alone, with nothing else running. Only a
    gate that fails twice is reported as failed, and gates that needed the
    retry are named — a rising count there means the job count is too high
    for this machine.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import os
import subprocess
import sys
import time


def say(msg: str = "") -> None:
    """Print immediately. Piped to a file, Python buffers stdout, so a run
    that is interrupted or watched from another terminal shows nothing."""
    print(msg, flush=True)

# Fast-core gates (== parity-check), all ZEsarUX-free.
PARITY_CHECK_GATES = [
    "test",
    "test-laffc-ball-frame1",
    "test-bat-deflection",
    "test-enemy-descend",
    "test-rng-walk",
    "test-enemy-steer",
    "test-ball-no-tunnel",
    "test-enemy-attr-parity",
]

# Additional QEMU-only gates from parity-check-full (ZEsarUX gates
# test-frame-step / replay-l3-entry / capture-timeline-both /
# replay-l3-brick-flash-both are intentionally omitted — run them serially).
FULL_EXTRA_GATES = [
    "test-wall-bounce", "test-magnet-ball", "test-brik-anim-pace",
    "test-levels-sweep", "test-enemy-flyover-redraw", "test-normal-ball-launch",
    "test-laffc-levels-sane", "test-hud", "test-round-banner-border",
    "test-brick-flash", "test-death-sparks", "test-rocket-bonus",
    "test-game-over",
    "test-game-over-visual", "test-name-entry-visual", "test-life-loss", "test-level-advance", "test-stuck-ball-offset", "test-invariant-owners",
    "test-bonus-fall", "test-bomb-fall", "test-pts400-fall", "test-bullet-fly",
    "test-laser-cadence", "test-enemy-anim", "test-bonus-drop",
    "test-bonus-effects", "test-bonus-effects2", "test-bonus-typepick",
    "test-bullet-blast", "test-brick-scoring", "test-ball-speed-ramp",
    "test-rocket-completion-no-ball", "test-rocket-flight-redraw",
    "test-ball-dirty-redraw", "test-ball-object-dirty-redraw",
    "test-bullet-dirty-redraw", "test-bomb-dirty-redraw",
    "test-blast-dirty-redraw", "test-visual-checkpoints",
    "test-bat-fire-dirty-redraw", "test-multiball-dirty-redraw",
    "test-bigball-dirty-redraw", "test-stuck-ball-dirty-redraw",
    "test-enemy-brick-residue", "test-enemy-brick-walk", "test-enemy-margin-clamp",
    "test-two-player-turn", "test-double-play-court", "test-double-play-bat2", "test-double-play-input", "test-double-play-alien-kill",
    "test-stuck-auto-launch",
    "test-bat-redraw-window",
    "test-ball-left-wall-escape", "test-l3-replay-seed",
    "test-ball-paths-no-tunnel", "test-sprite-attr-parity",
    "test-laffc-ball-l5-metal",
]


def run_gate(gate: str, idx: int) -> dict:
    # BATTY_SERIAL_PROBE makes the capture_frame_timeline gates wait for the
    # port's COM1 frame-reached marker instead of a wall-clock sleep, so they
    # stay frame-exact even when the concurrent fan-out slows each QEMU below
    # real time (the failure mode that broke them under oversubscription/TCG).
    env = dict(os.environ,
               BATTY_TEST_FLOPPY=f"build/batty-test-{idx}.img",
               BATTY_SERIAL_PROBE="1")
    t0 = time.time()
    p = subprocess.run(["make", gate], env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    dt = time.time() - t0
    tail = "\n".join(p.stdout.strip().splitlines()[-3:])
    return {"gate": gate, "rc": p.returncode, "dt": dt, "tail": tail}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=0,
                    help="0 = pick from core count and whether --full is set")
    ap.add_argument("--full", action="store_true",
                    help="also run the parity-check-full QEMU-only gates")
    ap.add_argument("--gates", default="",
                    help="comma-separated gate override")
    args = ap.parse_args()

    if args.gates:
        gates = [g for g in args.gates.split(",") if g.strip()]
    else:
        gates = list(PARITY_CHECK_GATES)
        if args.full:
            gates += FULL_EXTRA_GATES

    cpus = os.cpu_count() or 4
    if args.jobs <= 0:
        # The full run includes gates that boot QEMU many times each, so its
        # concurrency has to be lower than the core count would suggest.
        args.jobs = max(2, cpus // 2) if args.full else max(2, cpus - 2)

    say(f"pre-building the test EXE once (shared, read-only during the run)...")
    pb = subprocess.run(["make", "build/batty-test.exe"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    if pb.returncode != 0:
        say("FAIL: could not build build/batty-test.exe")
        return 2

    say(f"running {len(gates)} gates, {args.jobs} at a time on {cpus} cores, "
          f"each on its own floppy")
    say("failures are retried once alone before being believed\n")
    t0 = time.time()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(run_gate, g, i): g for i, g in enumerate(gates)}
        for fut in concurrent.futures.as_completed(futs):
            r = fut.result()
            tag = "PASS" if r["rc"] == 0 else "FAIL"
            say(f"  [{tag}] {r['gate']:<34} {r['dt']:6.1f}s")
            if r["rc"] != 0:
                for ln in r["tail"].splitlines():
                    say(f"         | {ln}")
            results.append(r)
    wall = time.time() - t0

    # Retry every failure serially, with the machine to itself. A gate that
    # passes on the retry was starved, not broken.
    flaky = []
    fails = []
    for r in [r for r in results if r["rc"] != 0]:
        say(f"\n  retrying {r['gate']} alone...")
        again = run_gate(r["gate"], 0)
        if again["rc"] == 0:
            say(f"  [PASS] {r['gate']:<34} {again['dt']:6.1f}s  (starved when parallel)")
            r["rc"] = 0
            flaky.append(r["gate"])
        else:
            say(f"  [FAIL] {r['gate']:<34} {again['dt']:6.1f}s  (failed alone too)")
            for ln in again["tail"].splitlines():
                say(f"         | {ln}")
            fails.append(r)

    serial = sum(r["dt"] for r in results)
    say(f"\n{len(results)} gates in {wall:.1f}s wall "
          f"(serial would be ~{serial:.0f}s; {serial / wall:.1f}x speedup)")
    if flaky:
        say(f"\n{len(flaky)} gate(s) only passed on the serial retry: "
              f"{', '.join(flaky)}")
        say(f"  -> {args.jobs} jobs is too many for {cpus} cores here; "
              f"lower J if this list grows")
    if fails:
        say(f"\nFAIL parity-check-parallel: {len(fails)} gate(s) failed "
              f"BOTH in parallel and alone: "
              f"{', '.join(r['gate'] for r in fails)}")
        return 1
    say(f"\nPASS parity-check-parallel: all {len(results)} gates green")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
