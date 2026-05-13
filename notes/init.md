# Init chain — from tape load to main game loop

## Tape layout

`original/batty.tap` (30 142 bytes) holds **4 blocks**:

| # | type  | size  | meaning                                     |
|---|-------|-------|---------------------------------------------|
| 0 | BASIC header (lying)  | 17 B | name=`Batty`, autorun line 0          |
| 1 | "BASIC" data (also lying) | 41 B | 41-byte Z80 self-relocating loader   |
| 2 | DATA (headless)       | 6 912 B | loading screen, loaded straight to `0x4000` |
| 3 | DATA (headless)       | 23 156 B | main game blob, loaded to `0x6800`   |

Block 1 isn't BASIC at all — line-length field `0xC567` is impossible
(>file size). It's pure Z80 code disguised by the BASIC header so the
ROM `LOAD ""` picks it up.

## Block 1 (the tape loader)

Self-relocates 41 bytes to `0x67D7`, then jumps to the copied code:

```
copy itself to 0x67D7
LD IX, 0x4000; LD DE, 0x1B00; CALL 0x0556    ; ROM LD-BYTES — load screen
LD IX, 0x6800; LD DE, 0x5A74; CALL 0x0556    ; load main blob
; control falls through into 0x6800 because the just-loaded blob
; overwrites the loader's last bytes — elegant trick.
```

Effective entry point of the loaded game: **`0x6800`**.

## Block 3 at `0x6800`: first init stage

The first ~37 bytes build a **shift-pre-computation table** at
`0xF200..0xFFFF` (3 584 bytes):

```
DI                          ; no interrupts during init
LD SP, 0x6000               ; stack just below load addr
XOR A; OUT (0xFE), A        ; border = black
LD HL, 0xF200               ; table dest
LD B, 0x01                  ; shift count (outer: B = 1..7)
outer_loop:
    LD C, 0x00              ; byte value (inner: C = 0..255)
    inner_loop:
        ; D = (C >> B), A = bits shifted out of C through carry
        LD D, C
        LD E, B
        XOR A
        shift_loop:
            SRL D
            RRA
            DEC E
            JR NZ, shift_loop
        INC H               ; HL += 0x100
        LD (HL), A          ; store carry bits at +0x100
        DEC H               ; HL -= 0x100
        LD (HL), D          ; store shifted byte at +0x000
        INC HL
        INC C
        JR NZ, inner_loop
    INC H                   ; H += 1 (skip the +0x100 buffer)
    INC B
    BIT 3, B                ; stop when B = 8
    JR Z, outer_loop
```

Layout per shift count B (each B occupies 512 bytes):

| B | Range          | Content                          |
|---|----------------|----------------------------------|
| 1 | 0xF200..0xF2FF | byte shifted right by 1 (high half) |
| 1 | 0xF300..0xF3FF | bits that fell off (low half)       |
| 2 | 0xF400..0xF5FF | shift-by-2 (high + low halves)      |
| … | …              | …                                  |
| 7 | 0xFE00..0xFFFF | shift-by-7                          |

This is the **runtime sprite-shift lookup table**: any sprite blitter
can fetch the "shifted-right-by-N" version of any byte without doing the
shifts inline.

## Second stage: `CALL sub_6853h` at `0x6825`

Walks a small 5-byte-record table at `0x68D7` (4 entries + terminator),
pre-shifting selected sprite/font data:

| # | Src    | Dst    | Bitmask | Shift counts generated |
|---|--------|--------|---------|------------------------|
| 0 | 0x7B16 | 0x7B48 | 0xFF    | 1..7 (all)             |
| 1 | 0x7E38 | 0x7D4E | 0x10    | 4                      |
| 2 | 0x7F42 | 0x828A | 0x10    | 4                      |
| 3 | 0x8188 | 0x81F2 | 0x10    | 4                      |

`sub_688Bh` does the actual shift using the `srl/rr` instructions inline
(it doesn't read from the 0xF200 table — that's the runtime path; this
one is one-shot startup). Source sprite layout: `[byte0 = chunks/row,
byte1 = rows, body = rows × chunks × (hi, lo)]`. Dst layout is wider
(each row produces 2*byte0+2 bytes after shifting).

The destination addresses don't contain "the main game sprites" — VRAM
reverse-search against snapshots showed near-zero matches from this set.
Probable role: HUD glyphs, small fixed-position icons.

## Third stage: XOR-pair unpacker at `0x6828`

Walks a **2-byte-entry table at `0x7796`** (80+ entries, terminated by
0x0000). For each entry the routine reads `(C, E)` as a 2-byte header
from the data, then runs over `C × E` bytes:

```
ld a, (hl)        ; A = byte[i]
inc hl
xor (hl)          ; A ^= byte[i+1]
ld (hl), a        ; byte[i+1] = byte[i] ^ byte[i+1]
inc hl
```

XOR-pair is its own inverse: running the same transform on a region a
second time restores the original bytes. Many ranges appear in the table
multiple times — the choice of which subranges land "decrypted" at
runtime is encoded by the count of entries.

This unpacker also touches a few code-range addresses (`0x68ED`,
`0x691F`, `0x6951`, `0x6973`) — these are operand bytes of SMC-patched
instructions in the loader's tail; the unpacker writes their initial
values.

## Fourth stage: `JP 0xB9B1` at `0x6850`

After the unpack loop terminates, a single byte is written (`LD (0x891D), 0x0C`)
and control jumps to `0xB9B1`. That's the main game / menu entry; the
init chain proper ends here.

## Memory map after init (verified against snapshots)

| Range            | Class       | Content                          |
|------------------|-------------|----------------------------------|
| 0x4000..0x57FF   | DYNAMIC     | screen pixel buffer (VRAM)       |
| 0x5800..0x5AFF   | DYNAMIC     | screen attribute buffer          |
| 0x5B00..0x5BFF   | STATIC      | printer buf (repurposed)         |
| 0x5C00..0x5CB5   | DYNAMIC     | ZX system vars                   |
| 0x5CB6..0x5FFF   | STATIC      | BASIC free area / game stack     |
| 0x6000..0x67FF   | STATIC      | below load addr                  |
| 0x6800..0xC273   | TAPE / SMC  | loaded blob; ~2.7k of SMC patches |
| 0xC274..0xF1FF   | STATIC/DYN  | post-blob workspace; field cache |
| 0xF200..0xFFFF   | STATIC      | shift table (built once)          |

(Numbers: 41.5 % tape-match, 32.7 % static-non-tape, 25.8 % dynamic
across 0x4000..0xFFFF, from 3 snapshots.)
