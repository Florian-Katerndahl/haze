#ifndef HAZE_H
#define HAZE_H

#define _DEFAULT_SOURCE

#include "types.h"

#include <stdio.h>
#include <gdal/gdal.h>

[[nodiscard]] struct rawData *allocateRawData(void);

void freeRawData(struct rawData *data);

[[nodiscard]] struct averagedData *allocateAverageData(void);

void freeAverageData(struct averagedData *data);

void readRasterDataset(GDALDatasetH raster, struct rawData **dataBuffer);

int averageRawData(const struct rawData *data, struct averagedData **average);

int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                 const size_t size, const size_t offset);

int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                    const size_t size, const size_t offset);

void reorderToPixelInterleave(struct rawData *data);

[[nodiscard]] mean_t *calculateAreaWeightedMean(intersection_t *intersections,
    const char *rasterWkt);

void writeWeightedMeans(mean_t *values, const char *filePath);

void freeWeightedMeans(mean_t *list);

double coordinateFromCell(double origin, double axisOfInterest, double pixelExtent,
                          double complementaryAxis, double rotation);

int processDaily(const option_t *options);

#endif // HAZE_H