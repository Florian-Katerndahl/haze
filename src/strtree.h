#ifndef STRTREE_H
#define STRTREE_H

#include "aoi.h"
#include "haze.h"
#include "types.h"
#include <gdal/ogr_core.h>
#include <gdal/cpl_port.h>
#include <gdal/ogr_api.h>
#include <stdbool.h>
#include <stdio.h>
#include <geos_c.h>

#define TREE_NODE_CAP 100

/**
 * @brief
 *
 * @warning I don't fully understand this. Taken from https://stackoverflow.com/a/32334103.
 *          Has something to do with relative errors.
 * @param a
 * @param b
 * @return true
 * @return false
 */
bool nearEqual(double a, double b);

bool gte(double a, double b);

bool lte(double a, double b);

/**
 * @brief Determine if two rectangles/bounding boxes overlap in planar geometry
 *
 * @note Completely overlapping and 1pixel overlaps bounding boxes are considered to intersect each other.
 * @note The ordering of a and b does not matter in practice.
 *
 * @link https://stackoverflow.com/a/306332 @endlink
 *
 * @param a Foreground Bounding box
 * @param b Background bounding box
 * @return true
 * @return false
 */
bool intersect(const struct boundingBox *a, const struct boundingBox *b);

void freeVectorGeometry(struct vectorGeometry *node);

/**
 * @brief Return the dataset/layer CRS as WKT
 * @note The function takes ownership of the WKT string, i.e. the returned string is
 *       allocated with CPLStrdup or CPLMalloc and should be freed with CPLFree.
 * @param dataset
 * @param layerName In case of vector dataset, optional layer name. If not given,
 *                  the first layer is selected.
 * @return char* Pointer to WKT string or NULL on error
 */
char *extractCRSAsWKT(GDALDatasetH *dataset, const char *layerName);

int buildGEOSGeometriesFromGDAL(const char *filePath, const char *layerName,
                                vectorGeometryList **geometries, const char *inputReferenceSystem);

GEOSSTRtree *buildSTRTreefromRaster(const struct averagedData *data,
                                    const struct geoTransform *transformation, cellGeometryList **cells);

void freeCellGeometryList(cellGeometryList *list);

intersection_t *querySTRTree(vectorGeometryList *areasOfInterest, GEOSSTRtree *rasterTree);

#endif // STRTREE_H