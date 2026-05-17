#!/usr/bin/env python3
"""Capture all 15 levels using the modded Batty .sna built by
build_modded_batty.py.

The modded source:
  - skips menu, banner, pause_long, metal_anim
  - hides HUD score labels + digits, lives indicator
  - sets object_bat_1+$14 = $09 (kill-aliens) so no bird/UFO ever spawns
  - traps the CPU with `DI; HALT; NOP` at the start of LB9E8_2 (gameplay loop)
    -> after level-init the CPU halts at PC=0xBA85 and stays there forever

Per-level flow:
  1. (first iter only) boot ZEsarUX with modded.sna -> halts at L1
  2. snap screen
  3. write level counter ($B7EA) = next level
  4. set PC = 0xBA24 (= CALL briks_calc, re-runs full level-init flow)
  5. resume; wait for PC to settle at 0xBA85 (halted again)
  6. -> step 2

Total time should be ~5-10s for all 15 (no tape boot, no banner pause, no metal anim).
"""
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zrcp import launch_emulator


ROOT = Path(__file__).resolve().parent.parent
# Default: locally-built submodule binary. Override with ZESARUX=...
ZESARUX = os.environ.get('ZESARUX', str(ROOT / 'tools/zesarux/src/zesarux'))
SNA     = ROOT / 'build' / 'modded_batty' / 'batty.sna'
OUT_DIR = ROOT / 'build' / 'level_gt'

LEVEL_COUNTER = 0xB7EA
LEVEL_INIT_PC = 0xBA24
HALT_PC       = 0xBB61   # `JR $` trap after the first gameplay-loop iter
                         # has painted bat/ball/lives into scr_buff and
                         # flushed them to VRAM (right before
                         # restore_objs_and_magnet would wipe them).

N_LEVELS = 15
INITIAL_BOOT_SECONDS = 2.0    # time for game_start + first level-init -> halt
PER_LEVEL_SECONDS    = 0.5    # quick settle after re-running BA24
# When True, restart ZEsarUX from a fresh sna load for EACH level. Slower
# (~3-4× the runtime) but immune to inter-level state leakage we observed
# in iter 12: a bulk run gave L11 attr $46 (yellow, cycle 0) where a
# fresh-boot capture of L11 alone gave $05 (cyan, cycle 2). The state-
# reuse path (poke level + set PC = $BA24) doesn't fully reset texture
# selection. See notes/per-level-profile.md.
FRESH_BOOT_PER_LEVEL = True


def wait_for_halt(zc, timeout=5.0):
    """Poll PC until it equals HALT_PC (CPU is sitting at our DI/HALT trap)."""
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        regs = zc.get_registers()
        pc = regs.get('PC')
        if isinstance(pc, int) and pc == HALT_PC:
            return True
        last = pc
        time.sleep(0.05)
    raise TimeoutError(f"PC never reached 0x{HALT_PC:04X} (last 0x{last:04X})" if isinstance(last, int) else "PC unknown")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--levels', type=str, default=None,
                    help='comma-separated 1-based level list (default: 1..15)')
    ap.add_argument('--visible', action='store_true')
    args = ap.parse_args()

    if args.levels:
        targets = sorted({int(s) for s in args.levels.split(',') if s.strip()})
        targets = [n for n in targets if 1 <= n <= N_LEVELS]
    else:
        targets = list(range(1, N_LEVELS + 1))

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if not SNA.exists():
        raise SystemExit(f"missing {SNA} — run scripts/build_modded_batty.py first")

    def capture_one(n_one_based, proc, zc, needs_init):
        """Save one level's GT. If needs_init=True the CPU is already at
        L1 spin; only pokes + restart for levels >= 2 are needed."""
        level_n = n_one_based - 1
        if level_n > 0 and needs_init:
            zc.write_memory(LEVEL_COUNTER, bytes([level_n]))
            zc.set_register('PC', LEVEL_INIT_PC)
            zc.exit_cpu_step()
            try:
                wait_for_halt(zc, timeout=5.0)
            except TimeoutError as e:
                print(f"  level {n_one_based:2d}: WARN {e}")
                zc.enter_cpu_step()
        scr = OUT_DIR / f"level_{n_one_based:02d}.scr"
        zc.save_screen(str(scr.resolve()))
        ok = scr.exists() and scr.stat().st_size == 6912
        print(f"  level {n_one_based:2d}: {scr.name}  {'OK' if ok else 'FAIL'}")

    if FRESH_BOOT_PER_LEVEL:
        print(f"launching one ZEsarUX per level (FRESH_BOOT_PER_LEVEL=True)...")
        for n_one_based in targets:
            proc, zc = launch_emulator(str(ZESARUX), machine='48k',
                                       extra_args=[str(SNA)], port=10000,
                                       headless=not args.visible)
            try:
                time.sleep(INITIAL_BOOT_SECONDS)
                zc.enter_cpu_step()
                # For L1 (level_n == 0), the post-boot state is already at
                # the L1 spin trap — no re-init needed.
                capture_one(n_one_based, proc, zc, needs_init=True)
            finally:
                try: zc.exit_emulator()
                except Exception: pass
                try: proc.wait(timeout=3)
                except Exception:
                    proc.kill()
    else:
        print(f"launching ZEsarUX with modded sna (state-reuse mode)...")
        proc, zc = launch_emulator(str(ZESARUX), machine='48k',
                                   extra_args=[str(SNA)], port=10000,
                                   headless=not args.visible)
        try:
            print(f"  connected: {zc.get_version()}")
            time.sleep(INITIAL_BOOT_SECONDS)
            zc.enter_cpu_step()
            regs = zc.get_registers()
            pc = regs.get('PC')
            print(f"  initial PC after boot: 0x{pc:04X} (expect 0x{HALT_PC:04X})")
            for n_one_based in targets:
                capture_one(n_one_based, proc, zc, needs_init=True)
        finally:
            try: zc.exit_emulator()
            except Exception: pass
            try: proc.wait(timeout=3)
            except Exception:
                proc.kill()

    print("\nrendering PNGs + grid...")
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from extract_scr import decode
    from PIL import Image
    def zx_pal():
        out = []
        for level in (0xE0, 0xFF):
            for ink in range(8):
                r = level if (ink & 2) else 0
                g = level if (ink & 4) else 0
                b = level if (ink & 1) else 0
                out.append((r, g, b))
        return out
    pal = zx_pal()
    GUTTER = 4
    grid = Image.new('RGB', (3 * (256 + GUTTER) + GUTTER, 5 * (192 + GUTTER) + GUTTER), (24, 24, 32))
    for n in range(1, N_LEVELS + 1):
        scr = OUT_DIR / f"level_{n:02d}.scr"
        if not scr.exists():
            slot = Image.new('RGB', (256, 192), (50, 50, 50))
        else:
            idx = decode(scr.read_bytes())
            slot = Image.new('RGB', (256, 192))
            for y in range(192):
                for x in range(256):
                    slot.putpixel((x, y), pal[idx[y*256 + x]])
            slot.save(scr.with_suffix('.png'))
        i = n - 1
        gx = (i % 3) * (256 + GUTTER) + GUTTER
        gy = (i // 3) * (192 + GUTTER) + GUTTER
        grid.paste(slot, (gx, gy))
    grid.save(OUT_DIR / 'all_levels_gt.png')
    print(f"wrote {OUT_DIR}/all_levels_gt.png")


if __name__ == '__main__':
    main()
