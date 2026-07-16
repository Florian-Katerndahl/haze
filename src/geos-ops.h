#ifndef GEOSOPS_H
#define GEOSOPS_H

/**
 * @file geos-ops.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures for general GEOS operations.
 * @version 0.1
 * @date 2026-05-04
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup geos-ops General GEOS Operations
 * @{
 */

#include <stdio.h>
#include <stdarg.h>

/**
 * @brief Print Messages from GEOS to stderr
 *
 * @param fmt Message format string
 * @param ... printf-like objects and references to fill
 *            format string
 */
void geosMessagesToStderr(const char *fmt, ...);

/** @} */ // end of group
#endif // GEOSOPS_H