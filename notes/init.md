# Init chain — from tape load to the main game loop

## Tape layout

`original/batty.tap` (30,142 bytes) holds **4 blocks**:

| # | type | size | meaning |
|---|------|------|---------|
| 0 | BASIC header (lying) | 17 B | name=`Batty`, autorun line 0 |
| 1 | "BASIC" data (also lying) | 41 B | a self-relocating Z80 loader |
| 2 | DATA (headless) | 6912 B | loading screen, loaded straight to `0x4000` |
| 3 | DATA (headless) | 23156 B | the main game blob, loaded to `0x6800` |

Block 1 is not BASIC at all — its line-length field `0xC567` is impossible,
larger than the file. It is Z80 code disguised by a BASIC header so the ROM
`LOAD ""` picks it up.

## Block 1 — the tape loader

Self-relocates its 41 bytes to `0x67D7`, then jumps to the copy:

```
copy itself to 0x67D7
LD IX, 0x4000; LD DE, 0x1B00; CALL 0x0556    ; ROM LD-BYTES — the screen
LD IX, 0x6800; LD DE, 0x5A74; CALL 0x0556    ; the main blob
; control FALLS THROUGH into 0x6800, because the just-loaded blob
; overwrites the loader's own last bytes.
```

So the game's effective entry point is **`0x6800`**.

## Stage 1 — the shift table at `0xF200..0xFFFF`

The first ~37 bytes build a 3584-byte pre-shift table:

```
DI
LD SP, 0x6000               ; stack just below the load address
XOR A; OUT (0xFE), A        ; border = black
LD HL, 0xF200
LD B, 0x01                  ; outer: shift count B = 1..7
outer_loop:
    LD C, 0x00              ; inner: byte value C = 0..255
    inner_loop:
        LD D, C / LD E, B / XOR A
        shift_loop:
            SRL D / RRA / DEC E / JR NZ, shift_loop
        INC H / LD (HL), A  ; bits shifted out, at +0x100
        DEC H / LD (HL), D  ; the shifted byte, at +0x000
        INC HL / INC C / JR NZ, inner_loop
    INC H                   ; skip the +0x100 buffer
    INC B / BIT 3, B        ; stop when B = 8
    JR Z, outer_loop
```

Each shift count occupies 512 bytes — the shifted bytes in the low half and
the carried-out bits in the high half — so B=1 is `0xF200..0xF3FF` and B=7 is
`0xFE00..0xFFFF`.

This is the runtime sprite-shift lookup: any blitter can fetch the
"shifted-right-by-N" version of any byte without shifting inline, which is
what lets the Z80 render sprites at arbitrary X positions cheaply. It is
`table_shifts`; the port shifts per row in C instead
(`notes/blitter-port.md`).

## Stage 2 — pre-shifting selected data (`sub_6853h` at `0x6825`)

Walks a 5-byte-record table at `0x68D7` (4 entries plus a terminator):

| # | src | dst | mask | shifts |
|---|-----|-----|------|--------|
| 0 | 0x7B16 | 0x7B48 | 0xFF | 1..7 |
| 1 | 0x7E38 | 0x7D4E | 0x10 | 4 |
| 2 | 0x7F42 | 0x828A | 0x10 | 4 |
| 3 | 0x8188 | 0x81F2 | 0x10 | 4 |

`sub_688Bh` does the shifting inline with `srl`/`rr` rather than reading the
`0xF200` table — that table is the runtime path, this is one-shot startup.
Source layout is `[byte0 = chunks/row, byte1 = rows, body = rows x chunks x
(hi, lo)]`; the destination is wider, `2*byte0+2` bytes per row after
shifting.

These destinations are NOT the main game sprites — a VRAM reverse-search
against snapshots found near-zero matches. Probable role: HUD glyphs and small
fixed-position icons.

## Stage 3 — the XOR-pair unpacker at `0x6828`

Walks a 2-byte-entry table at `0x7796` (80+ entries, `0x0000`-terminated).
For each entry it reads `(C, E)` as a 2-byte header from the data, then runs
over `C x E` bytes:

```
ld a, (hl) / inc hl / xor (hl) / ld (hl), a / inc hl
```

XOR-pair is its own inverse, so running the transform on a region a second
time restores it. Many ranges appear in the table more than once, and which
subranges end up "decrypted" at runtime is encoded by the COUNT of entries.

The unpacker also touches four code-range addresses (`0x68ED`, `0x691F`,
`0x6951`, `0x6973`) — operand bytes of SMC-patched instructions in the
loader's tail, whose initial values it is writing.

## Stage 4 — `JP 0xB9B1` at `0x6850`

After the unpack loop, one byte is written (`LD (0x891D), 0x0C`) and control
jumps to `0xB9B1`, the main game / menu entry. The init chain ends there.

`gfx_inverse` — the boot pass that XORs every sprite's pix bytes with its mask
— runs from the game side; see `notes/bird-render-parity.md`.

## Memory map after init (verified against snapshots)

| range | class | content |
|-------|-------|---------|
| 0x4000..0x57FF | dynamic | screen pixel buffer (VRAM) |
| 0x5800..0x5AFF | dynamic | screen attribute buffer |
| 0x5B00..0x5BFF | static | printer buffer, repurposed |
| 0x5C00..0x5CB5 | dynamic | ZX system vars |
| 0x5CB6..0x5FFF | static | BASIC free area / game stack |
| 0x6000..0x67FF | static | below the load address |
| 0x6800..0xC273 | tape / SMC | the loaded blob; ~2.7k of SMC patches |
| 0xC274..0xF1FF | mixed | post-blob workspace; the field cache |
| 0xF200..0xFFFF | static | the shift table, built once |

Across `0x4000..0xFFFF`: 41.5% tape-match, 32.7% static non-tape, 25.8%
dynamic, from three snapshots.
