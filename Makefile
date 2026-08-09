# batty — MS-DOS recreation of Batty (Elite, 1987) targeting 386 + VGA.
# Toolchain: Open Watcom v2 + mtools + QEMU.
#
# 32-bit protected mode via a flat memory model: the game is an LE
# executable loaded by the DOS32A extender, which ships on the floppy.

WATCOM_DIR ?= vendor/openwatcom-v2/current-build-2026-08-01
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

WPP        = $(WATCOM_BIN)/wpp386
WLINK      = $(WATCOM_BIN)/wlink
WDIS       = $(WATCOM_BIN)/wdis
WATCOM_H   = $(WATCOM_DIR)/h
WATCOM_LIB = $(WATCOM_DIR)/lib386/dos
# Real-mode stub bound into the LE image; it loads the extender at startup.
WSTUB      = $(WATCOM_DIR)/dos/wstub.exe
# DOS32A, not DOS/4GW: a tenth the size, and DOS/4GW's hardware-interrupt
# reflection breaks the INT 9 keyboard chain (the game hangs on the title
# screen). The stub looks for the extender under the name DOS4GW.EXE.
EXTENDER   = $(WATCOM_DIR)/dos/DOS32A.EXE

# -bt=dos = 32-bit DOS target      -3  = 386 instruction set
# -os     = optimize for size      -s  = no stack overflow checks
# -oi     = inline intrinsics (memset/memcpy)
# -w4 -we = max warnings, treat as errors
# -zastd=c++0x enables the handful of C++11 features Open Watcom has
# (static_assert, decltype, the >> template close). See notes/toolchain.md.
WPPFLAGS = -bt=dos -3 -os -s -w4 -we -oi -zastd=c++0x -i=$(WATCOM_H)
# `format os2 le` is the linear executable the extender loads.
WLINKFMT = format os2 le option stub=$(WSTUB)

# Modules compile to their own object and expose a header; main.cpp is
# the state machine and wiring. (zxvga.cpp is still #included -- it has no
# separate object yet.)
MODULES = src/rng.cpp src/physics.cpp src/assets.cpp src/zxvga.cpp src/bricks.cpp src/sound.cpp src/hud.cpp src/objects.cpp src/weapons.cpp src/replay_parse.cpp src/replay.cpp src/enemies.cpp src/bonus_codes.cpp src/scoring.cpp
SRC     = src/main.cpp $(MODULES)
OBJ      = $(patsubst src/%.cpp,build/%.obj,$(SRC))
TEST_OBJ = $(patsubst src/%.cpp,build/%-test.obj,$(SRC))
# src/zxvga.cpp (the video engine) is #included by main.cpp rather than
# compiled separately, so it is a build dependency alongside the headers.
HEADERS = $(wildcard src/*.h)
EXE     = build/batty.exe
TEST_EXE = build/batty-test.exe

# Which test EXE the test-floppy rule packs as BATTY.EXE.
FLOPPY_TEST_EXE ?= $(TEST_EXE)

# hi_score.bin and main_menu.bin are NOT floppy assets: the port renders
# both screens from MARKUP.BIN / MENUMARK.BIN markup, and these two
# 48 KB captures exist only as test-visual's references. They rode
# along on every image until 2026-08-09; check_floppy_assets now holds
# the line in both directions.
ASSETS  = assets/loading.bin assets/hi_score.bin assets/main_menu.bin \
          assets/font.bin assets/markup.bin assets/main_menu_markup.bin \
          assets/indicator.bin assets/bottom_sprites.bin \
          assets/hud_sprites.bin \
          assets/levels.bin assets/level_attrs.bin \
          assets/bg_tile.bin assets/frame_l1.bin \
          assets/sprites.bin assets/separator.bin assets/border.bin \
          assets/random_seed.bin
HISCORE_SNAP      ?= build/snapshots/20260513T202038Z/screen.scr
MAINMENU_SNAP     ?= build/snapshots/20260513T202041Z/screen.scr
MAINMENU_SNAP_RAM ?= build/snapshots/20260513T202041Z/ram_4000_FFFF.bin
LEVEL1_SNAP_RAM   ?= build/snapshots/20260513T202101Z/ram_4000_FFFF.bin

FLOPPY_SRC      ?= vendor/msdos/floppy-minimal.img
# `make run`: normal interactive boot image.
FLOPPY_OUT       = build/batty.img
# `make test`: full 4-state visual-regression image. Honours BATTY_TEST_FLOPPY
# so scripts/run_gates_parallel.py can give each concurrent gate its own image
# (build/batty-test-<i>.img); the AUTOEXEC scratch is derived per-floppy so
# concurrent assembly never collides on one file.
TEST_FLOPPY_OUT  = $(if $(BATTY_TEST_FLOPPY),$(BATTY_TEST_FLOPPY),build/batty-test.img)
AUTOEXEC_T       = build/AUTOEXEC-T-$(notdir $(basename $(TEST_FLOPPY_OUT))).BAT

ZESARUX ?= tools/zesarux/src/zesarux
ZESARUX_CONFIGURE_OPTS ?= --enable-sdl2
ifeq ($(HOST_OS),Linux)
  ZESARUX_AO ?= sdl
  ZESARUX_VO ?= sdl
  ZESARUX_RUN_OPTS ?= --zoom 4
else
  ZESARUX_AO ?= null
  ZESARUX_VO ?=
  ZESARUX_RUN_OPTS ?=
endif
ZRCP_PORT ?= 10000

BOX86_BIN       ?= /home/ddanila/.local/86box/bin/86Box
BOX86_VM_DIR    ?= build/86box
BOX86_MACHINE   ?= ibmxt
BOX86_GFXCARD   ?= vga
BOX86_FDD_TYPE  ?= 35_2hd
BOX86_ASSETPATH ?= /home/ddanila/fun/86Box/src/unix/assets
BOX86_ROMPATH   ?= /home/ddanila/fun/86Box-roms

PROFILE_LEVEL  ?= 1
PROFILE_FRAMES ?= 180
PROFILE_WAIT   ?= 25
PROFILE_BALL_OBJECT ?= 02008000A0001802020C000008070000000000000080
PROFILE_BALL_STUCK  ?= 0
# A ball baked INSIDE the brick band (y=0x48) so it bounces through the
# bricks and destroys them — a normal-play (no-laser) brick-hit profile, the
# scenario that exercises the brick-hit redraw path without bullet noise.
PROFILE_BALL_FIELD  ?= 0200800048001802020C000008070000000000000080
# Laser bat (bonus_applied=0x01 at byte 0x14) for the brick-destruction
# profile: with auto-fire, bullets continuously destroy bricks so the
# incremental band-rebuild path is exercised + measurable.
PROFILE_BAT_LASER   ?= 01017400AD000000040DEFAE1C0A74AD040DF0000180
# A/B toggle: empty = incremental scoped rebuild (default); 1 = force the
# whole-band rebuild baseline. `make profile-bricks` vs `... FULL_BAND=1`.
FULL_BAND           ?=

# .PHONY, derived from the Makefile itself rather than hand-listed.
#
# The hand-written list had drifted to 85 of 109 task targets: everything
# added in the last stretch — test-life-loss, test-level-advance,
# test-gate-index, parity-check-parallel and 21 others — was missing. A
# target that is not .PHONY silently does nothing if a file of that name
# ever appears, which is the same "runs green while running nothing"
# failure as a host suite that is never invoked.
#
# This is the fourth hand-maintained list in this repo found out of sync
# (after test-fast vs parity-check vs CI, and the BATTY_* passthrough).
# Deriving it means there is no list to drift.
PHONY_TARGETS := $(shell grep -oE '^(test|parity|replay|profile|run|clean|all|help|floppy|assets|snapshot|candidates|regions)[a-zA-Z0-9_.-]*:' $(lastword $(MAKEFILE_LIST)) | tr -d ':' | sort -u)
.PHONY: $(PHONY_TARGETS)

all: $(EXE) $(ASSETS)

help:
	@echo "batty targets:"
	@echo "  all           build $(EXE) + assets (default)"
	@echo "  assets        decode original/*.scr into assets/"
	@echo "  floppy        pack $(EXE) + assets onto $(FLOPPY_OUT)"
	@echo "  run           build the floppy and boot it in QEMU (our recreation)"
	@echo "  run-86box     build the floppy and boot it in 86Box (IBM XT + VGA)"
	@echo "  profile-auto  run deterministic headless QEMU render profile"
	@echo "  profile-86box run 86Box with BATTY_RENDER_PROFILE=1 (sound off)"
	@echo "  read-profile  extract and print PROFILE.TXT from $(FLOPPY_OUT)"
	@echo "  run-original  boot the ORIGINAL batty.tap in ZEsarUX with ZRCP open"
	@echo "  snapshot      dump RAM + screen from running ZEsarUX -> build/snapshots/"
	@echo "  regions       static scan of main blob -> build/regions.{txt,blockdef}"
	@echo "  candidates    render bytedata regions as PNGs -> assets/candidates/"
	@echo ""
	@echo "  tests — see notes/testing.md for what each gate covers"
	@echo "  test-fast     every host suite + every source gate, no emulator, seconds"
	@echo "  parity-check-parallel   the full QEMU suite, in parallel (~6 min)"
	@echo "  parity-check-full       the above plus the ZEsarUX-oracle gates"
	@echo "  test          the four-state cycle on its own"
	@echo "  test-video    host-native video-engine tests (clash model, no emulator)"
	@echo ""
	@echo "  Individual gates are not listed here: the list went stale once"
	@echo "  already (it named 13 and omitted test-fast). notes/testing.md"
	@echo "  has the index, and test-gate-index keeps it complete."
	@echo ""
	@echo "  clean         remove build/"

build:
	@mkdir -p build

build/%.obj: src/%.cpp $(HEADERS) | build
	$(WPP) $(WPPFLAGS) -fo=$@ $<

build/%-test.obj: src/%.cpp $(HEADERS) | build
	$(WPP) $(WPPFLAGS) -dBATTY_SCORELESS_HUD -fo=$@ $<

$(EXE): $(OBJ)
	$(WLINK) name $@ $(WLINKFMT) $(addprefix file ,$(OBJ)) libpath $(WATCOM_LIB) library clib3r.lib library plib3r.lib

$(TEST_EXE): $(TEST_OBJ)
	$(WLINK) name $@ $(WLINKFMT) $(addprefix file ,$(TEST_OBJ)) libpath $(WATCOM_LIB) library clib3r.lib library plib3r.lib

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
# Perimeter frame (top HUD + left + right cyan strips). Three strips of
# raw PIXELS — no attrs; paint_frame_to_buff takes those from
# level_attrs.bin. 4416 B for four colour cycles.
#
# Checked in rather than built, because a clean checkout has no
# build/level_gt/ captures. It IS regenerable where they exist:
# `python3 scripts/extract_frame.py assets/frame_l1.bin` reproduced the
# committed file byte for byte on 2026-08-09.
#
# This note used to say regeneration was impossible — the frame needed a
# GT captured with the L6853 lives-skip patch while the test GT needs
# lives present, "two contradictory requirements". That stopped being
# true when FRAME_SIDE_W narrowed to 1: the side strip is now the
# ornament column alone, which the lives indicators never reach. See
# notes/state4-bat-band-triage.md for the original problem.

# Sprite block extracted verbatim from the original game's program at
# $7A8C..$8D46 (offset $128c..$2546 within 03_DATA_headless.dat.bin,
# which loads at $6800). Contains all masked sprites we need:
#   spr_big_ball, spr_lives_indicator, spr_ball_normal,
#   spr_bat_normal, spr_bat_big, spr_ufo_1..6, spr_bird_1..6,
#   spr_alien_blast_1..5, spr_bonus_* through spr_bonus_triple_ball
# (offsets recorded in main.cpp). Format per sprite: (width_bytes,
# height_rows) + rows of (mask, pixel) pairs per byte-column,
# drawn via blit_masked_sprite.
# The tape slice is patched to reproduce the original's BOOT-TIME state
# (notes/bird-render-parity.md, sprite-encoding decode): the game's
# gfx_inverse pass (preparation.asm) XORs each sprite's pix bytes with
# its mask, and bird_4's header claims 15 rows where the layout allots
# only 14 -- the walk overruns 3 pairs into spr_bird_5, net-corrupting
# its header height ($12^$03 = $11 -> the original draws 17 rows, not
# 18) and leaving its first data pair UN-transformed (live pix 00 under
# mask $30 renders INK via the original's (m|s)^pix; the port's
# (~m&d)|(m&p) needs pix=$30 for the same ink). Everything else is
# encoding-equivalent between tape data + the port blit and live data +
# the original blit (verified byte-exhaustively over all 49 sprites
# against the in-game snapshot).
# The two-player court divider, spr_separator at $7A2A. It sits just
# BELOW assets/sprites.bin's range ($7A8C..$8F50) — its 96-byte body
# ends exactly where that blob begins — so it needs its own extraction
# rather than a widened range, which would shift every existing offset.
# 98 bytes: the (width=2 bytes, height=$18 rows) header plus the body.
# File offset = Z80 addr - 0x6800 ($7A8C -> 0x128C).
# The perimeter-frame sprites, $6B3F..$6CBC: the bold/thin side pair
# (each immediately followed by its right-hand twin, which the original
# reaches by letting DE walk off the end of the left one) and the six
# distinct horizontal pieces set_border_horizontal draws. 382 bytes.
# Below sprites.bin's $7A8C range, so its own extraction.
# File offset = Z80 addr - 0x6800.
assets/border.bin: original/blocks/03_DATA_headless.dat.bin Makefile
	@python3 -c "open('$@','wb').write(open('$<','rb').read()[0x033F:0x04BD])"
	@echo "wrote $@ ($$(wc -c < $@) bytes, perimeter frame sprites)"

assets/separator.bin: original/blocks/03_DATA_headless.dat.bin Makefile
	@python3 -c "open('$@','wb').write(open('$<','rb').read()[0x122A:0x128C])"
	@echo "wrote $@ ($$(wc -c < $@) bytes, spr_separator)"

assets/sprites.bin: original/blocks/03_DATA_headless.dat.bin Makefile
	@python3 -c "import sys; b=bytearray(open('$<','rb').read()[0x128c:0x2546]); \
		b[0x0CED]=0x11; b[0x0CEF]=0x30; \
		open('$@','wb').write(bytes(b))"
	@echo "wrote $@ ($$(wc -c < $@) bytes, bird_5 boot-state patch)"

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

assets/hud_sprites.bin: original/blocks/03_DATA_headless.dat.bin
	@python3 -c "from pathlib import Path; b=Path('$<').read_bytes(); \
		Path('$@').write_bytes(b[0x68ED-0x6800:0x6A15-0x6800])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

assets/random_seed.bin: original/blocks/03_DATA_headless.dat.bin
	@python3 -c "from pathlib import Path; b=Path('$<').read_bytes(); \
		Path('$@').write_bytes(b[0x8000-0x6800:0xA000-0x6800])"
	@echo "wrote $@ ($$(wc -c < $@) bytes)"

floppy: $(FLOPPY_OUT)

# Both floppies ship the same EXE + assets; only AUTOEXEC.BAT differs.
# FLOPPY_EXE selects which EXE is packed.
FLOPPY_EXE ?= $(EXE)
$(FLOPPY_OUT): $(FLOPPY_EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(FLOPPY_EXE) ::BATTY.EXE
	mcopy -i $@ -o $(EXTENDER) ::DOS4GW.EXE
	mcopy -i $@ -o assets/loading.bin  ::LOADING.BIN
	mcopy -i $@ -o assets/font.bin     ::FONT.BIN
	mcopy -i $@ -o assets/markup.bin   ::MARKUP.BIN
	mcopy -i $@ -o assets/main_menu_markup.bin ::MENUMARK.BIN
	mcopy -i $@ -o assets/indicator.bin ::INDICAT.BIN
	mcopy -i $@ -o assets/bottom_sprites.bin ::BOTSPR.BIN
	mcopy -i $@ -o assets/hud_sprites.bin ::HUDSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/sprites.bin ::SPRITES.BIN
	mcopy -i $@ -o assets/separator.bin ::SEPARAT.BIN
	mcopy -i $@ -o assets/border.bin ::BORDER.BIN
	mcopy -i $@ -o assets/random_seed.bin ::RANDOM.BIN
	@printf '@ECHO OFF\r\n' > build/AUTOEXEC.BAT
	@if [ -n "$$BATTY_NOSOUND" ]; then \
	    printf 'SET BATTY_NOSOUND=%s\r\n' "$$BATTY_NOSOUND" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_SOUND_OFF" ]; then \
	    printf 'SET BATTY_SOUND_OFF=%s\r\n' "$$BATTY_SOUND_OFF" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_RENDER_PROFILE" ]; then \
	    printf 'SET BATTY_RENDER_PROFILE=%s\r\n' "$$BATTY_RENDER_PROFILE" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_PROFILE_AUTO_FRAMES" ]; then \
	    printf 'SET BATTY_PROFILE_AUTO_FRAMES=%s\r\n' "$$BATTY_PROFILE_AUTO_FRAMES" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_START_LEVEL" ]; then \
	    printf 'SET BATTY_START_LEVEL=%s\r\n' "$$BATTY_START_LEVEL" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_LEVEL" ]; then \
	    printf 'SET BATTY_LEVEL=%s\r\n' "$$BATTY_LEVEL" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_REPLAY_BALL_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_OBJECT=%s\r\n' "$$BATTY_REPLAY_BALL_OBJECT" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_REPLAY_BALL_STUCK" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_STUCK=%s\r\n' "$$BATTY_REPLAY_BALL_STUCK" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_REPLAY_BAT_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BAT_OBJECT=%s\r\n' "$$BATTY_REPLAY_BAT_OBJECT" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_AUTO_FIRE" ]; then \
	    printf 'SET BATTY_AUTO_FIRE=%s\r\n' "$$BATTY_AUTO_FIRE" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_SUPPRESS_NO_BALL_DEATH" ]; then \
	    printf 'SET BATTY_SUPPRESS_NO_BALL_DEATH=%s\r\n' "$$BATTY_SUPPRESS_NO_BALL_DEATH" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_FULL_BAND_REBUILD" ]; then \
	    printf 'SET BATTY_FULL_BAND_REBUILD=%s\r\n' "$$BATTY_FULL_BAND_REBUILD" >> build/AUTOEXEC.BAT ; \
	fi
	@if [ -n "$$BATTY_REPLAY_MULTIBALL" ]; then \
	    printf 'SET BATTY_REPLAY_MULTIBALL=%s\r\n' "$$BATTY_REPLAY_MULTIBALL" >> build/AUTOEXEC.BAT ; \
	fi
	@printf 'BATTY\r\n' >> build/AUTOEXEC.BAT
	mcopy -i $@ -o build/AUTOEXEC.BAT ::AUTOEXEC.BAT
	@echo "Floppy ready: $@  (menu-only cycle)"

$(TEST_FLOPPY_OUT): $(FLOPPY_TEST_EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(FLOPPY_TEST_EXE) ::BATTY.EXE
	mcopy -i $@ -o $(EXTENDER) ::DOS4GW.EXE
	mcopy -i $@ -o assets/loading.bin  ::LOADING.BIN
	mcopy -i $@ -o assets/font.bin     ::FONT.BIN
	mcopy -i $@ -o assets/markup.bin   ::MARKUP.BIN
	mcopy -i $@ -o assets/main_menu_markup.bin ::MENUMARK.BIN
	mcopy -i $@ -o assets/indicator.bin ::INDICAT.BIN
	mcopy -i $@ -o assets/bottom_sprites.bin ::BOTSPR.BIN
	mcopy -i $@ -o assets/hud_sprites.bin ::HUDSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/sprites.bin ::SPRITES.BIN
	mcopy -i $@ -o assets/separator.bin ::SEPARAT.BIN
	mcopy -i $@ -o assets/border.bin ::BORDER.BIN
	mcopy -i $@ -o assets/random_seed.bin ::RANDOM.BIN
	@# BATTY_LEVEL env passthrough — without injecting `SET BATTY_LEVEL=N`
	@# into the DOS boot AUTOEXEC.BAT, the C-side getenv() at run_level
	@# never sees the host's env. The line is only emitted when the host
	@# var is non-empty, so `make test` defaults to L1 as before.
	@if [ -n "$$BATTY_LEVEL" ]; then \
	    printf '@ECHO OFF\r\nSET BATTYALL=1\r\nSET BATTY_LEVEL=%s\r\n' "$$BATTY_LEVEL" > $(AUTOEXEC_T) ; \
	else \
	    printf '@ECHO OFF\r\nSET BATTYALL=1\r\n' > $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_START_LEVEL" ]; then \
	    printf 'SET BATTY_START_LEVEL=%s\r\n' "$$BATTY_START_LEVEL" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_PROBE" ]; then \
	    printf 'SET BATTY_REPLAY_PROBE=%s\r\n' "$$BATTY_REPLAY_PROBE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_RANDOM" ]; then \
	    printf 'SET BATTY_REPLAY_RANDOM=%s\r\n' "$$BATTY_REPLAY_RANDOM" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_RANDOM_SEED" ]; then \
	    printf 'SET BATTY_REPLAY_RANDOM_SEED=%s\r\n' "$$BATTY_REPLAY_RANDOM_SEED" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_COUNTER" ]; then \
	    printf 'SET BATTY_REPLAY_COUNTER=%s\r\n' "$$BATTY_REPLAY_COUNTER" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_RNG_PERFRAME" ]; then \
	    printf 'SET BATTY_RNG_PERFRAME=%s\r\n' "$$BATTY_RNG_PERFRAME" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BAT_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BAT_OBJECT=%s\r\n' "$$BATTY_REPLAY_BAT_OBJECT" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_OBJECT=%s\r\n' "$$BATTY_REPLAY_BALL_OBJECT" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_STUCK" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_STUCK=%s\r\n' "$$BATTY_REPLAY_BALL_STUCK" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_HIDE_BALL" ]; then \
	    printf 'SET BATTY_HIDE_BALL=%s\r\n' "$$BATTY_HIDE_BALL" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_LIVES" ]; then \
	    printf 'SET BATTY_REPLAY_LIVES=%s\r\n' "$$BATTY_REPLAY_LIVES" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_SCORE" ]; then \
	    printf 'SET BATTY_REPLAY_SCORE=%s\r\n' "$$BATTY_REPLAY_SCORE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_CLEAR_BRICKS" ]; then \
	    printf 'SET BATTY_REPLAY_CLEAR_BRICKS=%s\r\n' "$$BATTY_REPLAY_CLEAR_BRICKS" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_HOLD_GAME_OVER" ]; then \
	    printf 'SET BATTY_HOLD_GAME_OVER=%s\r\n' "$$BATTY_HOLD_GAME_OVER" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_NOSOUND" ]; then \
	    printf 'SET BATTY_NOSOUND=%s\r\n' "$$BATTY_NOSOUND" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_SOUND_OFF" ]; then \
	    printf 'SET BATTY_SOUND_OFF=%s\r\n' "$$BATTY_SOUND_OFF" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_RENDER_PROFILE" ]; then \
	    printf 'SET BATTY_RENDER_PROFILE=%s\r\n' "$$BATTY_RENDER_PROFILE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_PROFILE_AUTO_FRAMES" ]; then \
	    printf 'SET BATTY_PROFILE_AUTO_FRAMES=%s\r\n' "$$BATTY_PROFILE_AUTO_FRAMES" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FULL_BAND_REBUILD" ]; then \
	    printf 'SET BATTY_FULL_BAND_REBUILD=%s\r\n' "$$BATTY_FULL_BAND_REBUILD" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_SUPPRESS_NO_BALL_DEATH" ]; then \
	    printf 'SET BATTY_SUPPRESS_NO_BALL_DEATH=%s\r\n' "$$BATTY_SUPPRESS_NO_BALL_DEATH" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_VEL" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_VEL=%s\r\n' "$$BATTY_REPLAY_BALL_VEL" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_ENEMY_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_ENEMY_OBJECT=%s\r\n' "$$BATTY_REPLAY_ENEMY_OBJECT" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_ROCKET_ACTIVE" ]; then \
	    printf 'SET BATTY_REPLAY_ROCKET_ACTIVE=%s\r\n' "$$BATTY_REPLAY_ROCKET_ACTIVE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BONUS" ]; then \
	    printf 'SET BATTY_REPLAY_BONUS=%s\r\n' "$$BATTY_REPLAY_BONUS" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BOMB" ]; then \
	    printf 'SET BATTY_REPLAY_BOMB=%s\r\n' "$$BATTY_REPLAY_BOMB" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_PTS400" ]; then \
	    printf 'SET BATTY_REPLAY_PTS400=%s\r\n' "$$BATTY_REPLAY_PTS400" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BULLET" ]; then \
	    printf 'SET BATTY_REPLAY_BULLET=%s\r\n' "$$BATTY_REPLAY_BULLET" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_AUTO_FIRE" ]; then \
	    printf 'SET BATTY_AUTO_FIRE=%s\r\n' "$$BATTY_AUTO_FIRE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FORCE_SPAWN_BONUS" ]; then \
	    printf 'SET BATTY_FORCE_SPAWN_BONUS=%s\r\n' "$$BATTY_FORCE_SPAWN_BONUS" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BLAST" ]; then \
	    printf 'SET BATTY_REPLAY_BLAST=%s\r\n' "$$BATTY_REPLAY_BLAST" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FORCE_BRICK" ]; then \
	    printf 'SET BATTY_FORCE_BRICK=%s\r\n' "$$BATTY_FORCE_BRICK" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_RAMP" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_RAMP=%s\r\n' "$$BATTY_REPLAY_BALL_RAMP" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_MULTIBALL" ]; then \
	    printf 'SET BATTY_REPLAY_MULTIBALL=%s\r\n' "$$BATTY_REPLAY_MULTIBALL" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BIGBALL" ]; then \
	    printf 'SET BATTY_REPLAY_BIGBALL=%s\r\n' "$$BATTY_REPLAY_BIGBALL" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_WAIT_KEY" ]; then \
	    printf 'SET BATTY_REPLAY_WAIT_KEY=%s\r\n' "$$BATTY_REPLAY_WAIT_KEY" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_LAUNCH_FRAMES" ]; then \
	    printf 'SET BATTY_LAUNCH_FRAMES=%s\r\n' "$$BATTY_LAUNCH_FRAMES" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FRAME_PROBE" ]; then \
	    printf 'SET BATTY_FRAME_PROBE=%s\r\n' "$$BATTY_FRAME_PROBE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_VISUAL_PROBE_FRAMES" ]; then \
	    printf 'SET BATTY_VISUAL_PROBE_FRAMES=%s\r\n' "$$BATTY_VISUAL_PROBE_FRAMES" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_LAFFC" ]; then \
	    printf 'SET BATTY_LAFFC=%s\r\n' "$$BATTY_LAFFC" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_KINNOCK" ]; then \
	    printf 'SET BATTY_KINNOCK=%s\r\n' "$$BATTY_KINNOCK" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_GAME_MODE" ]; then \
	    printf 'SET BATTY_GAME_MODE=%s\r\n' "$$BATTY_GAME_MODE" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FAST_HOLDS" ]; then \
	    printf 'SET BATTY_FAST_HOLDS=%s\r\n' "$$BATTY_FAST_HOLDS" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_LIVES_2UP" ]; then \
	    printf 'SET BATTY_REPLAY_LIVES_2UP=%s\r\n' "$$BATTY_REPLAY_LIVES_2UP" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_LEGACY_COLLISION" ]; then \
	    printf 'SET BATTY_LEGACY_COLLISION=%s\r\n' "$$BATTY_LEGACY_COLLISION" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_HOLD_ROUND_BANNER" ]; then \
	    printf 'SET BATTY_HOLD_ROUND_BANNER=%s\r\n' "$$BATTY_HOLD_ROUND_BANNER" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_HOLD_ROCKET_CLEAR" ]; then \
	    printf 'SET BATTY_HOLD_ROCKET_CLEAR=%s\r\n' "$$BATTY_HOLD_ROCKET_CLEAR" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FORCE_BAT_FULL_REDRAW" ]; then \
	    printf 'SET BATTY_FORCE_BAT_FULL_REDRAW=%s\r\n' "$$BATTY_FORCE_BAT_FULL_REDRAW" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FORCE_BALL_FULL_REDRAW" ]; then \
	    printf 'SET BATTY_FORCE_BALL_FULL_REDRAW=%s\r\n' "$$BATTY_FORCE_BALL_FULL_REDRAW" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_FORCE_FULL_FLUSH_EACH_FRAME" ]; then \
	    printf 'SET BATTY_FORCE_FULL_FLUSH_EACH_FRAME=%s\r\n' "$$BATTY_FORCE_FULL_FLUSH_EACH_FRAME" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_MAGNET" ]; then \
	    printf 'SET BATTY_REPLAY_MAGNET=%s\r\n' "$$BATTY_REPLAY_MAGNET" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_TEST_KEY_BEFORE_ANIM" ]; then \
	    printf 'SET BATTY_TEST_KEY_BEFORE_ANIM=%s\r\n' "$$BATTY_TEST_KEY_BEFORE_ANIM" >> $(AUTOEXEC_T) ; \
	fi; \
	if [ -n "$$BATTY_SERIAL_PROBE" ]; then \
	    printf 'SET BATTY_SERIAL_PROBE=%s\r\n' "$$BATTY_SERIAL_PROBE" >> $(AUTOEXEC_T) ; \
	fi; \
	printf 'BATTY\r\n' >> $(AUTOEXEC_T)
	mcopy -i $@ -o $(AUTOEXEC_T) ::AUTOEXEC.BAT
	@echo "Test floppy ready: $@  (full 4-state cycle)"

run:
	rm -f $(FLOPPY_OUT)
	$(MAKE) $(FLOPPY_OUT)
	bash scripts/run.sh $(FLOPPY_OUT)

run-86box:
	rm -f $(FLOPPY_OUT)
	$(MAKE) $(FLOPPY_OUT)
	BOX86_BIN="$(BOX86_BIN)" \
	BOX86_VM_DIR="$(BOX86_VM_DIR)" \
	BOX86_MACHINE="$(BOX86_MACHINE)" \
	BOX86_GFXCARD="$(BOX86_GFXCARD)" \
	BOX86_FDD_TYPE="$(BOX86_FDD_TYPE)" \
	BOX86_ASSETPATH="$(BOX86_ASSETPATH)" \
	BOX86_ROMPATH="$(BOX86_ROMPATH)" \
	bash scripts/run_86box.sh $(FLOPPY_OUT)

profile-auto:
	rm -f $(FLOPPY_OUT)
	BATTY_RENDER_PROFILE=1 BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES) BATTY_START_LEVEL=1 BATTY_LEVEL=$(PROFILE_LEVEL) BATTY_REPLAY_BALL_OBJECT=$(PROFILE_BALL_OBJECT) BATTY_REPLAY_BALL_STUCK=$(PROFILE_BALL_STUCK) $(MAKE) $(FLOPPY_OUT)
	python3 scripts/run_profile_auto.py --floppy $(FLOPPY_OUT) --seconds $(PROFILE_WAIT)
	$(MAKE) read-profile
	python3 scripts/analyze_profile.py build/PROFILE.TXT --json build/profile-summary.json --min-frames $(PROFILE_FRAMES)

# Brick-destruction profile: laser bat + auto-fire so bullets continuously
# destroy bricks, exercising + measuring the incremental brick-band rebuild.
# A/B: `make profile-bricks` (scoped) vs `make profile-bricks FULL_BAND=1`
# (whole-band baseline). Compare the "band rebuild PIT" / "band rows
# rebuilt" lines in build/PROFILE.TXT.
profile-bricks:
	rm -f $(FLOPPY_OUT)
	BATTY_RENDER_PROFILE=1 BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES) BATTY_START_LEVEL=1 BATTY_LEVEL=$(PROFILE_LEVEL) \
	    BATTY_REPLAY_BAT_OBJECT=$(PROFILE_BAT_LASER) BATTY_AUTO_FIRE=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 \
	    BATTY_REPLAY_BALL_OBJECT=$(PROFILE_BALL_OBJECT) BATTY_REPLAY_BALL_STUCK=$(PROFILE_BALL_STUCK) \
	    BATTY_FULL_BAND_REBUILD=$(FULL_BAND) $(MAKE) $(FLOPPY_OUT)
	python3 scripts/run_profile_auto.py --floppy $(FLOPPY_OUT) --seconds $(PROFILE_WAIT)
	$(MAKE) read-profile

# Normal-play brick-hit profile: a ball bounces through the brick band
# (no laser), so brick-destruction redraw cost is measured without the
# bullet/auto-fire noise of profile-bricks. Watch "full dynamic frames" /
# "ball block bricks" / "band rebuilds".
profile-ballbricks:
	rm -f $(FLOPPY_OUT)
	BATTY_RENDER_PROFILE=1 BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES) BATTY_START_LEVEL=1 BATTY_LEVEL=$(PROFILE_LEVEL) \
	    BATTY_REPLAY_BALL_OBJECT=$(PROFILE_BALL_FIELD) BATTY_REPLAY_BALL_STUCK=0 \
	    BATTY_SUPPRESS_NO_BALL_DEATH=1 $(MAKE) $(FLOPPY_OUT)
	python3 scripts/run_profile_auto.py --floppy $(FLOPPY_OUT) --seconds $(PROFILE_WAIT)
	$(MAKE) read-profile

# Multi-ball profile: two extra balls in play (BATTY_REPLAY_MULTIBALL), to
# measure the extra-ball dirty tier. A/B with FULL_BAND-style: compare to
# the same scenario where extra balls force full-dynamic (git-revert era) —
# here just confirms the extra balls take the ball-object tier, not full.
profile-multiball:
	rm -f $(FLOPPY_OUT)
	BATTY_RENDER_PROFILE=1 BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES) BATTY_START_LEVEL=1 BATTY_LEVEL=$(PROFILE_LEVEL) \
	    BATTY_REPLAY_BALL_OBJECT=$(PROFILE_BALL_OBJECT) BATTY_REPLAY_BALL_STUCK=0 \
	    BATTY_REPLAY_MULTIBALL=1 BATTY_SUPPRESS_NO_BALL_DEATH=1 $(MAKE) $(FLOPPY_OUT)
	python3 scripts/run_profile_auto.py --floppy $(FLOPPY_OUT) --seconds $(PROFILE_WAIT)
	$(MAKE) read-profile

profile-86box:
	rm -f $(FLOPPY_OUT)
	BATTY_RENDER_PROFILE=1 $(MAKE) $(FLOPPY_OUT)
	BOX86_BIN="$(BOX86_BIN)" \
	BOX86_VM_DIR="$(BOX86_VM_DIR)" \
	BOX86_MACHINE="$(BOX86_MACHINE)" \
	BOX86_GFXCARD="$(BOX86_GFXCARD)" \
	BOX86_FDD_TYPE="$(BOX86_FDD_TYPE)" \
	BOX86_ASSETPATH="$(BOX86_ASSETPATH)" \
	BOX86_ROMPATH="$(BOX86_ROMPATH)" \
	bash scripts/run_86box.sh $(FLOPPY_OUT)

read-profile:
	@mkdir -p build
	@if ! timeout 5s mtype -i $(FLOPPY_OUT) ::PROFILE.TXT > build/PROFILE.TXT; then \
	    echo "PROFILE.TXT not found on $(FLOPPY_OUT). Exit the game cleanly after a profile run, then retry."; \
	    exit 1; \
	fi
	@cat build/PROFILE.TXT
	@python3 scripts/analyze_profile.py build/PROFILE.TXT

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

test:
	@# Force a TEST_FLOPPY_OUT rebuild so AUTOEXEC.BAT picks up the current
	@# BATTY_LEVEL env var (the floppy itself doesn't change with env so
	@# the implicit rule would otherwise consider it up-to-date).
	@rm -f $(TEST_FLOPPY_OUT)
	@$(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/test_visual.py --floppy $(TEST_FLOPPY_OUT)

# One entry point for the gameplay frame-parity regression: the static
# 5-checkpoint + per-level visual test, plus the byte-exact LAFFC collision
# gate (ball object matches the Spectrum over L3's 150-frame trajectory).
# Both green => the shipped parity (motion + collision) is intact. The
# cosmetic metal-brick shimmer is out of scope (see notes/laffc-decode.md).
# Fast routine regression core: the byte-exact gameplay-math gates
# (visual states + lints, ball/collision, bat/deflection, enemy, RNG).
# Run this per-change. For the COMPREHENSIVE coverage (every feature gate,
# incl. the rocket-clear / sparks / brick-flash / redraw guards), use
# `parity-check-full` below.
parity-check:
	@# Every host suite and source gate, by DELEGATION rather than by a
	@# second copy of the list. The two lists drifted once already:
	@# test_replay was added here and not to test-fast, test_replay_parse
	@# to test-fast and not here, and each looked complete on its own.
	@# check_host_tests_wired.py guards test-fast's list; this makes it
	@# the only list there is.
	$(MAKE) test-fast
	$(MAKE) test
	$(MAKE) test-laffc-ball-frame1
	$(MAKE) test-bat-deflection
	$(MAKE) test-enemy-descend
	$(MAKE) test-rng-walk
	$(MAKE) test-enemy-steer
	$(MAKE) test-ball-no-tunnel
	$(MAKE) test-enemy-attr-parity

# Parallel runner for the QEMU-only gates (the whole parity-check fast core,
# plus the parity-check-full feature gates with --full). Each gate runs on
# its own floppy image (BATTY_TEST_FLOPPY) so they execute concurrently —
# same gates, same assertions, ~Jx faster. ZEsarUX gates (frame-step /
# replay) are excluded (single ZRCP port) — run those via parity-check-full.
#   make parity-check-parallel J=8        # fast core, 8-wide
#   make parity-check-parallel J=8 FULL=1 # + feature gates
J ?= 8
parity-check-parallel:
	python3 scripts/run_gates_parallel.py --jobs $(J) $(if $(FULL),--full,)

# Comprehensive regression suite: the fast core PLUS every standalone
# feature/render gate that previously was NOT wired into any aggregate
# (so a routine run actually exercises the rocket-clear tally, death
# sparks, brick-flash render, midgame brick replay, ball launch, the
# multi-level LAFFC sanity sweep, and the dirty-redraw correctness
# guards). Slower (many QEMU boots) — run before milestones / merges.
parity-check-full:
	$(MAKE) parity-check
	$(MAKE) test-ball-no-tunnel FULL=1
	$(MAKE) test-ball-paths-no-tunnel
	$(MAKE) test-sprite-attr-parity
	$(MAKE) test-laffc-ball-l5-metal
	$(MAKE) test-gameplay-soak
	$(MAKE) test-frame-step
	$(MAKE) replay-l3-entry
	$(MAKE) test-wall-bounce
	$(MAKE) test-magnet-ball
	$(MAKE) test-brik-anim-pace
	$(MAKE) test-levels-sweep
	$(MAKE) test-enemy-flyover-redraw
	$(MAKE) test-normal-ball-launch
	$(MAKE) test-laffc-levels-sane
	$(MAKE) test-hud
	$(MAKE) test-round-banner-border
	$(MAKE) test-brick-flash
	$(MAKE) test-death-sparks
	$(MAKE) test-rocket-bonus
	$(MAKE) test-game-over
	$(MAKE) test-stuck-ball-offset
	$(MAKE) test-invariant-owners
	$(MAKE) test-bonus-fall
	$(MAKE) test-bomb-fall
	$(MAKE) test-pts400-fall
	$(MAKE) test-bullet-fly
	$(MAKE) test-laser-cadence
	$(MAKE) test-enemy-anim
	$(MAKE) test-bonus-drop
	$(MAKE) test-bonus-effects
	$(MAKE) test-bonus-effects2
	$(MAKE) test-bonus-typepick
	$(MAKE) test-bullet-blast
	$(MAKE) test-brick-scoring
	$(MAKE) test-ball-speed-ramp
	$(MAKE) test-rocket-completion-no-ball
	$(MAKE) test-rocket-flight-redraw
	$(MAKE) test-midgame-brick-replay
	$(MAKE) test-ball-dirty-redraw
	$(MAKE) test-ball-object-dirty-redraw
	$(MAKE) test-bullet-dirty-redraw
	$(MAKE) test-bomb-dirty-redraw
	$(MAKE) test-blast-dirty-redraw
	$(MAKE) test-visual-checkpoints
	$(MAKE) test-bat-fire-dirty-redraw
	$(MAKE) test-multiball-dirty-redraw
	$(MAKE) test-bigball-dirty-redraw
	$(MAKE) test-stuck-ball-dirty-redraw
	$(MAKE) test-enemy-brick-residue
	$(MAKE) test-enemy-brick-walk
	$(MAKE) test-enemy-margin-clamp
	$(MAKE) test-two-player-turn
	$(MAKE) test-double-play-court
	$(MAKE) test-double-play-bat2
	$(MAKE) test-bat-redraw-window
	$(MAKE) test-ball-left-wall-escape
	$(MAKE) test-l3-replay-seed

# Deterministic mid-game frame-timeline capture (port side of the
# frame-step parity sweep). Builds the test floppy starting directly in
# gameplay with the multi-checkpoint visual probe, then drives the port
# through each checkpoint and screendumps a byte-deterministic frame at
# each. FRAMES is a comma-separated ascending list; LEVEL picks the cycle.
FRAMES ?= 30,60,90
LEVEL  ?= 1
capture-timeline:
	@rm -f $(TEST_FLOPPY_OUT)
	@BATTY_START_LEVEL=1 BATTY_LEVEL=$(LEVEL) BATTY_VISUAL_PROBE_FRAMES=$(FRAMES) \
	    $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/capture_frame_timeline.py --floppy $(TEST_FLOPPY_OUT) --frames $(FRAMES)

# Original (ZEsarUX) side of the frame-step sweep: frame-step the Z80 the
# same checkpoint frame counts and dump each .scr, so the two timelines
# diff frame-for-frame. SNAPSHOT is a tracked mid-game .sna.
SNAPSHOT ?= build/snapshots/20260513T202101Z.sna
SETUP_REPLAY ?= replays/l3-brick-flash.json
TL_FRAMES_ORIG ?= 0,5,10
capture-timeline-original: $(ZESARUX)
	python3 scripts/capture_frame_timeline_original.py \
	    --snapshot $(SNAPSHOT) --frames $(TL_FRAMES_ORIG) --zesarux $(ZESARUX) \
	    --setup-from-replay $(SETUP_REPLAY) --require-motion

# Frame-step parity gate: capture the port and original L3 timelines on
# the SAME checkpoint frames and diff them frame-for-frame in the
# brick-play ROI. The port is seeded to the original's probed $BA83
# descriptors (same overrides as replay-l3-brick-flash). NOTE: this is
# not yet a 0px gate — the start frame is not byte-aligned and the metal-
# brick shimmer runs out of phase; see notes/replay-harness.md. The
# numbers localize where the two simulations diverge.
TL_FRAMES ?= 1,3,5
TL_CMP_FRAMES ?= 0,1,3,5
TL_ROI    ?= 8,32,248,128
TL_MAXDIFF ?= 0
# No enemy seed: the l3-brick-flash original DOES have an active enemy
# (sprite_set=$09 anim_bird at $9B96 — GT-confirmed 2026-06-05; it descends
# and steers, so it is genuinely active, NOT the inactive descriptor the
# earlier version of this comment claimed). But it is a FRESH alien
# descending from the top (x=168, y=1 -> ~28 over frames 0..28) and so stays
# ABOVE this TL_ROI (y=32..160) through the compared frames 0..5. The port
# had been seeded with a DIFFERENT, mid-flight enemy (x=164, y=27) that
# drops INTO the ROI by frame 1 while the original's is still above it — a
# position-mismatch confounder. Dropping the port seed removed it: frame-1
# ROI diff falls 363 -> 212 px, collapsing to a compact box around the ball.
# (Proper alignment for an enemy-specific gate: seed the port with the SAME
# fresh y=1 descriptor; see notes/enemy-movement.md.)
# RNG seed: the snapshot's actual $BA83 f0 values — random_number D:E=3793
# ($8D48/$8D49 bytes 93,37) and random_seed (LE) = 962A. With the per-frame
# tick now default, the OLD stale 8E49 (which wrote the wrong address $8E17,
# leaving the port's RNG un-seeded) made the port drop a SPURIOUS SLOW bonus
# the original lacks (the stale RNG passes the 5/16 drop gate on the L3
# brick destruction). The correct seed reproduces the original's RNG walk
# (make test-rng-walk) so the port drops no bonus here, matching the GT.
L3_SEED_ENV = BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A \
	BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 \
	BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C \
	BATTY_REPLAY_BALL_STUCK=0
# WAIT_KEY pauses the port at main-loop entry; --wait-key captures that
# as frame 0, byte-aligned with the original's post-setup $BA83.
# LAFFC_FLAG=1 measures the byte-exact LAFFC collision path (the one being
# driven to parity); empty (default) measures the shipping brick_collision.
LAFFC_FLAG ?=
capture-timeline-both: $(ZESARUX)
	@rm -f $(TEST_FLOPPY_OUT)
	@$(L3_SEED_ENV) BATTY_REPLAY_WAIT_KEY=1 BATTY_LAFFC=$(LAFFC_FLAG) \
	    BATTY_VISUAL_PROBE_FRAMES=$(TL_FRAMES) $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/capture_frame_timeline.py --floppy $(TEST_FLOPPY_OUT) \
	    --frames $(TL_FRAMES) --wait-key --out build/tl_port
	python3 scripts/capture_frame_timeline_original.py --snapshot $(SNAPSHOT) \
	    --frames $(TL_CMP_FRAMES) --zesarux $(ZESARUX) \
	    --setup-from-replay $(SETUP_REPLAY) --out build/tl_orig
	python3 scripts/compare_timelines.py --port build/tl_port \
	    --original build/tl_orig --frames $(TL_CMP_FRAMES) --roi $(TL_ROI) \
	    --max-diff $(TL_MAXDIFF)

# Longer-horizon validation of the byte-exact LAFFC collision path: the
# ball must stay in lockstep with the Spectrum for 40 frames (residual
# stays bounded to the brick-hit shimmer, ~hundreds of px, never the
# thousands that a lost/diverged ball would produce).
gate-laffc-long:
	$(MAKE) capture-timeline-both LAFFC_FLAG=1 \
	    TL_FRAMES=1,5,10,20,40 TL_CMP_FRAMES=0,1,5,10,20,40

# Frame-step parity GATE: capture-timeline-both pinned to the documented
# L3 residual floor (notes/metal-shimmer.md): f0/f1/f2/f4 = 0 px exact,
# f3/f5 <= 4 px and f6 <= 1 px (the capture-phase transient at the brick-
# destruction moment -- PORT f3 == ORIG f5 byte-for-byte). ANY brick-band /
# dirty-redraw / flash render change that drifts the destroy transient
# fails here (this is the gate that would have caught the BRICK_FLASH_TICKS
# 2->1 regression, 4px -> 88-134px, which the headless suites missed).
# Needs ZEsarUX (built from tools/zesarux); ~25s.
TL_GATE_FRAMES     = 1,2,3,4,5,6
TL_GATE_CMP_FRAMES = 0,1,2,3,4,5,6
TL_GATE_BUDGETS    = 0,0,0,4,0,4,1
test-frame-step: $(ZESARUX)
	@rm -f $(TEST_FLOPPY_OUT)
	@$(L3_SEED_ENV) BATTY_REPLAY_WAIT_KEY=1 \
	    BATTY_VISUAL_PROBE_FRAMES=$(TL_GATE_FRAMES) $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/capture_frame_timeline.py --floppy $(TEST_FLOPPY_OUT) \
	    --frames $(TL_GATE_FRAMES) --wait-key --out build/tl_port
	python3 scripts/capture_frame_timeline_original.py --snapshot $(SNAPSHOT) \
	    --frames $(TL_GATE_CMP_FRAMES) --zesarux $(ZESARUX) \
	    --setup-from-replay $(SETUP_REPLAY) --out build/tl_orig
	python3 scripts/compare_timelines.py --port build/tl_port \
	    --original build/tl_orig --frames $(TL_GATE_CMP_FRAMES) \
	    --roi $(TL_ROI) --budgets $(TL_GATE_BUDGETS)

# Headless regression locking in the byte-exact LAFFC ball state at L3
# frame 1 (no ZEsarUX needed; asserts object_ball_1 == the Spectrum probe).
test-laffc-ball-frame1:
	python3 scripts/test_laffc_ball_frame1.py

# Byte-exact ball-vs-brick parity on a NON-L3 level (broadens the oracle):
# the L5 row-0 boundary-metal bounce (the #6 scenario) must match the
# original's trajectory (captured via replays/l5-metal-ball.json). Locks the
# #6 LAFFC edge-mask fix against the real game, where L3 coverage couldn't.
test-laffc-ball-l5-metal:
	python3 scripts/test_laffc_ball_l5_metal.py

# Wall-bounce gate: the ball must reflect off the L/R side walls (and top)
# via change_direction's $1F/$3F masks, not pin against them. Guards the
# bounce_wall port in reflect_obj_dir (a swapped/off-by-one reflect pinned
# the ball at x=8 / x=240, juggling -- notes/lessons.md, notes/wall-bounce.md).
test-wall-bounce:
	python3 scripts/test_wall_bounce.py

# Ball-vs-brick collision invariant sweep (known-bugs.md #6 class): seeds a
# ball one step from a solid brick across levels x approaches x speeds and
# asserts it never passes THROUGH a still-solid brick unhit (must destroy /
# half-hit / bounce). ZEsarUX-free. NO_TUNNEL_ARGS overrides the matrix;
# FULL=1 runs all 15 levels + diagonals (parity-check-full).
# Default subset includes L5/L7 (row-0 metal bricks against the top
# boundary — the #6 repro) so the fast gate covers the boundary-face class.
NO_TUNNEL_ARGS ?= --levels 1,5,7 --speeds 6 --approaches up,down,up-l,up-r,down-l,down-r --max-cols 1
test-ball-no-tunnel:
ifdef FULL
	python3 scripts/test_ball_brick_collision.py --full
else
	python3 scripts/test_ball_brick_collision.py $(NO_TUNNEL_ARGS)
endif

# Enemy/sprite attribute-parity gate (known-bugs.md #7): a flying enemy must
# NOT recolour its cells -- the original blits pixels only (print_obj_to_buff),
# so the sprite shows ZX colour-clash in the underlying brick/bg attr. Asserts
# attr_buff == bg_attr_buff under a bird seeded over L3 bricks. ZEsarUX-free.
test-enemy-attr-parity:
	python3 scripts/test_enemy_attr_parity.py

# No-tunnel coverage for the non-primary collision paths (known-bugs.md #6
# secondary leads): step_extra_ball (multiball) + magnet_captured_move. Both
# call the same LAFFC the #6 edge-mask fix lives in; this guards that an
# extra/captured ball's centre never sits inside a solid brick.
test-ball-paths-no-tunnel:
	python3 scripts/test_ball_paths_no_tunnel.py

# Generalized sprite attribute-parity (known-bugs.md #7 class for ALL moving
# sprites): a falling bonus / enemy bomb / laser bullet seeded over the L3
# brick band must leave every cell attr unchanged (attr_buff == bg_attr_buff)
# -- moving objects blit pixels only, never recolour (print_obj_to_buff).
test-sprite-attr-parity:
	python3 scripts/test_sprite_attr_parity.py

# Long-run gameplay invariant soak (D4): sustained in-flight play across
# several levels (L1/L3/L5/L9) must never break the rules at any sampled
# checkpoint — ball never inside a solid brick, never escapes the walls,
# brick count only falls, score only rises. Catches over-time regressions a
# single-frame gate can't. Uses the deterministic serial frame wait.
test-gameplay-soak:
	python3 scripts/test_gameplay_soak.py

# Magnet-ball gate: an ON magnet must curve the ball (+-1/64 dir per
# frame while overlapping the 15x14 box, quantized release), an OFF
# magnet must not (handling_ball LA27E_0..11 -- known-bugs.md #5).
test-magnet-ball:
	python3 scripts/test_magnet_ball.py

# Intro-shimmer pace gate: the pre-round all-metal-bricks animation must
# run 8 frames x 2 full PIT edges (all_metal_briks_animation_snd $B765)
# and must NOT consume/abort on buffered input (known-bugs.md #4).
test-brik-anim-pace:
	python3 scripts/test_brik_anim_pace.py

# FAIL-gated state4 sweep across ALL 15 levels (make test gates L1 only;
# the rest were INFO-only, which hid the L3/L9 alien-race artefact --
# notes/per-level-profile.md 2026-06-11). Slow: 15 QEMU boots.
test-levels-sweep:
	python3 scripts/test_levels_sweep.py

# Enemy fly-over + bomb-drop A/B gate: the dirty and full compose paths
# must render an alien with a freshly dropped (still overlapping) bomb
# identically -- both follow the original's $9AD0 slot-paint order
# (balls < bullets < bomb/bonus/pts400 < enemy < rocket). Caught the
# draw-order divergence + the top-frame-centre erase
# (notes/bird-render-parity.md).
test-enemy-flyover-redraw:
	python3 scripts/test_enemy_flyover_redraw.py

# Bat-deflection gate: the LAB1F port in step_ball must reproduce the
# Spectrum's deflected direction across bat positions (ground truth in
# notes/bat-deflection.md, captured by scripts/capture_bat_deflection.py).
test-bat-deflection:
	python3 scripts/test_bat_deflection_port.py

# Enemy descend-phase gate: handling_bird must slide the fresh L3 alien
# down 1 px/frame (x/dir/spd/target held) like the original. RNG-
# independent ground truth in notes/enemy-movement.md (captured by
# scripts/capture_enemy_flight.py).
test-enemy-descend:
	python3 scripts/test_enemy_descend.py

# Byte-exact RNG-walk gate: the per-frame tick (flag ON) must reproduce
# the original's random_number walk from the L3 f0 seed. Ground truth in
# notes/rng-model.md.
test-rng-walk:
	python3 scripts/test_rng_walk.py

# Enemy STEERING gate (RNG-dependent): with the byte-correct L3 seed +
# the per-frame tick, handling_bird must steer the alien like the original
# (dir 0x11->0x12->0x13 over f16/f20/f24). Guards the RNG-walk + steering.
test-enemy-steer:
	python3 scripts/test_enemy_steer.py

# Multi-level sanity sweep: LAFFC must play comparably to brick_collision
# on every level (no level where LAFFC destroys ~no bricks while
# brick_collision plays). Must pass before flipping the default to LAFFC.
SANE_LEVELS ?= 1,5,10,15
SANE_FRAMES ?= 500
test-laffc-levels-sane:
	python3 scripts/test_laffc_levels_sane.py --levels $(SANE_LEVELS) --frames $(SANE_FRAMES)

# --- Video-engine tests (host-native; no DOS, no emulator) --------------
# Compiles src/zxvga.cpp with the host compiler — the __WATCOMC__ guard just
# points `vga` at an array — so these exercise the SHIPPING expansion table
# and blit logic directly.
# Milliseconds, so the clash model can be checked exhaustively (every attr
# x every byte) rather than sampled through a 10 s QEMU boot.
HOSTCXX      ?= c++
HOSTCXXFLAGS ?= -std=c++98 -O1 -Wall -Wextra -Werror -Wno-unused-function
VIDEO_TEST     = build/test_zxvga
VIDEO_TEST_SRC = tests/test_zxvga.cpp src/zxvga.cpp src/zxvga.h src/types.h

$(VIDEO_TEST): $(VIDEO_TEST_SRC) | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_zxvga.cpp

test-video: $(VIDEO_TEST)
	./$(VIDEO_TEST)

PHYSICS_TEST = build/test_physics

$(PHYSICS_TEST): tests/test_physics.cpp src/physics.cpp src/physics.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_physics.cpp

test-physics: $(PHYSICS_TEST)
	./$(PHYSICS_TEST)

REPLAY_PARSE_TEST = build/test_replay_parse

$(REPLAY_PARSE_TEST): tests/test_replay_parse.cpp src/replay_parse.cpp src/replay_parse.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_replay_parse.cpp

test-replay-parse: $(REPLAY_PARSE_TEST)
	./$(REPLAY_PARSE_TEST)

REPLAY_TEST = build/test_replay

# Links replay.cpp with the modules it seeds, since these tests assert on
# what landed in objects[] / the bullet arrays / the RNG, not on a return
# value. That IS the module's contract.
$(REPLAY_TEST): tests/test_replay.cpp src/replay.cpp src/replay.h \
                src/replay_parse.cpp src/objects.cpp src/weapons.cpp \
                src/physics.cpp src/rng.cpp src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_replay.cpp \
	    src/replay_parse.cpp src/objects.cpp src/weapons.cpp \
	    src/physics.cpp src/rng.cpp

test-replay: $(REPLAY_TEST)
	@$(REPLAY_TEST)

SCORING_TEST = build/test_scoring

$(SCORING_TEST): tests/test_scoring.cpp src/scoring.cpp src/scoring.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_scoring.cpp

test-scoring: $(SCORING_TEST)
	./$(SCORING_TEST)

BONUS_TEST = build/test_bonus_codes

$(BONUS_TEST): tests/test_bonus_codes.cpp src/bonus_codes.cpp src/bonus_codes.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_bonus_codes.cpp

test-bonus-codes: $(BONUS_TEST)
	./$(BONUS_TEST)

ENEMIES_TEST = build/test_enemies

$(ENEMIES_TEST): tests/test_enemies.cpp src/enemies.cpp src/enemies.h src/objects.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_enemies.cpp

test-enemies: $(ENEMIES_TEST)
	./$(ENEMIES_TEST)

WEAPONS_TEST = build/test_weapons

# Links physics because the bomb's fall goes through motion_accel_step.
$(WEAPONS_TEST): tests/test_weapons.cpp src/weapons.cpp src/weapons.h src/objects.h src/level.h src/physics.cpp src/physics.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_weapons.cpp src/physics.cpp

test-weapons: $(WEAPONS_TEST)
	./$(WEAPONS_TEST)

OBJECTS_TEST = build/test_objects

$(OBJECTS_TEST): tests/test_objects.cpp src/objects.cpp src/objects.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_objects.cpp

test-objects: $(OBJECTS_TEST)
	./$(OBJECTS_TEST)

HUD_TEST = build/test_hud_unit

$(HUD_TEST): tests/test_hud.cpp src/hud.cpp src/hud.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_hud.cpp

test-hud-unit: $(HUD_TEST)
	./$(HUD_TEST)

SOUND_TEST = build/test_sound

$(SOUND_TEST): tests/test_sound.cpp src/sound.cpp src/sound.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_sound.cpp

test-sound: $(SOUND_TEST)
	./$(SOUND_TEST)

BRICKS_TEST = build/test_bricks

$(BRICKS_TEST): tests/test_bricks.cpp src/bricks.cpp src/bricks.h src/level.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_bricks.cpp

# The golden half needs assets/levels.bin and build/level_gt/*.scr.
test-bricks: $(BRICKS_TEST) assets/levels.bin
	./$(BRICKS_TEST)

ASSETS_TEST = build/test_assets

$(ASSETS_TEST): tests/test_assets.cpp src/assets.cpp src/assets.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_assets.cpp

test-assets: $(ASSETS_TEST)
	./$(ASSETS_TEST)

RNG_TEST = build/test_rng

$(RNG_TEST): tests/test_rng.cpp src/rng.cpp src/rng.h src/types.h | build
	$(HOSTCXX) $(HOSTCXXFLAGS) -o $@ tests/test_rng.cpp

# Needs assets/random_seed.bin -- the walk is a function of those bytes.
test-rng: $(RNG_TEST) assets/random_seed.bin
	./$(RNG_TEST)

# The emulator-free source gates. CI runs these; so must any local check,
# otherwise a gate that greps for code can go stale when the code moves
# and nothing local notices.
# Do the gates' source assertions still match the source? Seconds, and it
# catches a rename that would otherwise fail a gate for a reason unrelated
# to what the gate guards.
test-gate-greps:
	python3 scripts/check_gate_greps.py

# Does the plan's status block still state true numbers? The line count
# and gate count there went stale three times; one of those times it was
# quoting the compiler's line count instead of wc -l.
test-notes-numbers:
	python3 scripts/check_notes_numbers.py

# known-bugs #13: an extra ball must not be able to write the primary
# ball's sign cache. Structural, because no pixel gate reaches the
# scenario that makes it observable.
test-ball-sign-cache-owner:
	python3 scripts/test_ball_sign_cache_owner.py

# Does every BATTY_* knob src reads actually reach DOS on the test
# floppy? A missing SET line does not error — the gate just runs a
# different scenario. This cost a run when BATTY_REPLAY_LIVES was added.
test-env-passthrough:
	python3 scripts/check_env_passthrough.py

# known-bugs #15: bios_ticks() does not advance during gameplay. Reading
# it is fine (the TIMED_OUT screens act on nothing); computing with it is
# a live dependency on a frozen clock.
test-frozen-clock:
	python3 scripts/check_frozen_clock.py

# A module that declares state must define it. objects[] was extern in
# objects.h but defined in main.cpp for eleven stages, which is only
# visible once something links the module without main.cpp.
test-module-ownership:
	python3 scripts/check_module_ownership.py

# Does every tests/test_*.cpp actually run under test-fast?
# test_replay_parse.cpp had 7 tests and a working target and was reachable
# only from parity-check, so it ran once every six minutes, not seconds.
test-host-tests-wired:
	python3 scripts/check_host_tests_wired.py

# Is every gate named in notes/testing.md's index? Before that index
# existed, 30 of 59 gates were mentioned nowhere in it.
test-gate-index:
	python3 scripts/check_gate_index.py

# Does every file path cited in a comment or note actually exist?
# Three had rotted, including one note that was never written.
test-doc-links:
	python3 scripts/check_doc_links.py

# Each debug switch's documented default vs the initialiser. The RNG
# model's comment said "OFF by default" for two months after it flipped.
test-switch-defaults:
	python3 scripts/check_switch_defaults.py

# No gate may be satisfied by a PREVIOUS run's output — stale captures
# or a stale PROBE.TXT. test-visual-checkpoints was.
test-gate-freshness:
	python3 scripts/check_gate_freshness.py

# known-bugs #3: the metal shimmer plays one pass, not a loop. Mutating
# the wrap leaves all 59 QEMU gates green, so this is the only guard.
test-shimmer-one-pass:
	python3 scripts/test_shimmer_one_pass.py

# known-bugs #14: the multiball spawn must read the primary's dir byte.
# The full QEMU suite passed both before and after that fix.
test-multiball-source:
	python3 scripts/check_multiball_source.py

test-source-gates:
	$(MAKE) test-gate-greps
	$(MAKE) test-multiball-source
	$(MAKE) test-shimmer-one-pass
	$(MAKE) test-gate-freshness
	$(MAKE) test-switch-defaults
	$(MAKE) test-host-tests-wired
	$(MAKE) test-gate-index
	$(MAKE) test-doc-links
	$(MAKE) test-notes-numbers
	$(MAKE) test-ball-sign-cache-owner
	$(MAKE) test-env-passthrough
	$(MAKE) test-frozen-clock
	$(MAKE) test-module-ownership
	$(MAKE) test-l3-replay-seed
	$(MAKE) test-death-sparks
	$(MAKE) test-rocket-bonus
	$(MAKE) test-menu-start
	$(MAKE) test-kinnock
	$(MAKE) test-level-attrs-derivable
	$(MAKE) test-two-player-state
	$(MAKE) test-floppy-assets
	$(MAKE) test-frame-derivable

# Everything that needs no emulator: the host module tests plus the source
# gates. Seconds, and it is what CI checks.
test-fast: test-video test-rng test-physics test-assets test-bricks \
           test-sound test-hud-unit test-objects test-weapons test-enemies \
           test-bonus-codes test-scoring test-replay test-replay-parse \
           test-source-gates
	@echo "test-fast: all host tests and source gates green"

test-hud: $(FLOPPY_OUT)
	python3 scripts/test_hud.py --floppy $(FLOPPY_OUT)

test-bat-redraw-window:
	python3 scripts/test_bat_redraw_window.py

test-ball-dirty-redraw:
	python3 scripts/test_ball_dirty_redraw.py

test-ball-object-dirty-redraw:
	python3 scripts/test_ball_object_dirty_redraw.py

test-bullet-dirty-redraw:
	python3 scripts/test_bullet_dirty_redraw.py

test-bomb-dirty-redraw:
	python3 scripts/test_bomb_dirty_redraw.py

# Blast dirty/full comparison — the missing half of known-bugs.md #9.
test-blast-dirty-redraw:
	python3 scripts/test_blast_dirty_redraw.py

# The only gate on the multi-checkpoint visual probe path.
test-visual-checkpoints:
	python3 scripts/test_visual_checkpoints.py

test-bat-fire-dirty-redraw:
	python3 scripts/test_bat_fire_dirty_redraw.py

test-multiball-dirty-redraw:
	python3 scripts/test_multiball_dirty_redraw.py

test-bigball-dirty-redraw:
	python3 scripts/test_bigball_dirty_redraw.py

test-stuck-ball-dirty-redraw:
	python3 scripts/test_stuck_ball_dirty_redraw.py

# Long-run residue gate: 80 frames of brick destruction with an enemy
# crossing the band, dirty path vs the FORCE_FULL_FLUSH_EACH_FRAME
# baseline (VGA == buffers). Catches stale-VGA leftovers the
# dirty-vs-full-redraw gates can't see (both sides flush identically).
test-enemy-brick-residue:
	python3 scripts/test_enemy_brick_residue.py

# An alien that hits a brick destroys nothing and is not destroyed
# (LAFFC_30 branches on sprite_set; only $02, the ball, reaches the
# destroy path). It latches the snap point in LAA7B and walks there over
# the next few frames. The reaction leaves NO trace on screen, so this
# reads the enemy_home probe word — without it the whole thing is
# unobservable and 75 gates pass either way.
test-enemy-brick-walk:
	python3 scripts/test_enemy_brick_walk.py

# check_margins is three CLAMPS, no reflection and no re-aim. Replacing
# the port's invented reflect-and-re-aim with the literal clamps left all
# 76 gates green — no gate had ever watched an alien reach a margin.
test-enemy-margin-clamp:
	python3 scripts/test_enemy_margin_clamp.py

# Key 0 in the menu starts the game (orig main_menu tail). Not
# observable from a screendump: test-visual walks these screens with
# ENTER and never presses 0.
test-menu-start:
	python3 scripts/check_menu_start.py

# WS2 stage 4: a life loss hands the turn over in mode 1 and does not
# in mode 0. A/B on BATTY_GAME_MODE, read from the probe — the only
# on-screen trace is the round banner's PLAYER digit.
test-two-player-turn:
	python3 scripts/test_two_player_turn.py

# WS3 stage 1: game_mode $02 draws object_separator (LBE8B_10) and
# changes nothing else. A/B on BATTY_GAME_MODE, whole-frame diff.
test-double-play-court:
	python3 scripts/test_double_play_court.py

# WS3: LAB1F falls through to object_bat_2 in mode $02, and LAB1F_0
# re-owns the ball to the bat that hit it. A/B on BATTY_GAME_MODE with
# the ball seeded straight at bat 2.
test-double-play-bat2:
	python3 scripts/test_double_play_bat2.py

# The Kinnock easter egg (POKE 47475,0). Source-gated: it is up for
# ~0.3 s, so a timed screendump would be luck. Its expected text is
# parsed from the tape's own txt_kinnock.asm, not copied.
test-kinnock:
	python3 scripts/check_kinnock.py

# Every asset the port loads must ship, and nothing else. Caught two
# 48 KB screen captures riding along unread.
test-floppy-assets:
	python3 scripts/check_floppy_assets.py

# WS7 item 1 evidence: the frame's top and side strips are tape sprites
# drawn UPWARD, not captured pixels. 2368 of frame_l1.bin's 4968 bytes.
test-frame-derivable:
	python3 scripts/check_frame_derivable.py

# Proof for WS7: every live brick's attr byte in the captured
# level_attrs.bin is reproduced by briks_colors + print_border_shadow.
test-level-attrs-derivable:
	python3 scripts/check_level_attrs_derivable.py

# WS2 stage 1: per-player counters are real, the 2UP HUD slot reads
# players[1] instead of a literal 0. Invisible to a screendump —
# players[1].score is 0 in a 1-player game.
test-two-player-state:
	python3 scripts/check_two_player_state.py

test-rocket-flight-redraw:
	python3 scripts/test_rocket_flight_redraw.py

test-rocket-completion-no-ball:
	python3 scripts/test_rocket_completion_no_ball.py

test-round-banner-border:
	python3 scripts/test_round_banner_border.py

test-bonus-fall:
	python3 scripts/test_bonus_fall.py

test-bomb-fall:
	python3 scripts/test_bomb_fall.py

test-pts400-fall:
	python3 scripts/test_pts400_fall.py

test-bullet-fly:
	python3 scripts/test_bullet_fly.py

test-laser-cadence:
	python3 scripts/test_laser_cadence.py

test-enemy-anim:
	python3 scripts/test_enemy_anim.py

test-bonus-drop:
	python3 scripts/test_bonus_drop.py

test-bonus-effects:
	python3 scripts/test_bonus_effects.py

test-bonus-effects2:
	python3 scripts/test_bonus_effects2.py

test-bonus-typepick:
	python3 scripts/test_bonus_typepick.py

test-bullet-blast:
	python3 scripts/test_bullet_blast.py

test-brick-scoring:
	python3 scripts/test_brick_scoring.py

test-ball-speed-ramp:
	python3 scripts/test_ball_speed_ramp.py

test-brick-flash: $(TEST_FLOPPY_OUT)
	python3 scripts/test_brick_flash.py

test-game-over:
	python3 scripts/test_game_over.py

# The visual half of the game-over coverage. Reaches the screen with
# BATTY_REPLAY_LIVES=1 + BATTY_HIDE_BALL (death on the first frame) and
# holds it with BATTY_HOLD_GAME_OVER, so nothing depends on wall clock.
test-game-over-visual:
	python3 scripts/test_game_over_visual.py

# The last screen that had no visual coverage. Needs a seeded score to
# beat the high score, and one key to leave the game-over hold (#15).
test-name-entry-visual:
	python3 scripts/test_name_entry_visual.py

# Losing a life removes exactly one indicator. An A/B on
# BATTY_SUPPRESS_NO_BALL_DEATH, so the death is the only difference.
test-life-loss:
	python3 scripts/test_life_loss.py

# Level-clear -> next, and the level index wrapping at N_LEVELS. Both
# reachable only because BATTY_REPLAY_CLEAR_BRICKS empties the grid.
test-level-advance:
	python3 scripts/test_level_advance.py

test-stuck-ball-offset:
	python3 scripts/test_stuck_ball_offset.py

test-invariant-owners:
	python3 scripts/test_invariant_owners.py

test-rocket-bonus:
	python3 scripts/test_rocket_bonus.py

test-death-sparks:
	python3 scripts/test_death_sparks.py

test-normal-ball-launch:
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_LAUNCH_FRAMES=12 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/test_normal_ball_launch.py

test-ball-left-wall-escape:
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_FRAME_PROBE=12 BATTY_REPLAY_BAT_OBJECT=01000800AD000000040D15AE1C0A08000000F000FF80 BATTY_REPLAY_BALL_OBJECT=02000900A0002C02020C000008070000000000000080 BATTY_REPLAY_BALL_STUCK=0 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/test_ball_left_wall_escape.py

test-l3-replay-seed:
	python3 scripts/test_l3_replay_seed.py

test-midgame-brick-replay: replay-l3-brick-flash
	python3 scripts/test_midgame_brick_replay.py

replay-l3-brick-flash:
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_ENEMY_OBJECT=0905A4471B642D01030FDD74180CA41C030F30703100 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/replay_harness.py replays/l3-brick-flash.json --side port

replay-l3-brick-flash-both: $(ZESARUX)
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_ENEMY_OBJECT=0905A4471B642D01030FDD74180CA41C030F30703100 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/replay_harness.py replays/l3-brick-flash.json --side both --compare

# L3-entry static parity gate. Both runners pause at main-loop entry, so
# the capture lands on identical bytes on both sides — fail-gated via
# --fail-on-diff with comparison.aligned_start=true.
# Object overrides use the original's probed state at $BA83 (LB9E8_2,
# the original's main-loop entry, BEFORE handling_bat / enemy_prepare
# run on this frame), so the port's pause-time objects exactly match
# the original's probe values. RNG seeded to the snapshot's true f0
# state (random_number $8D48 = 3793, random_seed = 962A — same values
# the frame-step gate uses; the old 8E49 was a stale readback of the
# wrong address $8E17).
replay-l3-entry: $(ZESARUX)
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A BATTY_REPLAY_BAT_OBJECT=01007400AD000000040D00001C0A00000000F0008380 BATTY_REPLAY_BALL_OBJECT=02008400A0000803020C00000807000000000000C08C BATTY_REPLAY_ENEMY_OBJECT=00017800880000000318000018180000000050440000 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/replay_harness.py replays/l3-entry.json --side both --compare --fail-on-diff

tools/zesarux/src/zesarux:
	git submodule update --init tools/zesarux
	cd tools/zesarux/src && ./configure $(ZESARUX_CONFIGURE_OPTS) && $(MAKE)

run-original: $(ZESARUX)
	$(ZESARUX) --noconfigfile --machine 48k \
		$(if $(ZESARUX_VO),--vo $(ZESARUX_VO)) \
		--ao $(ZESARUX_AO) $(ZESARUX_RUN_OPTS) \
		--enable-remoteprotocol --remoteprotocol-port $(ZRCP_PORT) \
		--quickexit $(CURDIR)/original/batty.tap

# Same but with an infinite-lives patch (replaces `dec a` with `or a`
# at PC 0xBD35 - keeps lives at 3 forever and deterministically clears
# Z so the game-over branch never fires). Use `make snapshot` from
# another terminal to capture clean per-level GTs as you play through.
run-original-cheat:
	python3 scripts/run_original_cheat.py

snapshot:
	@mkdir -p build/snapshots
	python3 scripts/snapshot_ram.py build/snapshots

# build/ holds both generated outputs AND tracked inputs (snapshots/,
# level_gt/, gt_history/, the various RE .txt files). `rm -rf build`
# would clobber the tracked ones. Instead, ask git to remove only
# what's gitignored — i.e. everything actually generated.
clean:
	git clean -fdX build
