# batty — MS-DOS recreation of Batty (Elite, 1987) targeting 8086 + VGA.
# Toolchain: Open Watcom v2 + mtools + QEMU.

WATCOM_DIR ?= vendor/openwatcom-v2/current-build-2026-05-16
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
OBJ      = $(SRC:src/%.c=build/%.obj)
TEST_OBJ = build/main-test.obj
HEADERS = $(wildcard src/*.h)
EXE     = build/batty.exe
TEST_EXE = build/batty-test.exe

ASSETS  = assets/loading.bin assets/hi_score.bin assets/main_menu.bin \
          assets/font.bin assets/markup.bin assets/main_menu_markup.bin \
          assets/indicator.bin assets/bottom_sprites.bin \
          assets/hud_sprites.bin \
          assets/levels.bin assets/level_attrs.bin \
          assets/bg_tile.bin assets/frame_l1.bin \
          assets/sprites.bin assets/random_seed.bin
HISCORE_SNAP      ?= build/snapshots/20260513T202038Z/screen.scr
MAINMENU_SNAP     ?= build/snapshots/20260513T202041Z/screen.scr
MAINMENU_SNAP_RAM ?= build/snapshots/20260513T202041Z/ram_4000_FFFF.bin
LEVEL1_SNAP_RAM   ?= build/snapshots/20260513T202101Z/ram_4000_FFFF.bin

FLOPPY_SRC      ?= vendor/msdos/floppy-minimal.img
# `make run`: normal interactive boot image.
FLOPPY_OUT       = build/batty.img
# `make test`: full 4-state visual-regression image.
TEST_FLOPPY_OUT  = build/batty-test.img

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

.PHONY: all clean run run-86box profile-auto profile-86box read-profile floppy assets help run-original run-original-cheat snapshot candidates regions test test-hud test-bat-redraw-window test-ball-dirty-redraw test-ball-object-dirty-redraw test-rocket-flight-redraw test-rocket-completion-no-ball test-round-banner-border test-brick-flash test-rocket-bonus test-death-sparks test-normal-ball-launch test-ball-left-wall-escape test-l3-replay-seed test-midgame-brick-replay replay-l3-brick-flash replay-l3-brick-flash-both

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
	@echo "  test-brick-flash  verify brick flash clears vs original L3 reference"
	@echo "  test-ball-dirty-redraw  verify ball-only dirty redraw vs full baseline"
	@echo "  test-ball-object-dirty-redraw  verify ball+enemy dirty redraw vs full baseline"
	@echo "  test-rocket-flight-redraw  verify rocket-lifted bat redraw vs full baseline"
	@echo "  test-rocket-completion-no-ball  verify rocket clear redraw hides balls"
	@echo "  test-round-banner-border  verify original round-window black top band"
	@echo "  test-rocket-bonus  verify rocket bonus cannot trigger no-ball death"
	@echo "  test-death-sparks  verify bat death spark fanout mirrors original"
	@echo "  test-l3-replay-seed  verify deterministic L3 replay seed/probes"
	@echo "  test-midgame-brick-replay  fail-gate seeded L3 brick destruction replay"
	@echo "  replay-l3-brick-flash       run the L3 replay against the DOS port"
	@echo "  replay-l3-brick-flash-both  run DOS + original and print INFO diffs"
	@echo "  clean         remove build/"

build:
	@mkdir -p build

build/%.obj: src/%.c $(HEADERS) | build
	$(WCC) $(WCCFLAGS) -fo=$@ $<

$(TEST_OBJ): src/main.c $(HEADERS) | build
	$(WCC) $(WCCFLAGS) -dBATTY_SCORELESS_HUD -fo=$@ $<

$(EXE): $(OBJ)
	$(WLINK) name $@ format dos $(addprefix file ,$(OBJ)) libpath $(WATCOM_LIB) library clibs.lib

$(TEST_EXE): $(TEST_OBJ)
	$(WLINK) name $@ format dos file $(TEST_OBJ) libpath $(WATCOM_LIB) library clibs.lib

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
# Perimeter frame (top HUD + left + right cyan strips). 3 strips
# painted as raw pixels + per-char attrs; bottom edge has no frame
# ornament, so we skip it. ~1.3 KB total.
# assets/frame_l1.bin is a checked-in asset, NOT regenerated from the
# current build/level_gt/ GT — because frame extraction needs a GT
# captured with the L6853 lives-skip patch on (no lives in the side
# strip), while the test GT needs lives present (so render_lives is
# measured). Two contradictory requirements. The frame is re-extracted
# manually when needed; see notes/state4-bat-band-triage.md.

# Sprite block extracted verbatim from the original game's program at
# $7A8C..$8D46 (offset $128c..$2546 within 03_DATA_headless.dat.bin,
# which loads at $6800). Contains all masked sprites we need:
#   spr_big_ball, spr_lives_indicator, spr_ball_normal,
#   spr_bat_normal, spr_bat_big, spr_ufo_1..6, spr_bird_1..6,
#   spr_alien_blast_1..5, spr_bonus_* through spr_bonus_triple_ball
# (offsets recorded in main.c). Format per sprite: (width_bytes,
# height_rows) + rows of (mask, pixel) pairs per byte-column,
# drawn via blit_masked_sprite.
assets/sprites.bin: original/blocks/03_DATA_headless.dat.bin Makefile
	@python3 -c "import sys; b=open('$<','rb').read(); \
		open('$@','wb').write(b[0x128c:0x2546])"
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
	mcopy -i $@ -o assets/hud_sprites.bin ::HUDSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/frame_l1.bin ::FRAMEL1.BIN
	mcopy -i $@ -o assets/sprites.bin ::SPRITES.BIN
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
	@printf 'BATTY\r\n' >> build/AUTOEXEC.BAT
	mcopy -i $@ -o build/AUTOEXEC.BAT ::AUTOEXEC.BAT
	@echo "Floppy ready: $@  (menu-only cycle)"

$(TEST_FLOPPY_OUT): $(TEST_EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(TEST_EXE) ::BATTY.EXE
	mcopy -i $@ -o assets/loading.bin  ::LOADING.BIN
	mcopy -i $@ -o assets/hi_score.bin ::HISCORE.BIN
	mcopy -i $@ -o assets/main_menu.bin ::MAINMENU.BIN
	mcopy -i $@ -o assets/font.bin     ::FONT.BIN
	mcopy -i $@ -o assets/markup.bin   ::MARKUP.BIN
	mcopy -i $@ -o assets/main_menu_markup.bin ::MENUMARK.BIN
	mcopy -i $@ -o assets/indicator.bin ::INDICAT.BIN
	mcopy -i $@ -o assets/bottom_sprites.bin ::BOTSPR.BIN
	mcopy -i $@ -o assets/hud_sprites.bin ::HUDSPR.BIN
	mcopy -i $@ -o assets/levels.bin ::LEVELS.BIN
	mcopy -i $@ -o assets/level_attrs.bin ::LVLATTR.BIN
	mcopy -i $@ -o assets/bg_tile.bin ::BGTILE.BIN
	mcopy -i $@ -o assets/frame_l1.bin ::FRAMEL1.BIN
	mcopy -i $@ -o assets/sprites.bin ::SPRITES.BIN
	mcopy -i $@ -o assets/random_seed.bin ::RANDOM.BIN
	@# BATTY_LEVEL env passthrough — without injecting `SET BATTY_LEVEL=N`
	@# into the DOS boot AUTOEXEC.BAT, the C-side getenv() at run_level
	@# never sees the host's env. The line is only emitted when the host
	@# var is non-empty, so `make test` defaults to L1 as before.
	@if [ -n "$$BATTY_LEVEL" ]; then \
	    printf '@ECHO OFF\r\nSET BATTYALL=1\r\nSET BATTY_LEVEL=%s\r\n' "$$BATTY_LEVEL" > build/AUTOEXEC-T.BAT ; \
	else \
	    printf '@ECHO OFF\r\nSET BATTYALL=1\r\n' > build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_START_LEVEL" ]; then \
	    printf 'SET BATTY_START_LEVEL=%s\r\n' "$$BATTY_START_LEVEL" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_PROBE" ]; then \
	    printf 'SET BATTY_REPLAY_PROBE=%s\r\n' "$$BATTY_REPLAY_PROBE" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_RANDOM" ]; then \
	    printf 'SET BATTY_REPLAY_RANDOM=%s\r\n' "$$BATTY_REPLAY_RANDOM" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BAT_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BAT_OBJECT=%s\r\n' "$$BATTY_REPLAY_BAT_OBJECT" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_OBJECT=%s\r\n' "$$BATTY_REPLAY_BALL_OBJECT" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_STUCK" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_STUCK=%s\r\n' "$$BATTY_REPLAY_BALL_STUCK" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_HIDE_BALL" ]; then \
	    printf 'SET BATTY_HIDE_BALL=%s\r\n' "$$BATTY_HIDE_BALL" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_SUPPRESS_NO_BALL_DEATH" ]; then \
	    printf 'SET BATTY_SUPPRESS_NO_BALL_DEATH=%s\r\n' "$$BATTY_SUPPRESS_NO_BALL_DEATH" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_BALL_VEL" ]; then \
	    printf 'SET BATTY_REPLAY_BALL_VEL=%s\r\n' "$$BATTY_REPLAY_BALL_VEL" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_ENEMY_OBJECT" ]; then \
	    printf 'SET BATTY_REPLAY_ENEMY_OBJECT=%s\r\n' "$$BATTY_REPLAY_ENEMY_OBJECT" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_ROCKET_ACTIVE" ]; then \
	    printf 'SET BATTY_REPLAY_ROCKET_ACTIVE=%s\r\n' "$$BATTY_REPLAY_ROCKET_ACTIVE" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_REPLAY_WAIT_KEY" ]; then \
	    printf 'SET BATTY_REPLAY_WAIT_KEY=%s\r\n' "$$BATTY_REPLAY_WAIT_KEY" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_LAUNCH_FRAMES" ]; then \
	    printf 'SET BATTY_LAUNCH_FRAMES=%s\r\n' "$$BATTY_LAUNCH_FRAMES" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_FRAME_PROBE" ]; then \
	    printf 'SET BATTY_FRAME_PROBE=%s\r\n' "$$BATTY_FRAME_PROBE" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_VISUAL_PROBE_FRAMES" ]; then \
	    printf 'SET BATTY_VISUAL_PROBE_FRAMES=%s\r\n' "$$BATTY_VISUAL_PROBE_FRAMES" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_HOLD_ROUND_BANNER" ]; then \
	    printf 'SET BATTY_HOLD_ROUND_BANNER=%s\r\n' "$$BATTY_HOLD_ROUND_BANNER" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_HOLD_ROCKET_CLEAR" ]; then \
	    printf 'SET BATTY_HOLD_ROCKET_CLEAR=%s\r\n' "$$BATTY_HOLD_ROCKET_CLEAR" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_FORCE_BAT_FULL_REDRAW" ]; then \
	    printf 'SET BATTY_FORCE_BAT_FULL_REDRAW=%s\r\n' "$$BATTY_FORCE_BAT_FULL_REDRAW" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_FORCE_BALL_FULL_REDRAW" ]; then \
	    printf 'SET BATTY_FORCE_BALL_FULL_REDRAW=%s\r\n' "$$BATTY_FORCE_BALL_FULL_REDRAW" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	if [ -n "$$BATTY_FORCE_FULL_FLUSH_EACH_FRAME" ]; then \
	    printf 'SET BATTY_FORCE_FULL_FLUSH_EACH_FRAME=%s\r\n' "$$BATTY_FORCE_FULL_FLUSH_EACH_FRAME" >> build/AUTOEXEC-T.BAT ; \
	fi; \
	printf 'BATTY\r\n' >> build/AUTOEXEC-T.BAT
	mcopy -i $@ -o build/AUTOEXEC-T.BAT ::AUTOEXEC.BAT
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
TL_ROI    ?= 8,32,248,128
TL_MAXDIFF ?= 0
L3_SEED_ENV = BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_RANDOM=8E49 \
	BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 \
	BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C \
	BATTY_REPLAY_BALL_STUCK=0 \
	BATTY_REPLAY_ENEMY_OBJECT=0905A4471B642D01030FDD74180CA41C030F30703100
capture-timeline-both: $(ZESARUX)
	@rm -f $(TEST_FLOPPY_OUT)
	@$(L3_SEED_ENV) BATTY_VISUAL_PROBE_FRAMES=$(TL_FRAMES) $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/capture_frame_timeline.py --floppy $(TEST_FLOPPY_OUT) \
	    --frames $(TL_FRAMES) --out build/tl_port
	python3 scripts/capture_frame_timeline_original.py --snapshot $(SNAPSHOT) \
	    --frames $(TL_FRAMES) --zesarux $(ZESARUX) \
	    --setup-from-replay $(SETUP_REPLAY) --out build/tl_orig
	python3 scripts/compare_timelines.py --port build/tl_port \
	    --original build/tl_orig --frames $(TL_FRAMES) --roi $(TL_ROI) \
	    --max-diff $(TL_MAXDIFF)

test-hud: $(FLOPPY_OUT)
	python3 scripts/test_hud.py --floppy $(FLOPPY_OUT)

test-bat-redraw-window:
	python3 scripts/test_bat_redraw_window.py

test-ball-dirty-redraw:
	python3 scripts/test_ball_dirty_redraw.py

test-ball-object-dirty-redraw:
	python3 scripts/test_ball_object_dirty_redraw.py

test-rocket-flight-redraw:
	python3 scripts/test_rocket_flight_redraw.py

test-rocket-completion-no-ball:
	python3 scripts/test_rocket_completion_no_ball.py

test-round-banner-border:
	python3 scripts/test_round_banner_border.py

test-brick-flash: $(TEST_FLOPPY_OUT)
	python3 scripts/test_brick_flash.py

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
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=8E49 BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_ENEMY_OBJECT=0905A4471B642D01030FDD74180CA41C030F30703100 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/replay_harness.py replays/l3-brick-flash.json --side port

replay-l3-brick-flash-both: $(ZESARUX)
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=8E49 BATTY_REPLAY_BAT_OBJECT=01017400AD000000040DEFAE1C0A74AD040DF0008380 BATTY_REPLAY_BALL_OBJECT=02006C004E001F03020CEEF008076C4E020C0000008C BATTY_REPLAY_BALL_STUCK=0 BATTY_REPLAY_ENEMY_OBJECT=0905A4471B642D01030FDD74180CA41C030F30703100 $(MAKE) $(TEST_FLOPPY_OUT)
	python3 scripts/replay_harness.py replays/l3-brick-flash.json --side both --compare

# L3-entry static parity gate. Both runners pause at main-loop entry, so
# the capture lands on identical bytes on both sides — fail-gated via
# --fail-on-diff with comparison.aligned_start=true.
# Object overrides use the original's probed state at $BA83 (LB9E8_2,
# the original's main-loop entry, BEFORE handling_bat / enemy_prepare
# run on this frame), so the port's pause-time objects exactly match
# the original's probe values.
replay-l3-entry: $(ZESARUX)
	rm -f $(TEST_FLOPPY_OUT)
	BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_RANDOM=8E49 BATTY_REPLAY_BAT_OBJECT=01007400AD000000040D00001C0A00000000F0008380 BATTY_REPLAY_BALL_OBJECT=02008400A0000803020C00000807000000000000C08C BATTY_REPLAY_ENEMY_OBJECT=00017800880000000318000018180000000050440000 $(MAKE) $(TEST_FLOPPY_OUT)
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
