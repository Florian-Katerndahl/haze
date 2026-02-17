#include "paths.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char *constructFormattedPath(const char *fmt, ...) {
    char path[MAX_PATH_LENGTH] = {0};

    va_list args;
    va_start(args, fmt);
    int charsWritten = vsnprintf(path, MAX_PATH_LENGTH, fmt, args);
    va_end(args);

    if (charsWritten < 0 || charsWritten >= MAX_PATH_LENGTH) {
        return NULL;
    }

    // include terminating nul character
    size_t pathLength = strlen(path) + 1;

    return strndup(path, pathLength);
}

__attribute__ ((weak, alias("constructFormattedPath"))) char *constructURL(const char *fmt, ...);

__attribute__ ((weak, alias("constructFormattedPath"))) char *constructFilePath(const char *fmt, ...);
