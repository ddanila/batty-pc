#!/usr/bin/env python3
"""Deterministic mid-game frame-timeline capture (original side).

The mirror of scripts/capture_frame_timeline.py for the Spectrum game
running in ZEsarUX. It frame-steps the Z80 a fixed number of game frames
between captures so the original-side timeline lines up frame-for-frame
with the port-side one, the missing half of the frame-step parity gate
(see notes/replay-harness.md).

Frame boundary: the main-loop top `LB9E8_2 = $BA83` is reached exactly
once per 50 Hz frame (it sits just past the `EI/HALT/DI` frame wait), so
"advance one frame" == "run until PC hits $BA83 again". To count N
frames we step one opcode off $BA83 (so the breakpoint does not retrap
at zero opcodes — see notes/lessons.md) and run until it re-traps, N
times.

Usage:

    python3 scripts/capture_frame_timeline_original.py \
        --snapshot build/snapshots/20260513T202101Z.sna \
        --frames 0,5,10 --require-motion

`make capture-timeline-original SNAPSHOT=... FRAMES=0,5,10` wires it.

Exit code 0 when every checkpoint produced a capture; nonzero on a
missing capture. With --require-motion, also fails if consecutive
captures are identical (proving the Z80 actually advanced).
"""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import ZrcpClient, launch_emulator
from extract_scr import decode as decode_scr
from replay_harness import write_indices, apply_original_setup
from test_visual import PLAYFIELD_W, PLAYFIELD_H

FRAME_PC = 0xBA83          # LB9E8_2, main-loop top, once per 50 Hz frame
RUN_MAX = 5_000_000        # safety cap; the breakpoint trips long before


def run_to_frame_pc(zc: ZrcpClient, frame_pc: int) -> None:
    """Run until PC == frame_pc (a frame boundary), then hold in cpu-step."""
    zc.enable_breakpoints()
    zc.set_breakpoint(1, f"PC={frame_pc:04X}H")
    zc.run(RUN_MAX, no_stop_on_data=True, timeout=30.0)
    pc = zc.get_registers().get("PC", -1)
    if pc != frame_pc:
        raise RuntimeError(f"failed to reach frame PC ${frame_pc:04X}; got ${pc:04X}")


def frame_step(zc: ZrcpClient, n: int, frame_pc: int) -> None:
    """Advance exactly n game frames. Assumes PC is currently at frame_pc."""
    # Tolerant enable: ZEsarUX errors if breakpoints are already on, and a
    # preceding setup's run_until_pc may have left them enabled.
    zc.command("enable-breakpoints", allow_error=True)
    zc.set_breakpoint(1, f"PC={frame_pc:04X}H")
    for _ in range(n):
        zc.run(1, no_stop_on_data=True, timeout=10.0)            # step off frame_pc
        zc.run(RUN_MAX, no_stop_on_data=True, timeout=30.0)      # run until it re-traps
        pc = zc.get_registers().get("PC", -1)
        if pc != frame_pc:
            raise RuntimeError(
                f"frame step landed at ${pc:04X}, expected ${frame_pc:04X}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--snapshot', default='build/snapshots/20260513T202101Z.sna')
    ap.add_argument('--frames', default='0,5,10',
                    help='comma-separated ascending absolute frame indices, '
                         'relative to the first frame boundary after load')
    ap.add_argument('--frame-pc', type=lambda s: int(s, 0), default=FRAME_PC)
    ap.add_argument('--out', default='build/frame_timeline_original')
    ap.add_argument('--zesarux', default='tools/zesarux/src/zesarux')
    ap.add_argument('--zrcp-port', type=int, default=10000)
    ap.add_argument('--require-motion', action='store_true')
    ap.add_argument('--setup-from-replay',
                    help='apply this replay spec\'s original.setup before '
                         'stepping (pokes level/RNG, jumps into active play); '
                         'reuses replay_harness.apply_original_setup')
    args = ap.parse_args()

    frames = [int(t) for t in args.frames.split(',') if t.strip()]
    if not frames or frames != sorted(frames) or len(set(frames)) != len(frames):
        print(f'frames must be a strictly-ascending list, got {args.frames!r}')
        return 2

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    proc, zc = launch_emulator(args.zesarux, machine="48k",
                               extra_args=[], port=args.zrcp_port, headless=True)
    captures = []
    try:
        zc.snapshot_load(str(Path(args.snapshot).resolve()))
        if args.setup_from_replay:
            spec = json.loads(Path(args.setup_from_replay).read_text())
            original = spec.get("original", {})
            # Let ZEsarUX settle / drop its menu before enter-cpu-step,
            # mirroring replay_harness.run_original.
            time.sleep(float(original.get("boot_wait", 0.5)))
            apply_original_setup(zc, original.get("setup", []))
        # Land on the first frame boundary; checkpoints count from here.
        # Skip if the setup already left us parked on it (e.g. its trailing
        # run_until_pc landed at frame_pc) — re-running would retrap at 0
        # opcodes (see notes/lessons.md).
        if zc.get_registers().get("PC", -1) != args.frame_pc:
            run_to_frame_pc(zc, args.frame_pc)
        prev = 0
        for n in frames:
            if n > prev:
                frame_step(zc, n - prev, args.frame_pc)
                prev = n
            scr = out / f'frame_{n:04d}.scr'
            zc.save_screen(str(scr.resolve()))
            idx = decode_scr(scr.read_bytes())
            (out / f'frame_{n:04d}.idx').write_bytes(idx)
            captures.append((n, scr, idx))
    finally:
        zc.exit_emulator()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()

    rc = 0
    prev_idx = None
    prev_n = None
    for n, scr, idx in captures:
        if prev_idx is None:
            print(f'  frame {n:>4}: captured ({PLAYFIELD_W}x{PLAYFIELD_H})')
        else:
            diff = sum(1 for a, b in zip(idx, prev_idx) if a != b)
            tag = 'OK' if diff > 0 else 'STATIC'
            print(f'  frame {prev_n:>4} -> {n:>4}: {diff:>6} px changed [{tag}]')
            if diff == 0 and args.require_motion:
                rc = 1
        prev_idx = idx
        prev_n = n
    if len(captures) != len(frames):
        rc = 1

    print('PASS: original-side timeline captured'
          if rc == 0 else 'FAIL: missing or static captures')
    return rc


if __name__ == '__main__':
    sys.exit(main())
