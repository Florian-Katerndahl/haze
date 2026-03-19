#ifndef FSCHECK_H
#define FSCHECK_H
/**
 * @file fscheck.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures to check file access permissions.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup fscheck File System Checks
 * @{
 */

#include <stdbool.h>

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

/** @} */ // end of group
#endif // FSCHECK_H