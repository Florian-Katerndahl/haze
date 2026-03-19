#ifndef HAZE_H
#define HAZE_H
/**
 * @file haze.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures for the central processing part of water vapor information from ECMWF's ERA-5 data.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup haze Water Vapor Processing
 * @{
 */

#define _DEFAULT_SOURCE

#include "types.h"

#include <stdio.h>
#include <gdal/gdal.h>

/**
 * @brief Free encapsulated fields of raw data struct
 *
 * @param data Object to free.
 */
void freeRawData(struct rawData *data);

/**
 * @brief Free encapsulated fields of averaged data struct
 *
 * @param data Object to free.
 */
void freeAverageData(struct averagedData *data);

/**
 * @brief Read all bands of an GDAL raster dataset into a buffer
 *
 * @note This function returns an error if inputs are not of type double/GDT_FLOAT64.
 *
 * @note After the function returns, the dataBuffer object contains a heap-allocated buffer
 *       and the caller musst free it after use.
 *
 * @param raster Opened raster dataset.
 * @param dataBuffer Reference to raw data buffer.
 * @return 0 on success, 1 on error.
 */
int readRasterDataset(GDALDatasetH raster, struct rawData *dataBuffer);

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
int averageRawData(const struct rawData *data, struct averagedData *average);

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
int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData *average,
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
int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData *average,
                                    const size_t size, const size_t offset);

/**
 * @brief Transpose data tensor from band sequential to band interleaved by pixel
 *
 * @param data Reference to structure holding data.
 * @return int 0 on success, 1 on error.
 */
int reorderToBandInterleavedByPixel(struct rawData *data);

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
 * @return in 0 on success, 1 on error.
 */
int writeWeightedMeans(mean_t *values, const char *filePath);

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

/**
 * @brief Parse logfile for processing
 *
 * @details This function reads lines of the log file sequentially, extracting the
 *          the file path of respective ERA-5 dataset and processing status. It's
 *          assuming the format mentioned below.
 *          The order of lines in the log file are preserved in the returned linked list
 *          to keep diffs between program runs minimal.
 *
 * @note The log file's format is "<file path>\tSTATUS".
 *
 * @param filePath File path to logfile.
 * @return Reference to linked list storing one line per node, possibly NULL on error.
 */
stringList *parseLogFile(const char *filePath);

/**
 * @brief Update logfile with new dataset statuses
 *
 * @note The log file's format is "<file path>\tSTATUS".
 *
 * @param list Linked list storing one line per node with file path of ERA-5 dataset and processing status.
 * @param filePath File path to logfile.
 * @return int 0 on success, 1 on error.
 */
int writeUpdatedLogFile(stringList *list, const char *filePath);

/**
 * @brief Deduce temporal information from a given dataset
 *
 * @details This function deduces temoporal information (contained years, months, days and hours)
 *          of a given dataest by reading the metadata from all bands and counting observed
 *          values.
 *
 * @note The options struct is manipulated by this function; assumes files contain only a single
 *       year and a single month, thus only storing arbitrary days and hours combinations.
 *
 * @param options Reference to parsed options, without temoporal information fields set.
 * @param dataset Opened GDAL dataset, from which temoporal information should be read.
 * @return 0 on success, 1 on error.
 */
int backFillOptions(option_t *options, GDALDatasetH dataset);

/**
 * @brief Main procedure to process downloaded ERA-5 datasets
 *
 * @details This function implements processing ERA-5 datasets to a water vapor database usable by FORCE.
 *          Each unprocessed dataset is scanned to deduce temporal information stored in it, averaged on
 *          a daily basis, vectorized and stored in a STRTree.
 *          The supplied vector dataset containing the area of interest is converted to GEOS geometries
 *          once and possibly reprojected to EPSG:4326.
 *          Intersections between vectorized ERA-5 data and AOI are used to compute area-weighted
 *          mean of water vapor, whereby a single geometry entry in the AOI is used to compute weight values.
 *
 * @note The SRS of input files is hardcoded to EPSG:4326 as ECMWF is aligned to it
 *       horizontally. Should this change in the future, this procedure would need to
 *       be updated.
 *
 * @param options Reference to parsed options struct.
 * @return 0 on success, 1 on error.
 */

int process(option_t *options);

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

/** @} */ // end of group
#endif // HAZE_H
