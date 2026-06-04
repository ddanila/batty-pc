#!/usr/bin/env python3
"""Trace writes to the enemy target byte (object_enemy+$14 = $9BAA) on the
original, to find what actually sets the enemy steering target and from
where — resolving the $8E17-vs-0x2C contradiction (see notes/rng-model.md,
notes/enemy-movement.md)."""
import json, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from zrcp import launch_emulator
from replay_harness import apply_original_setup

SNAP = "build/snapshots/20260513T202101Z.sna"
REPLAY = "replays/l3-brick-flash.json"
ENEMY_TGT = 0x9BAA   # object_enemy ($9B96) + $14

def main():
    proc, zc = launch_emulator("tools/zesarux/src/zesarux", machine="48k",
                               extra_args=[], port=10000, headless=True)
    try:
        zc.snapshot_load(str(Path(SNAP).resolve()))
        spec = json.loads(Path(REPLAY).read_text())
        orig = spec.get("original", {})
        time.sleep(float(orig.get("boot_wait", 0.5)))
        apply_original_setup(zc, orig.get("setup", []))
        zc.enable_breakpoints()
        zc.set_breakpoint(1, f"MWA={ENEMY_TGT:04X}H")
        print(f"tracing writes to enemy target ${ENEMY_TGT:04X}:")
        for i in range(10):
            zc.run(5000000)                      # until the MWA breakpoint
            regs = zc.get_registers()
            pc = regs.get("PC", -1)
            a = regs.get("A", -1)
            tgt = zc.read_memory(ENEMY_TGT, 1)[0]
            rnd = zc.read_memory(0x8E17, 2)       # random_number low,high
            print(f"  hit {i}: PC=${pc:04X} A=${a:02X} -> [$9BAA]=${tgt:02X} "
                  f"random_number=${rnd[0]:02X}{rnd[1]:02X}")
            zc.run(50)                            # step past the write
    finally:
        zc.exit_emulator()
        try: proc.wait(timeout=5)
        except Exception: proc.kill()

if __name__ == "__main__":
    main()
