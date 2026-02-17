/**
 * @file strtree.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures to create STRTrees from various data sources and query them.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef STRTREE_H
#define STRTREE_H

#include "types.h"
#include <gdal/gdal.h>
#include <gdal/ogr_api.h>
#include <geos_c.h>

#define TREE_NODE_CAP 100

/**
 * @brief Return the dataset/layer CRS as WKT
 *
 * @details This function works on raster and vector datasets. It extracts the the projection
 *          of the dataset and returns a new reference to its WKT representation.
 *
 * @note After the function returns, the caller owns the returned `char *` object and must free/destroy it after use with `CPLFree`.
 *
 * @param dataset GDAL dataset object to query for CRS infromation.
 * @param layerName Layer name to query. Only honored for vector datasets.
 * @return char* Pointer to WKT string, NULL on error
 */
[[nodiscard]] char *extractCRSAsWKT(GDALDatasetH dataset, const char *layerName);

/**
 * @brief Create a linked list of GEOS geometries from an OGR-readable vector dataset
 *
 * @details This functions opens an OGR-readable vector dataset and extracts a single layer from it.
 *          Every feature of the extracted layer is exported to a GEOS geometry and possibly transformed
 *          if the layer CRS does not match the one provided in `inputReferenceSystem`.
 *          No attribute information of the layer features is stored/exported.
 *
 * @remark Geometries that don't describe an area or are not the strict 2D version are not allowed and return an error.
 *         It's assumed that only one geometry type is associated with each feature.
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use.
 *
 * @param filePath Path to vector dataset.
 * @param layerName Layer to extract. If NULL, the first layer will be used.
 * @param inputReferenceSystem Target CRS in WKT representation.
 * @return vectorGeometryList* Reference to list of GEOS geometries, NULL on error.
 */
[[nodiscard]] vectorGeometryList *buildGEOSGeometriesFromFile(const char *filePath,
    const char *layerName,
    const char *inputReferenceSystem);

/**
 * @brief Build a STRTree of vectorized raster cells and their values
 *
 * @details This function creates a new STRTree object and inserts vectorized raster
 *          cells and their values. The individual cells are also stored in the `cells`
 *          list because the STRTree does not allow to return references to inserted items.
 *          GDAL's geo transfromation information is used to derive vectorized cell
 *          geometries as per GDAL's documentation. Thus, this function should work well
 *          even with non north-up raster datasets as the rotation is honored.
 *
 * @note After the function returns, the caller owns the returned `GEOSTree` object and musst free it after use.
 *
 * @param data Averaged data.
 * @param transformation Extracted geo transfomation information used to create vectorized cells.
 * @param cells Indirect reference to linked list storing vectorzied geometries. Will not point to valid list on error.
 * @return GEOSSTRtree* Reference to STRTree, NULL on error
 */
[[nodiscard]] GEOSSTRtree *buildSTRTreefromRaster(const struct averagedData *data,
    const struct geoTransform *transformation, cellGeometryList **cells);

/**
 * @brief Callback used when querying STRTree
 *
 * @details Add a new entry to a linked list holding cell geometries. I.e. a list of vectorized
 *          raster cells for a given query polygon.
 *
 * @param item void-casted `cellGeometry` object whose MBR intersects with MBR of query polygon.
 * @param userdata void-casted reference to `userdata_t`.
 */
void trackIntersectingGeometries(void *item, void *userdata);

/**
 * @brief Query STRTree with MBRs of "extraction" geometries
 *
 * @details This function iterates over all geometries stored in `areaOfInterest` and queries the
 *          previously created STRTree, consisting of vectorized raster cells, for intersections.
 *          Any intersecting cells are added to a list and may be used to calculate area weighted
 *          means of total water column.
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use.
 *
 * @param areasOfInterest List of "overlay" geometries used to query STRTree.
 * @param rasterTree STRTree of vectorized raster cells.
 * @return intersection_t* Reference to list connecting "overlay" geometries to intersecting vectorized raster cells.
 */
[[nodiscard]] intersection_t *querySTRTree(vectorGeometryList *areasOfInterest,
    GEOSSTRtree *rasterTree);

/**
 * @brief Convert the MBR of an OGR geometry to a GEOS geometry
 *
 * @note After the function returns, the caller owns the returned `GEOSGeometry` object and must free/destroy it after use.
 *
 * @param geom OGR geometry whose MBR should be converted.
 * @return GEOSGeometry* MBR of input geometry.
 */
[[nodiscard]] GEOSGeometry *boundingBoxOfOGRToGEOS(const OGRGeometryH geom);

/**
 * @brief Convert an OGR geometry to a GEOS geometry
 *
 * @details This function creates a temporary GEOS WKB-reader and converts the input OGR geometry
 *          to a corresponding GEOS geometry by ex-/importing via the WKB interfaces.
 *
 * @note The C++-API of GDAL offers a direct interface to export OGR geometries to GEOS. Since
 *       GDAL's C-API does not offer such functionality, this workaround is needed.
 * @note After the function returns, the caller owns the returned `GEOSGeometry` object and must free/destroy it after use.
 *
 * @param geom OGR geometry to convert.
 * @return GEOSGeometry* Converted geometry, NULL on error.
 */
[[nodiscard]] GEOSGeometry *OGRToGEOS(const OGRGeometryH geom);

#endif // STRTREE_H