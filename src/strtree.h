#ifndef STRTREE_H
#define STRTREE_H

#include "types.h"
#include <gdal/gdal.h>
#include <gdal/ogr_api.h>
#include <geos_c.h>

#define TREE_NODE_CAP 100

/**
 * @brief Return the dataset/layer CRS as WKT
 * @note The function takes ownership of the WKT string, i.e. the returned string is
 *       allocated with CPLStrdup or CPLMalloc and should be freed with CPLFree.
 * @param dataset
 * @param layerName In case of vector dataset, optional layer name. If not given,
 *                  the first layer is selected.
 * @return char* Pointer to WKT string or NULL on error
 */
[[nodiscard]] char *extractCRSAsWKT(GDALDatasetH dataset, const char *layerName);

[[nodiscard]] vectorGeometryList *buildGEOSGeometriesFromFile(const char *filePath,
    const char *layerName,
    const char *inputReferenceSystem);

[[nodiscard]] GEOSSTRtree *buildSTRTreefromRaster(const struct averagedData *data,
    const struct geoTransform *transformation, cellGeometryList **cells);

[[nodiscard]] intersection_t *querySTRTree(vectorGeometryList *areasOfInterest,
    GEOSSTRtree *rasterTree);

[[nodiscard]] GEOSGeometry *boundingBoxOfOGRToGEOS(const OGRGeometryH geom);

[[nodiscard]] GEOSGeometry *OGRToGEOS(const OGRGeometryH geom);

#endif // STRTREE_H