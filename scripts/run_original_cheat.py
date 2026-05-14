#!/usr/bin/env python3
"""Launch the ORIGINAL Batty in ZEsarUX with an "infinite lives" patch
applied, so you can play through and snapshot each level for clean
playthrough-based GTs.

Cheat: NOP out the `dec a` at PC 0xBD35 (the instruction in the
ld a,(B7E8) / dec a / ld (B7E8),a sequence that decrements the
lives counter when the ball falls past the bat).

The script launches ZEsarUX in interactive (windowed) mode, waits
for the tape to autoload past the BASIC + CODE blocks, opens a
ZRCP connection, pokes 0xBD35 = 0x00 (NOP), then disconnects.
ZEsarUX keeps running for you to play.

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
# Code at 0xBD32..0xBD37:
#   BD32: 3A E8 B7  ld a, (B7E8)   ; load lives
#   BD35: 3D        dec a          ; <-- patch to 0x00 (NOP)
#   BD36: 32 E8 B7  ld (B7E8), a   ; store back (now stores unchanged value)
LIVES_DEC_PC = 0xBD35
NOP_BYTE     = 0x00

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

    print(f'connecting ZRCP and patching 0x{LIVES_DEC_PC:04X} = 0x{NOP_BYTE:02X} (NOP) ...')
    try:
        with ZrcpClient(port=ZRCP_PORT) as zc:
            zc.connect()
            print(f'  ZEsarUX version: {zc.get_version()}')
            zc.write_memory(LIVES_DEC_PC, bytes([NOP_BYTE]))
            # Sanity read
            verify = zc.read_memory(LIVES_DEC_PC, 1)
            if verify == bytes([NOP_BYTE]):
                print(f'  patched: byte at 0x{LIVES_DEC_PC:04X} is now 0x{NOP_BYTE:02X}.')
            else:
                print(f'  WARN: verify read got {verify.hex()} (expected {NOP_BYTE:02X})')
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
