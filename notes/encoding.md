# Text + markup encoding

The menu and hi-score screens are drawn through a small markup language. The
buffer at `0x8FD1` holds one such stream; `sub_b796h` walks it with `B = 22`,
one row per text line, matching the Spectrum's 22 in-border rows. See
`notes/menu.md` for how it is called.

## Character codes

Verified against the 43-glyph font at `0x6A15`, 6 bytes per glyph. Each
visible glyph is one byte indexing the font directly
(`font_base + code * 6`).

| byte | meaning |
|------|---------|
| `0x00..0x09` | digits 0..9 |
| `0x0A..0x23` | letters A..Z (`A=0x0A`, `Z=0x23`) |
| `0x24` | period `.` |
| `0x25` | comma `,` |
| `0x26` | space |
| `0x27` | dash `-` |
| `0x28` | underscore `_` |
| `0x29` | doubled-vertical (Roman II) |
| `0x2A` | equals `=`, used for the `=====` divider |
| `0x2B`+ | sprite data begins (`0x6B13` in the tape blob) |
| `0x40..0x47` | inline attribute / ink colour |

The colour codes overlap ZX attribute byte values on purpose: `0x47` is
bright white ink, `0x46` bright yellow, `0x45` bright cyan, `0x44` bright
green, `0x43` bright magenta. The renderer pipes them straight into the
attribute area.

## Row markers and record shape

The first byte of a record marks its kind and fixes the starting column:

| byte | meaning | start col |
|------|---------|-----------|
| `0x30` | header row for 2-digit ranks ("10.") | 6 |
| `0x38` | header row for 1-digit ranks ("1." .. "9.") | 7 |
| `0x50` | centred title-style text (title, divider) | 10 |
| `0x58` | data row (score + initials) | 11 |
| `0x40` | full-line label, e.g. "ENTER YOUR NAME." | — |

All of them take the same shape:

    marker  Y  attr  count  payload[count]

`Y` is a screen pixel row, and the glyph top sits at `Y - 5`: the renderer
inserts 2 rows of top padding inside an 8-row char cell, and the -5 is the
resulting glyph-top offset. This bottom-anchoring is the same convention the
round banner and the Kinnock egg use (`notes/menu.md`).

`count` is the number of payload bytes. For headers it INCLUDES the trailing
`0x24` period, since the period is just another glyph. For data rows it is
exactly 14 = 6 score digits + 3 spaces + 5 characters of `H _ I _ T`-style
initials.

Scores are stored MSB-first as 7 byte-digits with literal leading zeros
preserved, so the maximum is 9,999,999. The port's HUD renders 6 digits
instead — see `score_to_digits`.

## Worked examples (from the hi-score state)

```
0x8FD1: 38 26 47 02 01 24   header, col 2, white, label "1."
0x8FD7: 38 36 47 02 02 24   header, col 2, white, label "2."
0x8FDD: 38 46 46 02 03 24   header, col 2, yellow, label "3."
... the colour fades by 1 every two rows ...
```

```
0x900E: 58 26 47 0E 01 00 00 00 00 00 00 26 26 26 11 26 12 26 1D
        ^^          ^^ ^^                ^^ ^^ ^^         ^^ ^^ ^^
        data row    count=14, then       3 spaces          H  I  T
                    7 score digits
```

The seven digit bytes encode the score one decimal digit per byte, with
leading zeros painted as glyphs rather than suppressed.

## Records can carry inline addresses

At `0x8FC0..0x8FD0`:

```
17 0D 26  <00 00 60 8F>  47 08  10 0A 16 0E 26 26 18 17
^^ ^^ ^^   ^^^^^^^^^^^^  ^^ ^^   G  A  M  E  sp sp O  N
N  D  sp   addr 0x8F60   attr    col=8
```

so a record can link to another row by address.
