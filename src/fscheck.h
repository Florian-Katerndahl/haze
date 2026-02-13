/**
 * @file fscheck.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures to check file access permissions.
 * @version 0.1
 * @date 2026-02-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef FSCHECK_H
#define FSCHECK_H

#include <stdbool.h>

#define MAX_PATH_LENGTH 1024

/// FIXME: this definitvely needs refactoring now as this function doesn't fit the file but I also don't really know where else to put it...
/**
 * @brief Construct file system paths and URLs with dynamic content up to fixed size
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use with.
 *
 * @note The URL can't be longer than `MAX_PATH_LENGTH` - 1 characters.
 *
 * @param fmt Format string for the path/URL.
 * @param ... Arguments for format placeholders.
 * @return char* Reference to heap-allocated path/URL, NULL on error.
 */
char *constructFormattedPath(const char *fmt, ...);

/**
 * @brief Check if a file exists.
 * 
 * @param filePath File path to check.
 * @return true Return true if file exists.
 * @return false Return false if file does not exist.
 */
bool fileExists(const char *filePath);

/**
 * @brief Check if a file is writable, using the effective user and group identifiers of the calling process.
 * 
 * @remark Race conditions may arise between calling the underlying function `euidaccess` and actually writing the file.
 * 
 * @param filePath File path to check.
 * @return true Return true if write access is granted.
 * @return false Return false if write access is not granted
 */
bool fileWritable(const char *filePath);

/**
 * @brief Check if a file is readable, usinig the effective user and group identifiers of the calling process.
 * 
 * @remark Race conditions may arise between calling the underlying function `euidaccess` and actually reading the file.
 * 
 * @param filePath File path to check.
 * @return true Return true if read access is granted.
 * @return false Return false if read access is not granted
 */
bool fileReadable(const char *filePath);

#endif // FSCHECK_H