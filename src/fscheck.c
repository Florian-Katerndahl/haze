#define _GNU_SOURCE
#include "fscheck.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

bool fileExists(const char *filePath) {
    if (euidaccess(filePath, F_OK) == -1) {
        perror("euidaccess");
        return false;
    }
    return true;
}

bool fileWritable(const char *filePath) {
    if (euidaccess(filePath, W_OK) == -1) {
        perror("euidaccess");
        return false;
    }
    return true;
}

bool fileReadable(const char *filePath) {
    if (euidaccess(filePath, R_OK) == -1) {
        perror("euidaccess");
        return false;
    }
    return true;
}
