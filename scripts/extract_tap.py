#!/usr/bin/env python3
"""Parse a ZX Spectrum .tap into its constituent blocks.

TAP wire format (public, see https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format):

    file := block*
    block := u16 LE length, length bytes of block_data
    block_data := flag u8, payload (length-2 bytes), xor_checksum u8
        flag 0x00 = header block (17 bytes payload):
            type u8, name char[10], data_len u16, param1 u16, param2 u16
            type 0=Program, 1=NumArray, 2=CharArray, 3=CODE
        flag 0xFF = data block

Prints a directory and writes each block's *payload* (without flag/checksum)
to a subdir as NN_<kind>_<name>.bin.
"""
import os
import struct
import sys
from pathlib import Path

TYPES = {0: "BASIC", 1: "NUMARR", 2: "CHRARR", 3: "CODE"}


def xor_checksum(data: bytes) -> int:
    x = 0
    for b in data:
        x ^= b
    return x & 0xFF


def safe_name(raw: bytes) -> str:
    name = raw.decode("latin-1").strip()
    return "".join(c if c.isalnum() or c in "._-" else "_" for c in name) or "noname"


def parse(tap_bytes: bytes):
    pos, idx = 0, 0
    blocks = []
    pending_header = None
    while pos < len(tap_bytes):
        if pos + 2 > len(tap_bytes):
            raise ValueError(f"truncated length field at {pos}")
        (blen,) = struct.unpack_from("<H", tap_bytes, pos)
        pos += 2
        if pos + blen > len(tap_bytes):
            raise ValueError(f"truncated block at {pos}, want {blen} bytes")
        block = tap_bytes[pos:pos + blen]
        pos += blen
        flag, payload, csum = block[0], block[1:-1], block[-1]
        cs_ok = xor_checksum(block[:-1]) == csum

        info = {"idx": idx, "flag": flag, "len": blen, "csum_ok": cs_ok,
                "payload": payload, "name": None, "type": None,
                "data_len": None, "load_addr": None, "param2": None}

        if flag == 0x00 and len(payload) == 17:
            t = payload[0]
            name = safe_name(payload[1:11])
            data_len, p1, p2 = struct.unpack_from("<HHH", payload, 11)
            info["type"]      = TYPES.get(t, f"T{t}")
            info["name"]      = name
            info["data_len"]  = data_len
            info["load_addr"] = p1 if t == 3 else None
            info["param2"]    = p2
            pending_header = info
        else:
            if pending_header is not None:
                info["name"]      = pending_header["name"]
                info["type"]      = pending_header["type"]
                info["load_addr"] = pending_header["load_addr"]
                pending_header    = None
            else:
                info["name"], info["type"] = "headless", "DATA"
        blocks.append(info)
        idx += 1
    return blocks


def main():
    if len(sys.argv) != 3:
        print("usage: extract_tap.py <in.tap> <out_dir>", file=sys.stderr)
        sys.exit(2)
    tap = Path(sys.argv[1]).read_bytes()
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    blocks = parse(tap)

    print(f"{len(blocks)} blocks in {sys.argv[1]} ({len(tap)} bytes)\n")
    print(f"{'idx':>3}  {'flag':>4}  {'len':>6}  {'csum':>4}  "
          f"{'type':>6}  {'load':>6}  name")
    for b in blocks:
        flag = f"0x{b['flag']:02X}"
        load = f"0x{b['load_addr']:04X}" if b["load_addr"] is not None else "-"
        cs   = "ok" if b["csum_ok"] else "BAD"
        kind = "HDR" if b["flag"] == 0x00 else "DAT"
        print(f"{b['idx']:>3}  {flag:>4}  {b['len']:>6}  {cs:>4}  "
              f"{b['type']:>6}  {load:>6}  {b['name']} [{kind}]")

        suffix = "hdr" if b["flag"] == 0x00 else "dat"
        outp = out_dir / f"{b['idx']:02d}_{b['type']}_{b['name']}.{suffix}.bin"
        outp.write_bytes(b["payload"])


if __name__ == "__main__":
    main()
