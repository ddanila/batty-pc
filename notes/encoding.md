# Text + markup encoding

Batty's menu / hi-score screens are drawn through a small markup language.
The buffer at `0x8FD1` (snap1, ~22 rows) is one such stream. `sub_b796h`
walks it with `B = 22` (= one row per text line on screen, matches Spectrum's
22 in-border rows).

## Character codes (verified — 43-glyph font at `0x6A15`, 6 bytes per glyph)

Each visible glyph is one byte; the byte indexes directly into the font
table at `0x6A15` (`font_base + code × 6`).

| Byte range    | Meaning                          |
|---------------|----------------------------------|
| `0x00..0x09`  | digits 0..9                       |
| `0x0A..0x23`  | letters A..Z (`A=0x0A`, `Z=0x23`) |
| `0x24`        | period `.`                        |
| `0x25`        | comma `,`                         |
| `0x26`        | space                             |
| `0x27`        | dash `-`                          |
| `0x28`        | underscore `_`                    |
| `0x29`        | doubled-vertical (Roman II)       |
| `0x2A`        | equals `=` (used for `=====` divider) |
| `0x2B`+       | **sprite data begins here** (0x6B13 in tape blob) |
| `0x40..0x47`  | inline attribute / ink colour (same encoding as ZX attr byte) |

Colour codes happen to overlap ZX attribute byte values: `0x47` = bright
white ink, `0x46` = bright yellow, `0x45` = bright cyan, `0x44` = bright
green, `0x43` = bright magenta. The renderer pipes them straight into the
attribute table at `0x5800+`.

## Row markers (control bytes)

These bytes mark the start of a new structural record in the buffer.

| Byte   | Meaning                                       | Starting X col |
|--------|-----------------------------------------------|----------------|
| `0x30` | header row for 2-digit ranks ("10.")          | 6              |
| `0x38` | header row for 1-digit ranks ("1." … "9.")    | 7              |
| `0x50` | centred title-style text (title, divider)     | 10             |
| `0x58` | data row (score + initials)                   | 11             |
| `0x40` | full-line label (e.g. "ENTER YOUR NAME.")     | (other screen) |

## Record shape

```
HEADER (0x30 / 0x38 / 0x50):
    marker  Y  attr  count  payload[count]
DATA (0x58):
    marker  Y  attr  count  payload[count]
```

`Y` is screen pixel-row; the actual glyph top sits at `Y − 5` (the 2-row
top padding the original renderer inserts comes from the +2 inside an
8-row char cell, the −5 is the empirical glyph-top offset on our VGA).

`count` is the number of payload bytes to render — for headers it
*includes* the trailing `0x24` (period), since the period is just another
glyph at that index. For data rows, count is exactly 14 = 6 score digits
+ 3 spaces + 5 chars of `H _ I _ T` style initials.

Scores are stored MSB-first as 7 byte-digits with literal leading zeros
preserved (max 9,999,999). The DOS port's HUD renders 6 digits instead
— see `score_to_digits` in `src/main.cpp`.

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
per byte (leading zeros painted as glyphs, not suppressed).

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

