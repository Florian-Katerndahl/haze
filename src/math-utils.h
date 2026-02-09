/**
 * @file math-utils.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures to process water vapor values.
 * @version 0.1
 * @date 2026-02-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdlib.h>

/**
 * @brief Compute water column height
 * 
 * @details ECMWF distributes data in unit of kilogram per square meter. To allow easy
 *          integration with FORCE, the values are converted to water column height.
 * 
 * @param x Water vapor concentration in kilogram per square meter.
 * @return double Scaled value.
 */
double kgsqmTocow(double x);

/**
 * @brief Compute the weighted average
 * 
 * @details Compute the weighted arithmetic mean of `count` amount of `values`. Every value
 *          must be associated with a weight.
 * 
 * \f[
 *      \bar{x} = \frac{\sum_{i = 1}^{\textrm{count}}x_i \times w_i}{\sum_{i = 1}^{\textrm{count}}w_i}
 * \f]
 * 
 * @param values Values to average.
 * @param weights Weights associated with values.
 * @param count Size of arrays.
 * @return double Weighted arithmetic mean, NAN on error.
 */
double calculateWeightedAverage(const double *values, const double *weights, size_t count);

#endif // MATH_UTILS_H