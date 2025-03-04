/* 
Documentation warns about usage of euidaccess, but since I don't do any safety-critical things, I don't care
*/
#ifndef FSCHECK_H
#define FSCHECK_H

#include <stdbool.h>

bool fileExists(const char *filePath);

bool fileWritable(const char *filePath);

bool fileReadable(const char *filePath);

#endif // FSCHECK_H