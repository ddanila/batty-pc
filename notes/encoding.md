# Text + markup encoding

Batty's menu / hi-score screens are drawn through a small markup language.
The buffer at `0x8FD1` (snap1, ~22 rows) is one such stream. `sub_b796h`
walks it with `B = 22` (= one row per text line on screen, matches Spectrum's
22 in-border rows).

## Character codes

Each visible glyph is one byte.

| Byte range    | Meaning           |
|---------------|-------------------|
| `0x00..0x09`  | digits 0..9        |
| `0x0A..0x23`  | letters A..Z (`A=0x0A`, `Z=0x23`) |
| `0x24`        | end-of-field marker |
| `0x26`        | space               |
| `0x25, 0x27`  | unknown (not seen) |
| `0x38`        | header-row marker   |
| `0x40..0x47`  | attribute / ink colour codes (same value space as ZX attr byte) |
| `0x58`        | new-row marker      |

(The colour codes happen to overlap ZX attribute bytes: `0x47` = bright
white ink, `0x46` = bright yellow, `0x45` = bright cyan, `0x44` = bright
green, `0x43` = bright magenta. The renderer probably writes them straight
into the attribute table at `0x5800+`.)

## Verified examples (snap1 = hi-score state)

```
0x8FD1: 38 26 47 02 01 24   header row, col=2, white attr, label "1."
0x8FD7: 38 36 47 02 02 24   header row, col=2, white attr, label "2."
0x8FDD: 38 46 46 02 03 24   header row, col=2, yellow attr, label "3."
... colour fades by 1 every two rows ...
```

```
0x900E: 58 26 47 0E 01 00 00 00 00 00 00 26 26 26 11 26 12 26 1D
        ^^                                 ^^ ^^                   ^^ ^^ ^^
        |                                  └─ score digit (1)      H  I  T
        new row    space attr=white                   3 spaces       (initials, space-separated)
```
The seven digit bytes after `0x0E` encode the score as one decimal digit
per byte (leading zeros suppressed by the renderer). 7 digits supports
scores up to 9,999,999.

## Record layout in the buffer

Each row in `0x8FD1..` appears to be one of:

| First byte | Layout                                                  |
|------------|---------------------------------------------------------|
| `0x38`     | static label row: `38 <Y_coord> <attr> <col?> <text…> 0x24` |
| `0x58`     | data row: `58 <Y_coord> <attr> <text…>`                |
| `0x30`     | seems to be a "totals" row (only seen once at `0x9007`) |

Embedded **pointers + linked headers** appear at `0x8FC0..0x8FD0`:

```
17 0D 26  <00 00 60 8F>  47 08  10 0A 16 0E 26 26 18 17
^^ ^^ ^^   ^^^^^^^^^^^^   ^^ ^^  G  A  M  E  sp sp O  N
N  D  sp   addr 0x8F60    attr=white col=8        ("GAME ON" text)
```

So records can carry **inline addresses** linking to other rows.

## How sub_b796h is called

```
sub_926bh:
    call sub_9231h          ; setup
    ld hl, 0xBF00           ; HL = source (live game state)
    ld de, 0x922E           ; DE = scratch / transformed
    call sub_b61ch          ; HL→DE copy/transform
    ld de, 0x8FD1           ; DE = render buffer
    ld b, 0x16              ; B = 22 rows
    jp sub_b796h            ; tail call: render 22 rows
```

`0xBF00` holds the "live" data (the player score, current entries, etc.).
`sub_b61ch` composes it into the rendered markup at `0x8FD1`. `sub_b796h`
walks the rendered buffer and paints the screen.

## Re-implementation sketch (for our DOS port)

Plain Python decoder:

```python
ALPHA = "0123456789??????????ABCDEFGHIJKLMNOPQRSTUVWXYZ"  # 0..0x23
def decode(buf, pos):
    out = []
    while pos < len(buf):
        b = buf[pos]; pos += 1
        if b == 0x58:   out.append('\\n')
        elif b == 0x38: out.append('[HDR]')
        elif b == 0x24: out.append('[EOF]')
        elif b == 0x26: out.append(' ')
        elif 0x40 <= b <= 0x4F: out.append(f'[c{b:02X}]')
        elif b < 0x24:  out.append(ALPHA[b])
        else:           out.append(f'[?{b:02X}]')
    return ''.join(out)
```

For the C port: this becomes a tiny switch-statement renderer that emits
8×8 glyphs (from our extracted font) into the playfield region of mode 13h
with the right palette index.
