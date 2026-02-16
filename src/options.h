/**
 * @file options.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file contains function signatures to parse command line options for haze.
 * @version 0.1
 * @date 2026-02-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include "types.h"
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief Print a help string for haze to `stdout`.
 * 
 */
void printHelp(void);

/**
 * @brief Parse command line options for haze
 * 
 * @details Parse command line options specified by user, validate numeric inputs, read
 *          authentication token and force trailing slash on directory paths.
 * 
 * @remark The returned object is owned by the caller and must be freed after use.
 * 
 * @param argc Argument count
 * @param argv Argument values
 * @return option_t* Reference to parsed options, NULL on error.
 */
[[nodiscard]] option_t *parseOptions(int argc, char *argv[]);

/**
 * @brief Parse string representation of integers of various forms to array of integer
 * 
 * @details This functions performs a series of checks to determine if the string contains a
 *          range of integers (i.e. "min:max") or a comma-separated list of values (i.e. "v1,v2,v3").
 *          If neither format is found, a scalar value is assumed.
 *          After parsing, it is checked that all values are within [min, max].
 * 
 * @param arr Array to store converted values in.
 * @param n Size of array.
 * @param argString String representation of integers in various forms.
 * @param min Minimum value, all stored values must be greater or equal than this value.
 * @param max Maximum value, all stored values must be smaller or equal than this value.
 * @return int 0 on success, 1 on failure.
 * 
 * @related parseRange, parseList, parseSingle
 */
int parseIntegers(int *arr, size_t n, char *argString, const int min, const int max);

/**
 * @brief Parse a range of integers denoted by min:max to list of intgers with closed interval bounds
 * 
 * @note It is not guarantueed that arr is large enough to hold all values specified by the range.
 *       Thus, the list may be truncated silently.
 * @note If max < min, an error is returned.
 * 
 * @param arr Array to store converted values in.
 * @param n Size of array.
 * @param argString String representation of integer range.
 * @return int 0 on success, 1 on failure.
 */
int parseRange(int *arr, size_t n, const char *argString);

/**
 * @brief Parse a comma-separated list of integers to an list of integers
 * 
 * @note The argString argument is modified by replacing all occurences of a comma with NULL.
 * @note This function uses `atoi` and does not catch conversion errors.
 * 
 * @param arr Array to store converted values in.
 * @param n Size of array.
 * @param argString String representation of comma-separated intger values.
 * @return int Always return 0.
 */
int parseList(int *arr, size_t n, char *argString);

/**
 * @brief Convert the string representation of an integer to an integer
 * 
 * @note This function uses `atoi` and does not catch conversion errors.
 * 
 * @param arr Array to store converted value in.
 * @param argString String representation of integer.
 * @return int Always return 0.
 */
int parseSingle(int *arr, const char *argString);

/**
 * @brief Test if all elements in an array are within range and initialized
 * 
 * @details Test if all elements in `arr` (up to n) are in the closed interval [min, max]
 *          and different to the symbolic constant INITVAL.
 * 
 * @param arr Array to check
 * @param n Length of array
 * @param min Minimum value, all stored values must be greater or equal than this value.
 * @param max Maximum value, all stored values must be smaller or equal than this value.
 * @return true Return true, iff all values in `arr` are in (min, max) and different from `INITVAL` constant.
 * @return false Return false, otherwise.
 */
bool validateArray(const int *arr, const size_t n, const int min, const int max);

/**
 * @brief Get CDS API authentication token from various sources
 * 
 * @details Query both the environment variables of the running process, possibly a specified file
 *          path and `$HOME/.cdsapirc` in that order. Returns after the first successfull
 *          token extraction.
 * 
 * @param authenticationToken Pointer to `char *` where token should be stored 
 * @param filePath File storing CDS API credentials, possibly NULL.
 * @return int 0 on success, 1 on failure.
 */
int getAuthentication(char **authenticationToken, const char *filePath, bool *neededAllocation);

/**
 * @brief Get CDS API authentication token from environment
 * 
 * @details Query the environment of the calling process for a variable called `ADSAUTH`.
 *          If found, the corresponding value is used for authentication.
 * 
 * @note After the function returns successfully, the caller owns `*authenticationToken` and must free it after use.
 * 
 * @param authenticationToken Indirect reference to object storing the access token/key.
 * @return int 0 on success, 1 on failure.
 */
int getAuthenticationFromEnvironment(char **authenticationToken);

/**
 * @brief Get CDS API authentication token from a file
 * 
 * @details Get the authentication token from a specified file. In case `filePath` is set
 *          to NULL, try to access the file `$HOME/.cdsapirc` and extract the authentication
 *          token from there.
 * 
 * @note After the function returns successfully, the caller owns `*authenticationToken` and must free it after use.
 * 
 * @param authenticationToken Indirect reference to object storing the access token/key.
 * @param filePath File storing CDS API credentials, possibly NULL.
 * @return int 0 on success, 1 on failure.
 */
int getAuthenticationFromFile(char **authenticationToken, const char *filePath);

/**
 * @brief Parse `cdsapirc` file and extract the key value
 *
 * @note Assumes no padding for value to extract.
 *
 * @note After the function returns, the caller owns the returned object and must free it after use.
 * 
 * @param cdsapirc File path to `.cdsapirc`.
 * @return char* Reference to key, NULL on error.
 */
[[nodiscard]] char *extractKey(const char *cdsapirc);

/**
 * @brief Enforce file system path doesn't end with a slash
 * 
 * @param options Reference to parsed options.
 */
void forceNoTrailingSlash(const option_t *options);

/**
 * @brief Print fields of options struct to `stdout`
 * 
 * @param options Object whose fields to print.
 */
void printOptions(const option_t *options);

#endif // OPTIONS_H