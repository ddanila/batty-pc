#!/usr/bin/env python3
"""Capture a RAM + screen snapshot from a running ZEsarUX via ZRCP.

Connects to ZRCP on localhost:10000 (the port that `make run-original`
opens), reads RAM from 0x4000..0xFFFF (the entire 48K writable space),
also saves the current screen via ZRCP save-screen and a re-render of
the 0x4000..0x57FF SCR region via the existing extract_scr.py.

Outputs land in build/snapshots/<UTC timestamp>/:
  ram_4000_FFFF.bin    49152 bytes — full RAM dump
  screen.scr           6912  bytes — pixel + attr region
  screen.png           the SCR rendered through our decoder
  registers.txt        PC / SP / regs at snapshot time
"""
import datetime
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import ZrcpClient


RAM_START = 0x4000
RAM_END   = 0x10000   # exclusive
SCR_LEN   = 6912


def main():
    out_root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build/snapshots")
    ts = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
    out = out_root / ts
    out.mkdir(parents=True, exist_ok=True)

    with ZrcpClient() as zc:
        zc.connect()
        regs = zc.get_registers()
        ram = zc.read_memory(RAM_START, RAM_END - RAM_START)

    (out / "ram_4000_FFFF.bin").write_bytes(ram)
    (out / "screen.scr").write_bytes(ram[:SCR_LEN])
    (out / "registers.txt").write_text(str(regs) + "\n")

    # Re-render SCR through our decoder so it lands next to the dump.
    subprocess.run(
        ["python3", "scripts/extract_scr.py",
         str(out / "screen.scr"), str(out / "screen.raw")],
        check=True,
    )
    # Tiny inline PNG export from the 8bpp raw using PIL.
    try:
        from PIL import Image
        raw = (out / "screen.raw").read_bytes()
        img = Image.frombytes("L", (256, 192), raw)
        img.save(out / "screen.png")
    except ImportError:
        pass

    print(f"snapshot at {out}/  ({len(ram)} B RAM, regs in registers.txt)")


if __name__ == "__main__":
    main()
