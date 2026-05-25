#!/usr/bin/env python3
"""Run the auto-exiting DOS profile floppy under headless QEMU."""

from __future__ import annotations

import argparse
import subprocess
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--floppy", type=Path, default=Path("build/batty.img"))
    parser.add_argument("--seconds", type=float, default=25.0)
    parser.add_argument("--log", type=Path, default=Path("build/profile-qemu.log"))
    args = parser.parse_args()

    if not args.floppy.exists():
        raise SystemExit(f"missing floppy image: {args.floppy}")

    args.log.parent.mkdir(parents=True, exist_ok=True)
    with args.log.open("wb") as log:
        proc = subprocess.Popen([
            "qemu-system-i386",
            "-drive", f"if=floppy,format=raw,file={args.floppy}",
            "-boot", "a",
            "-m", "4",
            "-display", "none",
            "-monitor", "stdio",
            "-no-reboot",
        ], stdin=subprocess.PIPE, stdout=log, stderr=log)
        try:
            time.sleep(args.seconds)
            if proc.stdin is not None:
                proc.stdin.write(b"quit\n")
                proc.stdin.flush()
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            raise SystemExit("QEMU did not exit after monitor quit")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
