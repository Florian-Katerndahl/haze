CC=gcc
DEFINES=-D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS
SANITIZERS+=-fsanitize=address,undefined,leak -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fstack-protector-strong -fstack-clash-protection
CFLAGS=-Wall -Wextra -pedantic -std=c2x -flto -ggdb $(SANITIZERS)
JANSONFLAGS=$(shell pkg-config --cflags --libs jansson)
CURLFLAGS=$(shell pkg-config --cflags --libs libcurl)
GDALFLAGS=$(shell pkg-config --cflags --libs gdal)
MATHFLAGS=-lm
LOCALFLAGS=-L

.PHONY: all clean

all: main

build/fscheck.o: src/fscheck.c src/fscheck.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/fscheck.c -o $@

build/aoi.o: src/aoi.c src/aoi.h
	$(CC) $(DEFINES) $(CFLAGS) -c src/aoi.c $(GDALFLAGS) -o $@

build/main.o: main.c
	$(CC) $(DEFINES) $(CFLAGS) -c main.c -o build/main.o

main: build/main.o build/aoi.o build/fscheck.o	
	$(CC) $(DEFINES) $(CFLAGS) $^ $(GDALFLAGS) -o $@

clean:
	rm -f main
	rm -f build/*
