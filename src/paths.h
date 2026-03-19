#ifndef PATHS_H
#define PATHS_H
/**
 * @file paths.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes two alias functions used to construct file paths and URLs
 * @version 0.1
 * @date 2026-02-17
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup paths Resource Path generation
 * @{
 */

#define MAX_PATH_LENGTH 1024

/**
 * @brief Construct file system URLs with dynamic content up to fixed size
 *
 * @remark This is an alias to `constructFormattedPath`
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use with.
 *
 * @note The URL can't be longer than `MAX_PATH_LENGTH` - 1 characters.
 *
 * @param fmt Format string for the path/URL.
 * @param ... Arguments for format placeholders.
 * @return char* Reference to heap-allocated path/URL, NULL on error.
 */
char *constructURL(const char *fmt, ...);

/**
 * @brief Construct file system paths with dynamic content up to fixed size
 *
 * @remark This is an alias to `constructFormattedPath`
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use with.
 *
 * @note The URL can't be longer than `MAX_PATH_LENGTH` - 1 characters.
 *
 * @param fmt Format string for the path/URL.
 * @param ... Arguments for format placeholders.
 * @return char* Reference to heap-allocated path/URL, NULL on error.
 */
char *constructFilePath(const char *fmt, ...);

/** @} */ // end of group
#endif // PATHS_H