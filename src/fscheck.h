#ifndef FSCHECK_H
#define FSCHECK_H

#include <stdbool.h>

bool fileExists(char *filePath);

bool fileWritable(char *filePath);

bool fileReadable(char *filePath);

bool directoryExists(char *dirPath);

bool directoryWritable(char *dirPath);

bool directoryReadable(char *dirPath);

#endif // FSCHECK_H