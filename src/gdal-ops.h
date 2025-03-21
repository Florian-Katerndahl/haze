/* Functions related to operations on GDAL datasets etc. that are not exclusive to submodules */
#ifndef GDAL_OPS_H
#define GDAL_OPS_H

#include "types.h"
#include <gdal/gdal.h>

[[nodiscard]] GDALDatasetH openRaster(const char *filePath);

GDALRasterBandH openRasterBand(GDALDatasetH raster, int index);

[[nodiscard]] GDALDatasetH openVector(const char *filePath);

OGRLayerH  openVectorLayer(GDALDatasetH vector, const char *name);

void closeGDALDataset(GDALDatasetH dataset);

int getRasterMetadata(GDALDatasetH raster, struct geoTransform *geoTransformation);

bool isProjected(const char *Wkt);

#endif // GDAL_OPS_H