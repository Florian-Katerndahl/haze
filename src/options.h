#ifndef OPTIONS_H
#define OPTIONS_H

#include "types.h"
#include <stdbool.h>
#include <stdlib.h>

void printHelp(void);

[[nodiscard]] option_t *parseOptions(int argc, char *argv[]);

int parseIntegers(int *arr, size_t n, char *argString, const int min, const int max);

int parseRange(int *arr, size_t n, const char *argString);

int parseList(int *arr, size_t n, char *argString);

int parseSingle(int *arr, const char *argString);

bool validateArray(const int *arr, const size_t n, const int min, const int max);

int getAuthentication(char **authenticationToken, const char *filePath, bool *neededAllocation);

int getAuthenticationFromEnvironment(char **authenticationToken);

int getAuthenticationFromFile(char **authenticationToken, const char *filePath);

void printOptions(const option_t *options);


#endif // OPTIONS_H