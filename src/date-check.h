#ifndef DATECHECK_H
#define DATECHECK_H

/**
 * @file date-check.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header files describes function signatures for checking date validity.
 * @version 0.1
 * @date 2026-04-27
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup date-check Date Validity Check
 * @{
 */

#include <stdbool.h>

/**
 * @brief Test if given combination of year, month and day is a valid date.
 *
 * @details This function tests whether a combination of year, month and date does represent
 *          a valid date by checking if the year is a value greater or equal to 0 the month
 *          is in the interval [1, 12] and the day is in the range from 1 to the corresponding
 *          number of maximum days per month (leap year adjusted).
 *
 * @note Not all intricacies of date checking are considered.
 *
 * @param year Value denoting year.
 * @param month Value denoting month.
 * @param day Value denoting day.
 * @return true Return true if input tuple represents a valid year.
 * @return false Return false if input tuple does not represent a valid year.
 */
bool isValidDate(int year, int month, int day);

/** @} */ // end of group
#endif