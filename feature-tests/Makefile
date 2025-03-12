CC=gcc
DEFINES=-D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS
SANITIZERS+=-fsanitize=address,undefined,leak -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fstack-protector-strong -fstack-clash-protection
CFLAGS=-Wall -Wextra -pedantic -std=c2x -flto -ggdb -O0 $(SANITIZERS)
JANSONFLAGS=$(shell pkg-config --cflags --libs jansson)
CURLFLAGS=$(shell pkg-config --cflags --libs libcurl)
GDALFLAGS=$(shell pkg-config --cflags --libs gdal)
GEOSFLAGS=$(shell pkg-config --cflags --libs geos)
MATHFLAGS=-lm
LOCALFLAGS=-L

.PHONY: clean test-strtree

test-strtree:
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/fscheck.c -o ../build/fscheck.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/math-utils.c -o ../build/math-utils.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/strtree.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/strtree.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/haze.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/haze.o
	$(CC) $(DEFINES) $(CFLAGS) -c test.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/test.o
	$(CC) $(DEFINES) $(CFLAGS) ../build/math-utils.o ../build/fscheck.o ../build/strtree.o ../build/haze.o ../build/test.o $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o $@

clean:
	rm -f main
	rm -f build/*
