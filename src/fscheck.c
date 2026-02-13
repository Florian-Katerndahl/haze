#define _GNU_SOURCE
#include "fscheck.h"
#include <stdbool.h>
#include <unistd.h>
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

bool fileExists(const char *filePath)
{
  if (euidaccess(filePath, F_OK) == -1) {
    return false;
  }
  return true;
}

bool fileWritable(const char *filePath)
{
  if (euidaccess(filePath, W_OK) == -1) {
    return false;
  }
  return true;
}

bool fileReadable(const char *filePath)
{
  if (euidaccess(filePath, R_OK) == -1) {
    return false;
  }
  return true;
}
