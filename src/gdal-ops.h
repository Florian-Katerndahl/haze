#ifndef GDAL_OPS_H
#define GDAL_OPS_H
/**
 * @file gdal-ops.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures of general GDAL/OGR operations not exclusive to other submodules.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup gdal-ops General GDAL Operations
 * @{
 */

#include "types.h"
#include <gdal/gdal.h>
#include <gdal/ogr_api.h>
#include <sys/cdefs.h>

/**
 * @brief Open a raster dataset in read-only mode.
 *
 * @remark After the function returns, the caller owns the returned `GDALDatasetH` object and must close it after use.
 *
 * @param filePath Raster file to open.
 * @return GDALDatasetH GDAL handle to opened dataset, NULL on failure.
 */
[[nodiscard]] GDALDatasetH openRasterDataset(const char *filePath);

/**
 * @brief Fetch a raster band from an opened raster dataset.
 *
 * @note The raster band indices are 1-based.
 * @remark The returned raster band is owned by the corresponding raster dataset and should not be freed/destroyed.
 *
 * @param raster Raster object to fetch band from.
 * @param index Band index.
 * @return GDALRasterBandH GDAL handle to raster band, NULL on failure.
 */
GDALRasterBandH openRasterBand(GDALDatasetH raster, int index);

/**
 * @brief Open a vector dataset in read-only mode.
 *
 * @warning The GML and GMLLAS drivers are dregistered because it's explicitly mentioned they
 *          maybe not import coordinates in a "gis friendly" way!
 *
 * @remark After the function returns, the caller owns the returned `GDALDatasetH` object and must close it after use.
 *
 * @param filePath Vector file to open.
 * @return GDALDatasetH GDAL handle to opened dataset, NULL on failure.
 */
[[nodiscard]] GDALDatasetH openVectorDataset(const char *filePath);

/**
 * @brief Fetch a vector layer from an opened vector dataset.
 *
 * @note If `name` is null, the first vector layer is returned.
 * @remark The returned vector layer is owned by the corresponding vector dataset and should not be freed/destroyed.
 *
 * @param vector Vector object to fetch layer from.
 * @param name Layer name to fetch, possibly `NULL`.
 * @return OGRLayerH GDAL handle to vector layer, NULL on failure.
 */
OGRLayerH openVectorLayer(GDALDatasetH vector, const char *name);

/**
 * @brief Close an open raster or vector dataset.
 *
 * @note This function may call `exit(1)` if closing the `GDALDatasetH` object failed.
 *
 * @param dataset Object to close.
 */
void closeGDALDataset(GDALDatasetH dataset);

/**
 * @brief Get Geo-transformation information from opened raster dataset.
 *
 * @param raster Raster dataset to query.
 * @param geoTransformation Struct to store geotransformation values in.
 * @return int 0 on success, 1 on failure.
 *
 * @related struct geoTransfrom
 */
int getRasterMetadata(GDALDatasetH raster, struct geoTransform *geoTransformation);

/**
 * @brief Determine if a given CRS is geographic or planar/projected.
 *
 * @note If CRS type couldn't be fetched, CRS_UNKNOWN is returned indicating an error.
 *
 * @param Wkt String representation of CRS.
 * @return CRS_TYPE Type of CRS.
 */
CRS_TYPE getCRSType(const char *Wkt);

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
 * @brief Create a coordinate transformation between to CRS's
 *
 * @details This function creates `OGRSpatialReferenceH` objects from the supplied WKT representations.
 *          The transformation object is created using the `OCTNewCoordinateTransformationEx` function,
 *          setting optional arguments to NULL.
 *          The returned coordinate transformation object can be used with various transformation API's
 *          offered by GDAL.
 *
 * @remark The returned object is owned by the caller and must be freed/destroyed after use.
 *
 * @param from Source CRS as WKT string.
 * @param to Destination CRS as WKT string.
 * @param inputUsesCompliantOrdering Boolean flag indicating if the CRS associated with the object to transform
 *        uses authority compliant ordering or *gis friendly* ordering. When data is read from file, this is most
 *        likely `false`.
 * @return OGRCoordinateTransformationH A new transformation object, NULL on error.
 */
OGRCoordinateTransformationH transformationFromWKTs(char *from, char *to,
    bool inputUsesCompliantOrdering);

/**
 * @brief Convert an OGR geometry to a GEOS geometry
 *
 * @details This function creates a temporary GEOS WKB-reader and converts the input OGR geometry
 *          to a corresponding GEOS geometry by ex-/importing via the WKB interfaces.
 *
 * @note The C++-API of GDAL offers a direct interface to export OGR geometries to GEOS. Since
 *       GDAL's C-API does not offer such functionality, this workaround is needed.
 *
 * @note After the function returns, the caller owns the returned `GEOSGeometry` object and must free/destroy it after use.
 *
 * @warning This function is not reentrant or thread-safe!
 *
 * @param geom OGR geometry to convert.
 * @return GEOSGeometry* Converted geometry, NULL on error.
 */
[[nodiscard]] GEOSGeometry *OGRToGEOS(const OGRGeometryH geom);

/**
 * @brief Convert a GEOS geometry to an OGR geometry
 *
 * @details This function creates a temporary GEOS WKB-writer and converts the input GEOS geometry
 *          to a corresponding OGR geometry by ex-/importing via the respective WKB interfaces.
 *
 * @note After the function returns, the caller owns the returned `OGRGeometryH` object and must
 *       free/destroy it after use.
 *
 * @warning This function is not reentrant or thread-safe!
 *
 * @param geom GEOS geometry to convert.
 * @param crs Spatial reference object to assign to geometry, can be NULL.
 * @return OGRGeometryH Converted geometry, NULL on error.
 */
[[nodiscard]] OGRGeometryH OGRFromGEOS(const GEOSGeometry *geom, OGRSpatialReferenceH crs);

/** @} */ // end of group
#endif // GDAL_OPS_H
