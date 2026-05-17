#!/usr/bin/env python3
"""Live-trace the gameplay blitter via ZEsarUX + ZRCP.

Restores snap3 (level 1 in-progress) directly into a freshly-launched
ZEsarUX, sets a memory-write watchpoint on a VRAM pixel byte that's
guaranteed to be written each frame, runs for one frame, and reports
the PC at the moment of the trap. That PC sits inside (or just past)
the screen sprite blitter.

Usage:
    python3 scripts/trace_blitter.py [--snap DIR] [--wp ADDR] [--keep]

`--keep` leaves ZEsarUX running afterwards for manual inspection
(connect with `nc localhost 10000`).
"""
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import launch_emulator


# Default: locally-built submodule binary. Override with ZESARUX=...
_REPO_ROOT = Path(__file__).resolve().parent.parent
ZESARUX = os.environ.get("ZESARUX", str(_REPO_ROOT / "tools/zesarux/src/zesarux"))
DEFAULT_SNAP = "build/snapshots/20260513T202101Z"
DEFAULT_WP_ADDR = 0xE800   # mid sprite cache; blitter reads each frame


def restore_snapshot(zc, snap_dir: Path):
    """Build a .sna from snap_dir and snapshot-load it into ZEsarUX.
    Way faster than byte-streaming RAM via write_memory (which took
    ~minutes for 48K)."""
    sna_path = snap_dir.parent / f"{snap_dir.name}.sna"
    if not sna_path.exists() or sna_path.stat().st_mtime < (snap_dir / 'ram_4000_FFFF.bin').stat().st_mtime:
        subprocess.run(['python3', str(Path(__file__).parent / 'snap_to_sna.py'),
                        str(snap_dir), str(sna_path)], check=True)
    print(f"  snapshot-load {sna_path}")
    print(f"  -> {zc.snapshot_load(str(sna_path.resolve()))}")
    out = zc.get_registers()
    print(f"  PC after load = 0x{out.get('PC', -1):04X}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--snap', default=DEFAULT_SNAP,
                    help=f'snapshot directory (default: {DEFAULT_SNAP})')
    ap.add_argument('--wp-addr', type=lambda s: int(s, 0), default=DEFAULT_WP_ADDR,
                    help='memory write watchpoint address (default: 0x4100)')
    ap.add_argument('--tstates', type=int, default=120_000,
                    help='t-states to run after restoring (default: ~1.7 frames @ 70k/frame)')
    ap.add_argument('--keep', action='store_true',
                    help='leave ZEsarUX running afterwards')
    args = ap.parse_args()

    snap_dir = Path(args.snap).resolve()
    assert snap_dir.is_dir(), f"no such snapshot dir: {snap_dir}"

    print(f"launching ZEsarUX (headless)...")
    proc, zc = launch_emulator(str(ZESARUX), machine='48k',
                               extra_args=[], port=10000, headless=True)
    try:
        ver = zc.get_version()
        print(f"  connected: {ver}")

        print(f"restoring snapshot {snap_dir.name}...")
        restore_snapshot(zc, snap_dir)

        # Pause; both `set-breakpoint` and `run verbose N` want us in step mode.
        zc.enter_cpu_step()

        wp = f"MRA={args.wp_addr:04X}H"
        print(f"setting memory-read watchpoint: {wp}")
        zc.set_breakpoint(1, wp)
        zc.enable_breakpoints()

        # Resume; the watchpoint will halt the CPU. Poll get-registers
        # until PC stops at our breakpoint address (or any address other
        # than 0x0038 / 0xBB38 / ROM idle loop addresses).
        print(f"resuming; polling for breakpoint trip...")
        zc.exit_cpu_step()
        DEADLINE = time.time() + 5.0
        trapped = False
        while time.time() < DEADLINE:
            time.sleep(0.05)
            regs_now = zc.get_registers()
            # If ZEsarUX paused itself on the BP, PC will be at the
            # instruction _after_ the trap (most ZRCP builds; some
            # halt _on_ the trap address itself). Either way, PC won't
            # equal the running-state addresses.
            if regs_now.get('PC', 0) not in (0x0038, 0xBB37, 0xBB38):
                trapped = True
                break
        print(f"  trapped={trapped} PC=0x{regs_now.get('PC',0):04X}")
        resp = "(polled)"
        # `run verbose` returns end-state info incl. the breakpoint that fired.
        print("--- run result ---")
        print(resp)
        print("------------------")

        # Re-pause and dump PC + a disassembly window
        zc.enter_cpu_step()
        regs_now = zc.get_registers()
        pc = regs_now.get('PC', 0)
        print(f"PC at pause = 0x{pc:04X}")
        print(f"AF={regs_now.get('AF',0):04X} BC={regs_now.get('BC',0):04X} "
              f"DE={regs_now.get('DE',0):04X} HL={regs_now.get('HL',0):04X} "
              f"IX={regs_now.get('IX',0):04X} IY={regs_now.get('IY',0):04X} "
              f"SP={regs_now.get('SP',0):04X}")
        print("--- disasm window (PC-4 .. PC+12) ---")
        for offset in range(-4, 13, 1):
            try:
                d = zc.disassemble(pc + offset, 1)
                print(f"  0x{pc+offset:04X}: {d}")
            except Exception as e:
                print(f"  0x{pc+offset:04X}: <{e}>")
                break
    finally:
        if args.keep:
            print("leaving ZEsarUX running; connect: nc localhost 10000")
        else:
            try: zc.exit_emulator()
            except Exception: pass
            proc.wait(timeout=5)


if __name__ == "__main__":
    main()
