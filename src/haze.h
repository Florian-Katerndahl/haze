/**
 * @file haze.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures for the central processing part of water vapor information from ECMWF's ERA-5 data.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef HAZE_H
#define HAZE_H

#define _DEFAULT_SOURCE

#include "types.h"

#include <stdio.h>
#include <gdal/gdal.h>

/**
 * @brief Allocate a raw data object and encapsulated fields on the heap
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use.
 *
 * @return struct rawData* Raw data object, NULL on error.
 */
[[nodiscard]] struct rawData *allocateRawData(void);

/**
 * @brief Free a raw data object and encapsulated fields
 *
 * @param data Object to free.
 */
void freeRawData(struct rawData *data);

/**
 * @brief Allocate an averaged data object and encapsulated fields on the heap
 *
 * @note After the function returns, the caller owns the returned object and musst free it after use.
 *
 * @return struct averagedData* Averaged data object, NULL on error.
 */
[[nodiscard]] struct averagedData *allocateAverageData(void);

/**
 * @brief Free an averaged data object and encapsulated fields
 *
 * @param data Object to free.
 */
void freeAverageData(struct averagedData *data);

/**
 * @brief Read all bands of an GDAL raster dataset into a buffer
 *
 * @note This function returns an error if inputs are not of type double/GDT_FLOAT64.
 *
 * @note After the function returns, the caller owns the dataBuffer object and musst free it after use.
 *
 * @param raster Opened raster dataset.
 * @param dataBuffer Indirect reference to raw data buffer, *dataBuffer is NULL on error.
 */
void readRasterDataset(GDALDatasetH raster, struct rawData **dataBuffer);

/**
 * @brief Compute arithmetic mean pixel values across raster band dimension for all bands
 *
 * @note Assumes data is ordered as band -> column -> row (see internal offset calculations).
 *
 * @note After the function returns, the caller owns the dataBuffer object and musst free it after use.
 *
 * @param data Raw data struct.
 * @param average Averaged data struct to store data in.
 * @return int 0 on success, 1 on error.
 */
int averageRawData(const struct rawData *data, struct averagedData **average);

/**
 * @brief Compute arithmetic mean pixel values across raster band dimension for a subset of bands
 *
 * @details This function allows to average a subset of bands pointed to by `data` by
 *          starting from `offset` up to but not including `offset + size`.
 *
 * @note Assumes data is ordered as band -> column -> row, i.e. band sequential.
 *
 * @note After the function returns, the caller owns the dataBuffer object and musst free it after use.
 *
 * @param data Reference to structure holding data.
 * @param average Indirect reference to structure where averaged values are stored.
 * @param size Size of window to use for arithmetic mean calculation.
 * @param offset Starting band.
 * @return int 0 on success, 1 on error.
 */
int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                 const size_t size, const size_t offset);

/**
 * @brief Compute arithmetic mean pixel values across raster band dimension for a subset of bands
 *
 * @details This function allows to average a subset of bands pointed to by `data` by
 *          starting from `offset` up to but not including `offset + size`.
 *
 * @note Assumes data is ordered as column -> row -> band, i.e. band interleaved by pixel.
 *
 * @note After the function returns, the caller owns the dataBuffer object and musst free it after use.
 *
 * @param data Reference to structure holding data.
 * @param average Indirect reference to structure where averaged values are stored.
 * @param size Size of window to use for arithmetic mean calculation.
 * @param offset Starting band.
 * @return int 0 on success, 1 on error.
 */
int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                    const size_t size, const size_t offset);

/**
 * @brief Transpose data tensor from band sequential to band interleaved by pixel
 *
 * @param data Reference to structure holding data.
 */
void reorderToBandInterleavedByPixel(struct rawData *data);

/**
 * @brief Compute area weighted mean for features of AOI dataset
 *
 * @details This functions iterates over all features in `intersections`, computes their centroid
 *          coordinates and, depending on the version of GDAL used, the geographic or planar area.
 *          All intersecting geometries extracted from an STRTree are converted from GEOS geometries
 *          to OGR geometries via the WKB import/export interface.
 *          The actual intersection is performed without regards to underlying CRS, though the newly
 *          created polygon is assigned the spatial reference derived from `rasterWkt`. The error introduced
 *          by assuming planar geometries should be small. Area calculation is performed, depending on the
 *          version of GDAL used, differently depending of the CRS type (geographic vs. planar).
 *          A weight equal to the fractional cover of the intersecting geometry to the AOI feature is
 *          computed and used to calculate an area weighted arithmetic mean.
 *
 * @note With GDAL >= 3.9.0, area calculation of geographic coordinates is correct. Otherwise,
 *       planar geometries are assumed.
 *
 * @note Intersection of geometries is performed assuming planar geometries in all cases.
 *
 * @note After the function returns, the caller owns the returned object and musst free it.
 *
 * @param intersections List containing AOI features and all vectorized raster cells that intersect a given feature.
 * @param rasterWkt CRS in WKT representation of raster dataset.
 * @return mean_t* Reference to list containing centroids of AOI geometries and associated water column value, NULL on error.
 */
[[nodiscard]] mean_t *calculateAreaWeightedMean(intersection_t *intersections,
    const char *rasterWkt);

/**
 * @brief Write area weighted means to file in format usable by FORCE
 *
 * @param values List containing centroids of AOI geometries and associated water column value.
 * @param filePath Path to output file.
 */
void writeWeightedMeans(mean_t *values, const char *filePath);

/**
 * @brief Free list containing centroids of AOI geometries and associated water column value
 *
 * @param list List to free
 */
void freeWeightedMeans(mean_t *list);

/**
 * @brief Compute corner coordinates of raster cell based on geo transformation
 *
 * @details Computes the x/y coordinate given pixel/line index and a geo transformation.
 *          The axis of interest refers to the axis whose coordinate should be computed and
 *          influences which coordinate is used as the origin. I.e. to compute the x coordinate
 *          of the upper-left pixel, the origin must refer to x coordinate, the axis of interest
 *          to the columnn index 0, the pixel extent to the width of the pixel, the complementary
 *          axis to the row index and the rotation to the row rotation.
 *
 * @note GDAL uses the upper-left corner of a pixel as its origin, mapping the upper-left
 *       pixel to cell (0, 0). To get the center coordinate, one would need to use (0.5, 0.5)
 *
 * @param origin Origin coordinate of upper-left pixel in direction of requested coordinate dimension.
 * @param axisOfInterest Index in direction of requested coordinate dimension.
 * @param pixelExtent Pixel extent in CRS units in direction of requested coordinate dimension.
 * @param complementaryAxis Index in direction of complementary coordinate dimension.
 * @param rotation Rotation of complementary axis.
 * @return double Georeferenced coordinate value in direction of requested coordinate dimension.
 */
double coordinateFromCell(double origin, double axisOfInterest, double pixelExtent,
                          double complementaryAxis, double rotation);

int processDaily(stringList *successfulDownloads, const option_t *options);

/**
 * @brief Test if given combination of year, month and day is a valid date.
 *
 * @details This function tests whether a combination of year, month and date does represent
 *          a valid date by checking if the year is a value greater or equal to 0 the month
 *          is in the interval [1, 12] and the day is in the range from 1 to the corresponding
 *          number of maximum days per month (leap year adjusted).
 *
 * @note Not all intricacies of date checking are considered.
 *
 * @param year Value denoting year.
 * @param month Value denoting month.
 * @param day Value denoting day.
 * @return true Return true if input tuple represents a valid year.
 * @return false Return false if input tuple does not represent a valid year.
 */
bool isValidDate(int year, int month, int day);

#endif // HAZE_H