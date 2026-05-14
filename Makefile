# batty — MS-DOS recreation of Batty (Elite, 1987) targeting 8086 + VGA.
# Mirrors the adlib-rng toolchain layout: Open Watcom v2 + mtools + QEMU.

WATCOM_DIR ?= vendor/openwatcom-v2/current-build-2026-04-20
HOST_OS    := $(shell uname -s)
HOST_ARCH  := $(shell uname -m)
ifeq ($(HOST_OS),Darwin)
  ifeq ($(HOST_ARCH),arm64)
    WATCOM_BIN := $(WATCOM_DIR)/macos-arm64
  else
    WATCOM_BIN := $(WATCOM_DIR)/macos-x64
  endif
else
  WATCOM_BIN := $(WATCOM_DIR)/linux-amd64
endif

WCC        = $(WATCOM_BIN)/wcc
WLINK      = $(WATCOM_BIN)/wlink
WATCOM_H   = $(WATCOM_DIR)/h
WATCOM_LIB = $(WATCOM_DIR)/lib286/dos

# -0    = 8086 instruction set
# -ms   = small memory model (64K code + 64K data)
# -os   = optimize for size
# -s    = no stack overflow checks
# -za99 = C99
# -w4 -we = max warnings, treat as errors
# -oi   = inline intrinsics (memset/memcpy)
WCCFLAGS = -0 -ms -os -s -za99 -w4 -we -oi -i=$(WATCOM_H)

SRC     = src/main.c
OBJ     = $(SRC:src/%.c=build/%.obj)
HEADERS = $(wildcard src/*.h)
EXE     = build/batty.exe

ASSETS  = assets/loading.bin assets/hi_score.bin assets/main_menu.bin \
          assets/font.bin assets/markup.bin assets/main_menu_markup.bin \
          assets/indicator.bin assets/bottom_sprites.bin \
          assets/levels.bin assets/sprite_cache.bin assets/level_attrs.bin \
          assets/bg_tile.bin assets/bat_l1.bin assets/frame_l1.bin \
          assets/brick_bitmaps.bin assets/lives_l1.bin
HISCORE_SNAP      ?= build/snapshots/20260513T202038Z/screen.scr
MAINMENU_SNAP     ?= build/snapshots/20260513T202041Z/screen.scr
MAINMENU_SNAP_RAM ?= build/snapshots/20260513T202041Z/ram_4000_FFFF.bin
LEVEL1_SNAP_RAM   ?= build/snapshots/20260513T202101Z/ram_4000_FFFF.bin

FLOPPY_SRC      ?= vendor/msdos/floppy-minimal.img
FLOPPY_OUT       = build/batty.img        # `make run`: 2-state menu loop
TEST_FLOPPY_OUT  = build/batty-test.img   # `make test`: full 4-state cycle

ZESARUX ?= ../generaly/tools/zesarux/src/zesarux
ZRCP_PORT ?= 10000

.PHONY: all clean run floppy assets help run-original snapshot candidates regions test

all: $(EXE) $(ASSETS)

help:
	@echo "batty targets:"
	@echo "  all           build $(EXE) + assets (default)"
	@echo "  assets        decode original/*.scr into assets/"
	@echo "  floppy        pack $(EXE) + assets onto $(FLOPPY_OUT)"
	@echo "  run           build the floppy and boot it in QEMU (our recreation)"
	@echo "  run-original  boot the ORIGINAL batty.tap in ZEsarUX with ZRCP open"
	@echo "  snapshot      dump RAM + screen from running ZEsarUX -> build/snapshots/"
	@echo "  regions       static scan of main blob -> build/regions.{txt,blockdef}"
	@echo "  candidates    render bytedata regions as PNGs -> assets/candidates/"
	@echo "  clean         remove build/"

build:
	@mkdir -p build

build/%.obj: src/%.c $(HEADERS) | build
	$(WCC) $(WCCFLAGS) -fo=$@ $<

$(EXE): $(OBJ)
	$(WLINK) name $@ format dos $(addprefix file ,$(OBJ)) libpath $(WATCOM_LIB) library clibs.lib

assets: $(ASSETS)

assets/loading.bin: original/Batty.scr scripts/extract_scr.py
	python3 scripts/extract_scr.py $< $@

assets/hi_score.bin: $(HISCORE_SNAP) scripts/extract_scr.py
	python3 scripts/extract_scr.py $< $@

assets/main_menu.bin: $(MAINMENU_SNAP) scripts/extract_scr.py
	python3 scripts/extract_scr.py $< $@

assets/font.bin: original/blocks/03_DATA_headless.dat.bin scripts/extract_font.py
	python3 scripts/extract_font.py

# Main-menu markup: snap2 RAM 0x954D..0x9613 (199 B). Includes the
# "1 UP"/"2 UP" titles and "000000" score displays as proper markup
# records (cols 2/3/24/25, attr 0x07 = non-bright white).
# End is *exactly* the last record's last byte — any trailing bytes
# can contain spurious multiple-of-8 values that the parser would
# misread as new records.
assets/main_menu_markup.bin: $(MAINMENU_SNAP_RAM)
	@python3 -c "from pathlib import Path; \
		Path('$@').write_bytes(Path('$<').read_bytes()[0x954D-0x4000 : 0x9614-0x4000])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Player indicators: 32x16px each. P1 at blob 0x92C1, P2 at 0x9303
# (= P1+66, contiguous). Bundled together as 132 bytes; the C side
# splits them. Format per indicator: (w=4, h=16) header + 64 px bytes.
assets/indicator.bin: original/blocks/03_DATA_headless.dat.bin
	@python3 -c "from pathlib import Path; \
		Path('$@').write_bytes(Path('$<').read_bytes()[0x92C1-0x6800 : 0x92C1-0x6800+132])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# 15 static level layouts: 180 B each (12 rows x 15 cols, 1 B/cell)
# at blob 0x6CDB..0x7766. Pointer table at 0x6CBD (15 LE 16-bit ptrs)
# isn't shipped — we sequence levels by index in C since the deltas
# are uniform (0xB4).
assets/levels.bin: original/blocks/03_DATA_headless.dat.bin
	@python3 -c "from pathlib import Path; \
		Path('$@').write_bytes(Path('$<').read_bytes()[0x6CDB-0x6800 : 0x7766-0x6800+1])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Gameplay sprite cache: 3584 B at RAM 0xE400..0xF1FF from snap3
# (level 1, just started). Holds the 8 brick base sprites plus
# pre-shifted variants and bat/ball/HUD chunks. Cache slots are
# indexed directly by cell-value (`cell * 16`) per notes/levels.md.
assets/sprite_cache.bin: $(LEVEL1_SNAP_RAM)
	@python3 -c "from pathlib import Path; \
		Path('$@').write_bytes(Path('$<').read_bytes()[0xE400-0x4000 : 0xF200-0x4000])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Per-level brick attribute bands extracted from the GT captures.
# 15 levels x 12 char rows x 32 cols = 5760 B. Char rows 2..13 cover
# the brick field; we ship the full 12 rows so the C lookup stays
# simple. Generated only if the GT captures exist (run
# `python3 scripts/capture_levels.py` to refresh).
assets/level_attrs.bin: build/level_gt/level_01.scr scripts/extract_level_attrs.py
	python3 scripts/extract_level_attrs.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# 16x16-pixel honeycomb tile used as the playfield bg under the bricks.
# Pulled from a pure-bg region of level_01.scr; the tile bitmap is
# colour-invariant (1bpp; per-level paper/ink applies at render time).
assets/bg_tile.bin: build/level_gt/level_01.scr scripts/extract_bg_tile.py
	python3 scripts/extract_bg_tile.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Bat + on-bat ball composite at level-1 start: 4 bytes x 16 rows
# (= 32 x 16 px) at L1 pixel (112, 167). Includes the ball resting
# on the bat. Static L1 snapshot; will be replaced by per-frame bat
# render once Phase E (motion) lands.
assets/bat_l1.bin: build/level_gt/level_01.scr scripts/extract_bat.py
	python3 scripts/extract_bat.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Perimeter frame (top HUD + left + right cyan strips). 3 strips
# painted as raw pixels + per-char attrs; bottom edge has no frame
# ornament, so we skip it. ~1.3 KB total.
assets/frame_l1.bin: build/level_gt/level_01.scr scripts/extract_frame.py
	python3 scripts/extract_frame.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Per-cell 16x8 brick bitmaps extracted from the 15 GT captures.
# Bypasses the original's multi-pass neighbour-aware compositor
# (sub_b765h + sub_c101h) until we port that pipeline; for now we
# ship the final composited bitmap per (level, row, col).
# 15 * 180 * 16 = 43200 B.
assets/brick_bitmaps.bin: build/level_gt/level_01.scr scripts/extract_brick_bitmaps.py
	python3 scripts/extract_brick_bitmaps.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Second life indicator at bottom-left (the first one is captured by
# the wide frame strip). 2 bytes wide x 8 rows = 16 B.
assets/lives_l1.bin: build/level_gt/level_01.scr scripts/extract_lives.py
	python3 scripts/extract_lives.py $@
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

# Bottom decorative sprite + arrow combined: 32x13 each (4 bytes
# width × 13 rows) stored bottom-to-top per sub_b5f8h's convention.
# Source: blob 0x938E (P1) and 0x93C4 (P2). Each blob has a (w, h)
# header at -2: 0x938C/0x93C2 = `04 0D`. We bundle just the bodies
# (52 B each) and hardcode dimensions C-side. Visual layout
# top-to-bottom:
#   rows 0..4 :  decorative sprite (was our previous 20-B extraction)
#   rows 5..6 :  blank gap
#   rows 7..12:  small downward arrow
assets/bottom_sprites.bin: original/blocks/03_DATA_headless.dat.bin
	@python3 -c "from pathlib import Path; b=Path('$<').read_bytes(); \
		Path('$@').write_bytes(b[0x938E-0x6800:0x938E-0x6800+52] + \
		                        b[0x93C4-0x6800:0x93C4-0x6800+52])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

floppy: $(FLOPPY_OUT)

# Both floppies ship the same EXE + assets; only AUTOEXEC.BAT differs.
$(FLOPPY_OUT): $(EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(EXE) ::BATTY.EXE
	mcopy -i $@ -o assets/loading.bin  ::LOADING.BIN
	mcopy -i $@ -o assets/hi_score.bin ::HISCORE.BIN
	mcopy -i $@ -o assets/main_menu.bin ::MAINMENU.BIN
	mcopy -i $@ -o assets/font.bin     ::FONT.BIN
	mcopy -i $@ -o assets/markup.bin   ::MARKUP.BIN
	mcopy -i $@ -o assets/main_menu_markup.bin ::MENUMARK.BIN
	mcopy -i $@ -o assets/indicator.bin ::INDICAT.BIN
	mcopy -i $@ -o assets/bottom_sprites.bin ::BOTSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/sprite_cache.bin ::CACHE.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/bat_l1.bin ::BATL1.BIN
	mcopy -i $@ -o assets/frame_l1.bin ::FRAMEL1.BIN
	mcopy -i $@ -o assets/brick_bitmaps.bin ::BRICKBMS.BIN
	mcopy -i $@ -o assets/lives_l1.bin ::LIVESL1.BIN
	@printf '@ECHO OFF\r\nBATTY\r\n' > build/AUTOEXEC.BAT
	mcopy -i $@ -o build/AUTOEXEC.BAT ::AUTOEXEC.BAT
	@echo "Floppy ready: $@  (menu-only cycle)"

$(TEST_FLOPPY_OUT): $(EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(EXE) ::BATTY.EXE
	mcopy -i $@ -o assets/loading.bin  ::LOADING.BIN
	mcopy -i $@ -o assets/hi_score.bin ::HISCORE.BIN
	mcopy -i $@ -o assets/main_menu.bin ::MAINMENU.BIN
	mcopy -i $@ -o assets/font.bin     ::FONT.BIN
	mcopy -i $@ -o assets/markup.bin   ::MARKUP.BIN
	mcopy -i $@ -o assets/main_menu_markup.bin ::MENUMARK.BIN
	mcopy -i $@ -o assets/indicator.bin ::INDICAT.BIN
	mcopy -i $@ -o assets/bottom_sprites.bin ::BOTSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/sprite_cache.bin ::CACHE.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/bat_l1.bin ::BATL1.BIN
	mcopy -i $@ -o assets/frame_l1.bin ::FRAMEL1.BIN
	mcopy -i $@ -o assets/brick_bitmaps.bin ::BRICKBMS.BIN
	mcopy -i $@ -o assets/lives_l1.bin ::LIVESL1.BIN
	@printf '@ECHO OFF\r\nSET BATTYALL=1\r\nBATTY\r\n' > build/AUTOEXEC-T.BAT
	mcopy -i $@ -o build/AUTOEXEC-T.BAT ::AUTOEXEC.BAT
	@echo "Test floppy ready: $@  (full 4-state cycle)"

run: $(FLOPPY_OUT)
	bash scripts/run.sh $(FLOPPY_OUT)

# --- Reverse-engineering helpers ---

regions: build/regions.txt

build/regions.txt build/regions.blockdef: \
		original/blocks/03_DATA_headless.dat.bin scripts/scan_regions.py
	@mkdir -p build
	python3 scripts/scan_regions.py $< build/regions.txt build/regions.blockdef

candidates: build/regions.blockdef
	python3 scripts/render_candidates.py \
		original/blocks/03_DATA_headless.dat.bin \
		build/regions.blockdef assets/candidates

test: $(TEST_FLOPPY_OUT)
	python3 scripts/test_visual.py --floppy $(TEST_FLOPPY_OUT)

run-original:
	$(ZESARUX) --noconfigfile --machine 48k \
		--enable-remoteprotocol --remoteprotocol-port $(ZRCP_PORT) \
		--quickexit $(CURDIR)/original/batty.tap

snapshot:
	@mkdir -p build/snapshots
	python3 scripts/snapshot_ram.py build/snapshots

clean:
	rm -rf build
