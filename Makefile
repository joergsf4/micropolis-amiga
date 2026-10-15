CC       = m68k-amigaos-gcc
ARCH     = -noixemul -m68020 -fsingle-precision-constant -O2 -fomit-frame-pointer
# the upstream simulation core is old K&R-ish C: no warnings from it
COREFLAGS = $(ARCH) -std=gnu89 -w -Isrc/core -Isrc/amiga
AMIFLAGS  = $(ARCH) -std=gnu99 -Wall -Wno-unused-parameter -Isrc/core -Isrc/amiga
DEST     = /share/Micropolis
UP       = upstream

CORE_SRC = $(wildcard src/core/*.c)
CORE_OBJ = $(patsubst src/%.c,out/obj/%.o,$(CORE_SRC))

all: out/micropolis out/headless out/rtgtest out/listmodes data
	mkdir -p $(DEST)
	cp out/micropolis out/headless out/rtgtest out/listmodes $(DEST)/
	# icons like in the release (the drawer icon lives next to the drawer)
	cp out/icons/Micropolis.info $(DEST)/micropolis.info
	cp out/icons/Micropolis-drawer.info $(DEST)/../Micropolis.info
	cp release/ReadMe $(DEST)/ReadMe; cp out/icons/ReadMe.info $(DEST)/ReadMe.info
	cp COPYING $(DEST)/COPYING

out/obj/core/%.o: src/core/%.c $(wildcard src/core/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(COREFLAGS) -c $< -o $@

out/obj/amiga/%.o: src/amiga/%.c $(wildcard src/core/*.h src/amiga/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(AMIFLAGS) -c $< -o $@

out/headless: $(CORE_OBJ) out/obj/amiga/ui_core.o out/obj/amiga/stack.o out/obj/amiga/mapview.o out/obj/amiga/headless.o
	$(CC) $(ARCH) -o $@ $^ -lm

out/micropolis: $(CORE_OBJ) out/obj/amiga/ui_core.o out/obj/amiga/stack.o out/obj/amiga/gfx.o out/obj/amiga/sound.o out/obj/amiga/budget_ui.o out/obj/amiga/notice_ui.o out/obj/amiga/eval_ui.o out/obj/amiga/graph_ui.o out/obj/amiga/mapview.o out/obj/amiga/main.o
	$(CC) $(ARCH) -o $@ $^ -lm

out/listmodes: src/amiga/listmodes.c
	@mkdir -p out
	$(CC) $(AMIFLAGS) -o $@ $<

out/rtgtest: src/amiga/rtgtest.c
	@mkdir -p out
	$(CC) $(AMIFLAGS) -o $@ $<

# simulation data: string tables, scenarios, tile data (resource files) and the city files
data:
	mkdir -p $(DEST)/res $(DEST)/cities
	cp $(UP)/res/stri.* $(UP)/res/hexa.* $(UP)/res/snro.* $(DEST)/res/
	cp $(UP)/cities/*.cty $(DEST)/cities/
	cp out/data/*.bin out/data/notices.txt $(DEST)/res/
	mkdir -p $(DEST)/res/sounds; cp out/data/sounds/*.snd $(DEST)/res/sounds/

clean:
	rm -rf out
