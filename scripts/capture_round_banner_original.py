#!/usr/bin/env python3
"""Capture the ORIGINAL Batty 'PLAYER 1 / ROUND 01' banner from ZEsarUX.

Boots the real tape, starts a 1-player game by tapping "0" (the menu's
start key: in_a_fe CPLs the keyboard read, so $EFFE bit0 set == "0"
pressed -> RET NZ starts the round), then free-runs while polling the
ULA screen ($4000, 6912 B) over ZRCP. The banner is drawn directly to
$4000 and held for ~1.2 s by a busy-wait (pause_long -> pause_short at
$97D3), during which the screen is static -- so a full read taken once
the banner signature appears is tear-free. No video backend is needed.

Breakpoints are deliberately NOT used: arming one parks ZEsarUX in a
cpu-step state where injected key events never register.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import launch_emulator
from extract_scr import decode as decode_scr
from test_visual import PALETTE_RGB

ROOT = Path(__file__).resolve().parent.parent
ZESARUX = ROOT / "tools/zesarux/src/zesarux"
TAPE = ROOT / "original/batty.tap"
OUT = ROOT / "build/round_banner_shots/original_round01.png"
SCR_OUT = ROOT / "build/round_banner_shots/original_round01.scr"

KEY_0 = ord("0")            # ASCII; ZEsarUX uses ASCII for alnum keys
LOAD_WAIT = 20.0            # real-time tape load
SCALE = 3
# The round banner is held by pause_long -> pause_short, a busy-wait
# loop at $97D3..$97D6. While PC sits there the screen is fully drawn
# (level + banner) and static, so the read is tear-free.
PAUSE_SHORT = range(0x97D3, 0x97D7)


def save_png(idx: bytes, path: Path, scale: int = SCALE) -> None:
    from PIL import Image
    img = Image.new("RGB", (256, 192))
    img.putdata([PALETTE_RGB[b & 0x0F] for b in idx])
    img = img.resize((256 * scale, 192 * scale), Image.NEAREST)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def main() -> int:
    print(f"launching ZEsarUX headless with {TAPE.name} ...")
    proc, zc = launch_emulator(str(ZESARUX), machine="48k",
                               extra_args=["--quickexit", str(TAPE)],
                               headless=True)
    try:
        print(f"waiting {LOAD_WAIT:.0f}s for tape load (real time)...")
        time.sleep(LOAD_WAIT)
        print(f"  menu PC=${zc.get_registers().get('PC', 0):04X}; tapping '0'")

        zc.send_key_event(KEY_0, True)
        time.sleep(0.5)
        zc.send_key_event(KEY_0, False)

        deadline = time.time() + 8.0
        scr = None
        while time.time() < deadline:
            if zc.get_registers().get("PC", 0) in PAUSE_SHORT:
                scr = zc.read_memory(0x4000, 6912)
                break
            time.sleep(0.02)

        if scr is None:
            print("FAIL: never caught the banner pause ($97D3)", file=sys.stderr)
            return 1

        idx = decode_scr(scr)
        SCR_OUT.parent.mkdir(parents=True, exist_ok=True)
        SCR_OUT.write_bytes(scr)
        save_png(idx, OUT)
        print(f"wrote {OUT}")
        print(f"  round={zc.read_memory(0xB7EB, 1)[0]} "
              f"level={zc.read_memory(0xB7EA, 1)[0]}")
        return 0
    finally:
        try:
            zc.exit_emulator()
        except Exception:
            pass
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except Exception:
            proc.kill()


if __name__ == "__main__":
    raise SystemExit(main())
