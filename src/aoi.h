#ifndef AOI_H
#define AOI_H
/**
 * @file aoi.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures for bounding box generation and display.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup aoi Area Of Interest Generation
 * @{
 */

#include <gdal/ogr_core.h>

/**
 * @brief Compute the minimum bounding rectangle of the first or a specified layer and transform to EPSG:4326, if needed.
 *        All OGR-readable files and geometries are allowed.
 *
 * @note After the function returns, the caller owns the returned `OGREnvelope` object and musst free it after use with `CPLFree`.
 *
 * @param filePath Path to file holding area of interest
 * @param layerName Layer to use for bounding box creation. If NULL, use the first layer.
 * @return OGREnvelope* Reference to bounding box, NULL on error.
 */
[[nodiscard]] OGREnvelope *boxFromPath(const char *filePath, const char *layerName);

/**
 * @brief Print the top, right, bottom and left coordinates defining a bounding box to stdout.
 *
 * @param box Bounding box whose coordinates should be printed.
 */
void printBoundingBox(const OGREnvelope *box);

/** @} */ // end of group
#endif // AOI_H