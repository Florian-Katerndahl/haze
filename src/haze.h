#ifndef HAZE_H
#define HAZE_H

#include "types.h"

#include <stdio.h>
#include <gdal/gdal.h>

[[nodiscard]] struct rawData *allocateRawData(void);

void freeRawData(struct rawData *data);

[[nodiscard]] struct averagedData *allocateAverageData(void);

void freeAverageData(struct averagedData *data);

[[nodiscard]] GDALDatasetH openRaster(const char *filePath);

[[nodiscard]] GDALDatasetH openVector(const char *filePath);

void closeGDALDataset(GDALDatasetH dataset);

OGRLayerH  openVectorLayer(GDALDatasetH vector, const char *name);

GDALRasterBandH openRasterBand(GDALDatasetH raster, int index);

void readRasterDataset(GDALDatasetH raster, struct rawData **dataBuffer);

int averageRawData(const struct rawData *data, struct averagedData **average);

int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                 const size_t size, const size_t offset);

int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                    const size_t size, const size_t offset);

void reorderToPixelInterleave(struct rawData *data);

int getRasterMetadata(GDALDatasetH raster, struct geoTransform *geoTransformation);

bool isProjected(const char *Wkt);

[[nodiscard]] mean_t *calculateAreaWeightedMean(intersection_t *intersections, const char *rasterWkt);

void writeWeightedMeans(mean_t *values, const char *filePath);

void freeWeightedMeans(mean_t *list);

double coordinateFromCell(double origin, double axisOfInterest, double pixelExtent,
                          double complementaryAxis, double rotation);

#endif // HAZE_H