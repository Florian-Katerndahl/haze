#ifndef NUMERIC_CONVERSIONS_H
#define NUMERIC_CONVERSIONS_H
/**
 * @file numeric-conversions.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file contains function signatures to parse integers from strings.
 * @version 0.1
 * @date 2026-06-27
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup num_conversions Numeric Conversion of Integers from Strings (base 10)
 * @{
 */

#include <stdbool.h>

/**
 * @brief Parse string to long long in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return long long Parsed number.
 */
long long convertNumberSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to long in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return long Parsed number.
 */
long convertLongSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to int in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return int Parsed number.
 */
int convertIntSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to short in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return short Parsed number.
 */
short convertShortSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to unsigned int in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return unsigned int Parsed number.
 */
unsigned int convertUnsignedIntSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to unsigned short in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return unsigned short Parsed number.
 */
unsigned short convertUnsignedShortSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to positive int in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return int Parsed number.
 */
int convertPositiveIntegerSafely(const char *timeString, bool *error);

/**
 * @brief Parse string to positive short in base 10
 *
 * @note This function is a thin wrapper around `strtol`
 *       for slightly easier/cleaner usage in this code base.
 *
 * @param timeString String representation of number.
 * @param error Reference to error flag. Is set to `true`,
 *              on error. Otherwise the variable is not touched!
 * @return short Parsed number.
 */
short convertPostiveShortSafely(const char *timeString, bool *error);

/** @} */ // end of group
#endif // NUMERIC_CONVERSIONS_H