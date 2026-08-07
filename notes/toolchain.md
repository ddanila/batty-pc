# Toolchain and target

batty targets a **386 in 32-bit protected mode**, built with Open Watcom v2
from `vendor/openwatcom-v2/current-build-<date>/`.

```
wpp386 -bt=dos -3 -os -s -w4 -we -oi           # 32-bit DOS, 386 ISA
wlink  format os2 le option stub=wstub.exe     # LE image + real-mode stub
       library clib3r.lib library plib3r.lib   # 32-bit DOS C and C++ libs
```

The floppy carries `BATTY.EXE` plus the extender as `DOS4GW.EXE`.

## Flat memory model

The extender identity-maps the first megabyte, so:

- `vga` is an ordinary pointer to `0x000A0000` — no segment, no `__far`
- there is no near/far distinction anywhere, so no `_fmemcpy` / `_fmemset`
  / `_fmalloc`
- there is no 64 KB code or data ceiling, so data structures are sized for
  clarity rather than to fit a segment
- `memcpy` is the fastest bulk copy available; there is nothing to gain
  from hand-written string-op assembly

The video engine's blit writes 8 pixels as two aligned 32-bit stores
straight from the expansion table. `(BORDER_Y + y) * 320 + 32` is divisible
by 4 and every byte column advances by 8, so the alignment always holds.

Use fixed-width-by-convention types in anything shared with the host test
build: `unsigned long` is 4 bytes under Watcom 32-bit but **8 bytes** on a
64-bit host, which silently doubles the width of a store. `unsigned int` is
4 bytes on both.

## Extender: DOS32A, not DOS/4GW

Both ship in the OW2 snapshot. DOS32A is used because:

1. It is 27 KB against DOS/4GW's 265 KB, on a 1.44 MB floppy.
2. **Under DOS/4GW the game hangs on the title screen.** Its hardware
   interrupt reflection does not deliver the INT 9 chain that `new_int9`
   installs, so no keypress reaches the state machine and the state
   machine never advances. Under DOS32A the same unmodified INT 8 / INT 9
   handlers work.

Point 2 is worth remembering: the symptom looks exactly like a broken
keyboard handler, and the natural conclusion — "protected mode needs
DPMI-aware interrupt handlers" — is wrong here. The handlers are fine.

## C++ dialect

The sources are `.cpp`, compiled by `wpp386`. Open Watcom's C++ is
**C++98 plus `static_assert`, `decltype` and the `>>` template close**.

Available and used: classes, constructors/destructors, references,
overloading, default arguments, `bool`, `inline` functions, `static_assert`,
const-correctness, templates.

Not available: `enum class`, `constexpr`, `nullptr`, `auto`, range-`for`,
the C++11 library. Template *deduction* is also weak — it cannot deduce
array extents, and drops `const` when deducing from an array inside a
`const` struct.

Two things bit during the switch, both worth knowing before writing new
code:

- **No tentative definitions.** C lets `static int x;` appear twice at file
  scope; C++ treats each as a definition. A forward `static int x;` followed
  later by `static int x = 0;` is an error — the first one already defines
  and zero-initialises it, so delete the second.
- **`inp()` returns `unsigned int`.** Assigning it to `unsigned char`
  warns, and the build is `-we`. Cast at the call site.

The host test build uses `c++ -std=c++98`, deliberately matching the
compiler's ceiling so a host-only feature cannot creep into shared code.

## Verifying a refactor changed no code

`wdis` is vendored for this. For pure code motion there is also a cheaper
check that needs no disassembly — preprocess both revisions, normalise and
sort, and diff the line multiset:

```sh
wcc386 <flags> -p old.c | grep -v '^\s*$' | sed 's/[[:space:]]\+/ /g' | sort > old.s
wcc386 <flags> -p new.c | grep -v '^\s*$' | sed 's/[[:space:]]\+/ /g' | sort > new.s
diff old.s new.s        # empty => same statements, only order changed
```

See `notes/video-engine.md` for how this was used on the zxvga extraction.
