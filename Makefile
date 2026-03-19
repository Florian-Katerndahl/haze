CC=gcc
DEFINES=-D_FORTIFY_SOURCE=3
SANITIZERS=-fsanitize=address,undefined,leak -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fstack-protector-strong -fstack-clash-protection
OPTIMIZATION=-O3
CFLAGS=-Wall -Wextra -pedantic -std=c2x -flto $(SANITIZERS) $(OPTIMIZATION)
LINKFLAGS=$(shell pkg-config --cflags --libs jansson)
LINKFLAGS+=$(shell pkg-config --cflags --libs libcurl)
LINKFLAGS+=$(shell pkg-config --cflags --libs gdal)
LINKFLAGS+=$(shell pkg-config --cflags --libs geos)
LINKFLAGS+=-lm

OBJECTS := paths.o fscheck.o aoi.o haze.o types.o gdal-ops.o math-utils.o options.o api.o strtree.o
OBJECT_PATHS := $(foreach obj,$(OBJECTS),build/$(obj))

.PHONY: all
all: haze docs

debug: DEFINES+=-DDEBUG
debug: CFLAGS+=-ggdb
debug: override OPTIMIZATION=-O0
debug: haze

haze: main.c $(OBJECT_PATHS)
	$(CC) $(DEFINES) $(CFLAGS) $^ $(LINKFLAGS) -o $@

docs: Doxyfile manual/*.md src/*.h
	doxygen Doxyfile

$(OBJECT_PATHS): build/%.o: src/%.c
	$(CC) $(DEFINES) $(CFLAGS) $^ -c -o $@

.PHONY: clean
clean:
	rm -f haze
	rm -f build/*
	rm -fr docs/*
