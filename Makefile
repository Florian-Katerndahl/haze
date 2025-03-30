CC=gcc
DEFINES=-D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -DDEBUG
SANITIZERS+=-fsanitize=address,undefined,leak -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fstack-protector-strong -fstack-clash-protection
CFLAGS=-Wall -Wextra -pedantic -std=c2x -flto -ggdb -O0 $(SANITIZERS)
JANSONFLAGS=$(shell pkg-config --cflags --libs jansson)
CURLFLAGS=$(shell pkg-config --cflags --libs libcurl)
GDALFLAGS=$(shell pkg-config --cflags --libs gdal)
GEOSFLAGS=$(shell pkg-config --cflags --libs geos)
MATHFLAGS=-lm
LOCALFLAGS=-L

.PHONY: all clean

all: main

build/fscheck.o: src/fscheck.c src/fscheck.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/fscheck.c -o $@

build/aoi.o: src/aoi.c src/aoi.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/aoi.c $(GDALFLAGS) -o $@

build/haze.o: src/haze.c src/haze.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/haze.c $(GDALFLAGS) -o $@

build/types.o: src/types.c src/types.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/types.c $(GDALFLAGS) $(GEOSFLAGS) -o $@

build/gdal-ops.o: src/gdal-ops.c src/gdal-ops.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/gdal-ops.c $(GDALFLAGS) -o $@

build/math-utils.o: src/math-utils.c src/math-utils.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/math-utils.c -o $@

build/options.o: src/options.c src/options.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/options.c -o $@

build/api.o: src/api.c src/api.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/api.c $(CURLFLAGS) $(JANSONFLAGS) -o $@

build/strtree.o: src/strtree.c src/strtree.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/strtree.c $(GDALFLAGS) $(GEOSFLAGS) -o $@

build/main.o: main.c
	$(CC) $(DEFINES) $(CFLAGS) -c main.c $(GDALFLAGS) $(GEOSFLAGS) $(CURLFLAGS) -o build/main.o

main: build/main.o build/options.o build/types.o build/api.o build/aoi.o build/haze.o build/fscheck.o build/gdal-ops.o build/strtree.o build/math-utils.o
	$(CC) $(DEFINES) $(CFLAGS) $^ $(GDALFLAGS) $(GEOSFLAGS) $(CURLFLAGS) $(CURLFLAGS) $(JANSONFLAGS) -o haze

clean:
	rm -f main
	rm -f build/*
