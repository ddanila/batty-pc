#!/usr/bin/env python3
"""Launch the ORIGINAL Batty in ZEsarUX with an "infinite lives" patch
applied, so you can play through and snapshot each level for clean
playthrough-based GTs.

Cheat: at PC 0xBD32..0xBD3B the game runs

    BD32: ld a, (B7E8)   ; load lives counter
    BD35: dec a          ; <-- the decrement
    BD36: ld (B7E8), a   ; store back
    BD39: jr z, lbd45h   ; if result == 0, jump to game-over
    BD3B: ...            ; else continue / re-init level

We patch BD35 from `dec a` (0x3D) to `or a` (0xB7). After the patch:

    ld a, (B7E8)   ; A = 3
    or a           ; Z = 0 (A non-zero), A unchanged
    ld (B7E8), a   ; store 3 back (no change)
    jr z, ...      ; never taken

A naive NOP at 0xBD35 didn't work because the Z flag would be
inherited from the call at 0xBD2F (lb7dch), occasionally landing on
the game-over branch from "stale" Z. `or a` deterministically
clears Z (since lives is always >= 1 when we get here).

The script launches ZEsarUX in interactive (windowed) mode, waits
for the tape to autoload past the BASIC + CODE blocks, opens ZRCP,
pokes 0xBD35 = 0xB7, then disconnects. ZEsarUX keeps running.

Use `make snapshot` (separate terminal) to capture screen + RAM at
any point - the new GT references for each level go to
build/snapshots/<timestamp>/.
"""
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import ZrcpClient


ZESARUX = Path(__file__).resolve().parent.parent / "../generaly/tools/zesarux/src/zesarux"
TAPE    = Path(__file__).resolve().parent.parent / "original/batty.tap"
ZRCP_PORT = 10000

# Address of the `dec a` byte in the lives-decrement sequence.
# Patch 0x3D (dec a) -> 0xB7 (or a). See module docstring.
LIVES_DEC_PC = 0xBD35
OR_A_BYTE    = 0xB7

LOAD_WAIT_SECONDS = 18.0    # batty.tap is ~40 KB; real-time tape load


def main():
    cmd = [
        str(ZESARUX),
        '--noconfigfile',
        '--machine', '48k',
        '--enable-remoteprotocol',
        '--remoteprotocol-port', str(ZRCP_PORT),
        '--quickexit',
        str(TAPE),
    ]
    print(f'launching ZEsarUX (windowed): {" ".join(cmd)}')
    print(f'tape will load over ~{LOAD_WAIT_SECONDS:.0f}s; cheat patches after.')
    proc = subprocess.Popen(cmd)

    print(f'waiting {LOAD_WAIT_SECONDS:.0f}s for tape to load...')
    time.sleep(LOAD_WAIT_SECONDS)

    print(f'connecting ZRCP and patching 0x{LIVES_DEC_PC:04X} = 0x{OR_A_BYTE:02X} (or a) ...')
    try:
        with ZrcpClient(port=ZRCP_PORT) as zc:
            zc.connect()
            print(f'  ZEsarUX version: {zc.get_version()}')
            zc.write_memory(LIVES_DEC_PC, bytes([OR_A_BYTE]))
            verify = zc.read_memory(LIVES_DEC_PC, 1)
            if verify == bytes([OR_A_BYTE]):
                print(f'  patched: byte at 0x{LIVES_DEC_PC:04X} is now 0x{OR_A_BYTE:02X}.')
            else:
                print(f'  WARN: verify read got {verify.hex()} (expected {OR_A_BYTE:02X})')
    except (OSError, ConnectionRefusedError) as e:
        print(f'  ZRCP connect failed: {e}')
        print('  (ZEsarUX may not have finished starting; try LOAD_WAIT_SECONDS=25)')

    print()
    print('=' * 60)
    print('Infinite lives active. Play the game in the ZEsarUX window.')
    print('To snapshot RAM + screen at any moment, run in another terminal:')
    print('    make snapshot')
    print('Snapshots land in build/snapshots/<UTC timestamp>/.')
    print('=' * 60)

    # Wait for the user to close ZEsarUX.
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()


if __name__ == '__main__':
    main()
