#!/usr/bin/env python3
"""Detokenize a ZX Spectrum BASIC program payload.

Reference: https://sinclair.wiki.zxnet.co.uk/wiki/Spectrum_BASIC_program

Layout: u16BE line_no, u16LE line_len, tokens..., 0x0D, repeat.
Token bytes 0xA5..0xFF map to keywords. A digit literal is followed by
0x0E + 5 binary bytes (the parsed numeric form) which we skip.
"""
import struct
import sys
from pathlib import Path

TOKENS = [
    "RND", "INKEY$", "PI", "FN", "POINT", "SCREEN$", "ATTR", "AT",         # A5..AC
    "TAB", "VAL$", "CODE", "VAL", "LEN", "SIN", "COS", "TAN",              # AD..B4
    "ASN", "ACS", "ATN", "LN", "EXP", "INT", "SQR", "SGN",                 # B5..BC
    "ABS", "PEEK", "IN", "USR", "STR$", "CHR$", "NOT", "BIN",              # BD..C4
    "OR", "AND", "<=", ">=", "<>", "LINE", "THEN", "TO",                   # C5..CC
    "STEP", "DEF FN", "CAT", "FORMAT", "MOVE", "ERASE", "OPEN #", "CLOSE #",  # CD..D4
    "MERGE", "VERIFY", "BEEP", "CIRCLE", "INK", "PAPER", "FLASH", "BRIGHT",   # D5..DC
    "INVERSE", "OVER", "OUT", "LPRINT", "LLIST", "STOP", "READ", "DATA",      # DD..E4
    "RESTORE", "NEW", "BORDER", "CONTINUE", "DIM", "REM", "FOR", "GO TO",     # E5..EC
    "GO SUB", "INPUT", "LOAD", "LIST", "LET", "PAUSE", "NEXT", "POKE",        # ED..F4
    "PRINT", "PLOT", "RUN", "SAVE", "RANDOMIZE", "IF", "CLS", "DRAW",         # F5..FC
    "CLEAR", "RETURN", "COPY",                                                # FD..FF
]


def detokenize(data: bytes) -> str:
    out, p = [], 0
    while p + 4 <= len(data):
        line_no = (data[p] << 8) | data[p + 1]
        line_len = data[p + 2] | (data[p + 3] << 8)
        p += 4
        body = data[p:p + line_len]
        p += line_len
        out.append(f"{line_no} {render_line(body)}")
    return "\n".join(out)


def render_line(body: bytes) -> str:
    s, i = [], 0
    while i < len(body):
        b = body[i]
        if b == 0x0D:
            i += 1
            continue
        if b == 0x0E:
            # Binary number form — skip 5 bytes, ASCII form is already present.
            i += 6
            continue
        if 0xA5 <= b <= 0xFF:
            s.append(" " + TOKENS[b - 0xA5] + " ")
            i += 1
            continue
        s.append(chr(b))
        i += 1
    return "".join(s).strip()


def main():
    if len(sys.argv) != 2:
        print("usage: detokenize_basic.py <in.bin>", file=sys.stderr)
        sys.exit(2)
    print(detokenize(Path(sys.argv[1]).read_bytes()))


if __name__ == "__main__":
    main()
