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
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/gdal-ops.c $(GDALFLAGS) -o ../build/gdal-ops.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/types.c $(GDALFLAGS) $(GEOSFLAGS) -o ../build/types.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/strtree.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/strtree.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/haze.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/haze.o
	$(CC) $(DEFINES) $(CFLAGS) -c test.c $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o ../build/test.o
	$(CC) $(DEFINES) $(CFLAGS) ../build/math-utils.o ../build/fscheck.o ../build/types.o ../build/gdal-ops.o ../build/strtree.o ../build/haze.o ../build/test.o $(GDALFLAGS) $(MATHFLAGS) $(GEOSFLAGS) -o $@

test-api:
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/types.c $(GDALFLAGS) $(GEOSFLAGS) -o ../build/types.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/api.c $(CURLFLAGS) $(JANSONFLAGS) -o ../build/api.o
	$(CC) $(DEFINES) $(CFLAGS) -c ../src/options.c $(CURLFLAGS) $(JANSONFLAGS) -o ../build/options.o
	$(CC) $(DEFINES) $(CFLAGS) -c api-tests.c $(CURLFLAGS) $(JANSONFLAGS) -o ../build/api-tests.o
	$(CC) $(DEFINES) $(CFLAGS) ../build/types.o ../build/api.o ../build/options.o ../build/api-tests.o $(GDALFLAGS) $(GEOSFLAGS) $(CURLFLAGS) $(JANSONFLAGS) -o api-tests


clean:
	rm -f main
	rm -f build/*
