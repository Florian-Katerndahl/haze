/**
 * @file gdal-ops.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures of general GDAL/OGR operations not exclusive to other submodules.
 * @version 0.1
 * @date 2026-02-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef GDAL_OPS_H
#define GDAL_OPS_H

#include "types.h"
#include <gdal/gdal.h>
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
 * @note This function may call `exit(-1)` if no spatial reference could be determined.
 * 
 * @param Wkt String representation of CRS.
 * @return true Return true if dataset is associated with geographic CRS.
 * @return false Return false if dataset is associated with planar/projected CRS.
 */
bool isGeographic(const char *Wkt);

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
 * @return OGRCoordinateTransformationH A new transformation object, NULL on error.
 */
OGRCoordinateTransformationH transformationFromWKTs(const char *from, const char *to);

#endif // GDAL_OPS_H