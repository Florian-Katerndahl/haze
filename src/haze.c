#include "haze.h"
#include "types.h"
#include "gdal-ops.h"
#include "math-utils.h"
#include "strtree.h"
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <bits/posix2_lim.h>
#include <gdal/cpl_conv.h>
#include <geos_c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gdal/gdal.h>
#include <gdal/gdal_version.h>
#include <gdal/cpl_error.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <gdal/ogr_api.h>
#include <unistd.h>

[[nodiscard]] struct rawData *allocateRawData(void)
{
  struct rawData *dataBuffer = calloc(1, sizeof(struct rawData));
  if (dataBuffer == NULL) {
    perror("calloc");
    return NULL;
  }
  dataBuffer->data = calloc(1, sizeof(double **));
  if (dataBuffer->data == NULL) {
    perror("calloc");
    free(dataBuffer);
    return NULL;
  }

  return dataBuffer;
}

void freeRawData(struct rawData *data)
{
  free(*data->data);
  free(data->data);
  free(data);
}

[[nodiscard]] struct averagedData *allocateAverageData(void)
{
  struct averagedData *averageBuffer = calloc(1, sizeof(struct averagedData));
  if (averageBuffer == NULL) {
    perror("calloc");
    return NULL;
  }

  averageBuffer->data = calloc(1, sizeof(double **));
  if (averageBuffer->data == NULL) {
    perror("calloc");
    free(averageBuffer);
    return NULL;
  }

  return averageBuffer;
}

void freeAverageData(struct averagedData *data)
{
  free(*data->data);
  free(data->data);
  free(data);
}

void readRasterDataset(GDALDatasetH raster, struct rawData **dataBuffer)
{
  *dataBuffer = allocateRawData();
  if (dataBuffer == NULL) {
    return;
  }

  int dataSetBandCount = GDALGetRasterCount(raster);
  GDALRasterBandH layer = openRasterBand(raster, 1);
  if (layer == NULL) {
    *dataBuffer = NULL;
    return;
  }
  GDALDataType dType    = GDALGetRasterDataType(layer);
  assert(dType == GDT_Float64);
  size_t byteSize       = (size_t) GDALGetDataTypeSizeBytes(dType);
  assert(sizeof(double) == byteSize);
  size_t datasetColumns = (size_t) GDALGetRasterBandXSize(layer);
  size_t datasetRows    = (size_t) GDALGetRasterBandYSize(layer);

  (*dataBuffer)->bands = dataSetBandCount;
  (*dataBuffer)->columns = datasetColumns;
  (*dataBuffer)->rows = datasetRows;

  *(*dataBuffer)->data = calloc(datasetRows * datasetColumns * dataSetBandCount, sizeof(double));
  if (*dataBuffer == NULL) {
    perror("calloc");
    return;
  }

  // data seems to be BSQ? Or at least it saved into the buffer one scanline at a time
  //  could be helpful to manually transform to PIL
  CPLErr readErr = GDALDatasetRasterIOEx(
                     raster, GF_Read, 0, 0,
                     (int) datasetColumns, (int) datasetRows,
                     (void *) * (*dataBuffer)->data, (int) datasetColumns,
                     (int) datasetRows, dType,
                     dataSetBandCount, NULL, 0, 0, 0, NULL);

  if (readErr == CE_Failure) {
    fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
    free(*dataBuffer);
    *dataBuffer = NULL;
    return;
  }

  return;
}

int averageRawData(const struct rawData *data, struct averagedData **average)
{
  *average = allocateAverageData();
  if (*average == NULL) {
    return 1;
  }

  (*average)->columns = data->columns;
  (*average)->rows = data-> rows;

  *(*average)->data = calloc(data->rows * data->columns, sizeof(double));
  if ((*average)->data == NULL) {
    perror("calloc");
    return 1;
  }

  double sum;
  size_t xOffset;
  size_t yOffset;
  size_t bandOffset;
  double bandAsDouble = (double) data->bands;
  for (size_t row = 0; row < data->rows; row++) { // y
    yOffset = row * data->columns;
    for (size_t column = 0; column < data->columns; column++) { // x
      xOffset = column;
      sum = 0.0;
      for (size_t band = 0; band < data->bands; band++) { // band
        bandOffset = band * data->columns * data->rows;
        sum += *(*data->data + yOffset + xOffset + bandOffset);
      }
      *(*(*average)->data + xOffset + yOffset) = sum / bandAsDouble;
    }
  }
  return 0;
}

// FIXME: What was the offset?
int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                 const size_t size, const size_t offset)
{
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * `day of interest (0-based)`
  size_t startBand = offset;
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + offset;
  assert(startBand < data->bands);
  assert(boundary <= data->bands);

  *average = allocateAverageData();
  if (*average == NULL) {
    return 1;
  }

  (*average)->columns = data->columns;
  (*average)->rows = data-> rows;

  *(*average)->data = calloc(data->rows * data->columns, sizeof(double));
  if ((*average)->data == NULL) {
    perror("calloc");
    return 1;
  }

  double sum;
  size_t xOffset;
  size_t yOffset;
  size_t bandOffset;
  double bandAsDouble = (double) data->bands;
  for (size_t y = 0; y < data->rows; y++) {
    yOffset = y * data->columns;
    for (size_t x = 0; x < data->columns; x++) {
      xOffset = x;
      sum = 0.0;
      for (size_t band = startBand; band < boundary; band++) { // band
        bandOffset = band * data->columns * data->rows;
        sum += *(*data->data + yOffset + xOffset + bandOffset);
      }
      *(*(*average)->data + xOffset + yOffset) = sum / bandAsDouble;
    }
  }
  return 0;
}

int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                    const size_t size, const size_t offset)
{
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * `day of interest (0-based)`
  size_t startBand = offset;
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + offset;
  assert(startBand < data->bands);
  assert(boundary <= data->bands);

  *average = allocateAverageData();
  if (*average == NULL) {
    return 1;
  }

  (*average)->columns = data->columns;
  (*average)->rows = data-> rows;

  *(*average)->data = calloc(data->rows * data->columns, sizeof(double));
  if ((*average)->data == NULL) {
    perror("calloc");
    return 1;
  }

  double sum;
  size_t xOffset;
  size_t yOffset;
  size_t bandOffset;
  double bandAsDouble = (double) data->bands;
  for (size_t y = 0; y < data->rows; y++) {
    yOffset = y * data->columns * data->bands;
    for (size_t x = 0; x < data->columns; x++) {
      xOffset = x * data->bands;
      sum = 0.0;
      for (size_t band = startBand; band < boundary; band++) { // band
        bandOffset = band;
        sum += *(*data->data + yOffset + xOffset + bandOffset);
      }
      *(*(*average)->data + x + y * data->columns) = sum / bandAsDouble;
    }
  }
  return 0;
}

void reorderToPixelInterleave(struct rawData *data)
{
  // hmm, actually unsure if this is beneficial; page faults are not lower with two passes; probably needs testing with longer inputs to fully conclude if this is worth it
  // it becomes apparent however, that IO is THE bottleneck as execution differs only slightly with re-ordered data being 2 seconds faster (probably hot cache paths..)
  // cache misses grew marginally slower for PIL data
  // BUT without runtime sanitizers and O3: 5 major page faults vs 333 and 152232 minor page faults vs 148721 -> could be worth it for multiple calculations on same data
  double *temporaryArray = calloc(data->columns * data->rows * data->bands, sizeof(double));
  if (temporaryArray == NULL) {
    perror("calloc");
    return;
  }

  size_t originXOffset;
  size_t targetXOffset;
  size_t originYOffset;
  size_t targetYOffset;
  size_t originBandOffset;
  size_t targetBandOffset;
  for (size_t y = 0; y < data->rows; y++) {
    originYOffset = y * data->columns;
    targetYOffset = y * data->columns * data->bands;
    for (size_t x = 0; x < data->columns; x++) {
      originXOffset = x;
      targetXOffset = x * data->bands;
      for (size_t band = 0; band < data->bands; band++) {
        originBandOffset = band * data->columns * data->rows;
        targetBandOffset = band;
        *(temporaryArray + targetBandOffset + targetXOffset + targetYOffset) = *
          (*data->data + originYOffset + originXOffset + originBandOffset);
      }
    }
  }

  double *temp = *data->data;
  *data->data = temporaryArray;
  free(temp);

  return;
}

[[nodiscard]] mean_t *calculateAreaWeightedMean(intersection_t *intersections,
    const char *rasterWkt)
{
  mean_t *root = NULL;

  OGRSpatialReferenceH  spatialRef = OSRNewSpatialReference(rasterWkt);
  if (spatialRef == NULL) {
    fprintf(stderr, "Could not determine if CRS is projected or not");
    return NULL;
  }

#if GDAL_VERSION_NUM >= 3090000
  const bool geographic = isGeographic(rasterWkt);
#endif

  size_t wkbSize;
  GEOSWKBWriter *wkbWriter = GEOSWKBWriter_create();
  if (wkbWriter == NULL) {
    fprintf(stderr, "Failed to create WKB writer\n");
    free(root);
    return NULL;
  }

#if GDAL_VERSION_NUM < 3090000
  fprintf(stderr, "Calcluating cartesian area regardless of projection\n");
#endif

  while (intersections != NULL) {
    OGRGeometryH centroid = OGR_G_CreateGeometry(wkbPoint);
    if (centroid == NULL) {
      fprintf(stderr, "Failed to create empty centroid\n");
      continue; // is continue really appropriate here?
    }

    if (OGR_G_Centroid(intersections->reference, centroid) == OGRERR_FAILURE) {
      fprintf(stderr, "Failed to calculate centroid\n");
      OGR_G_DestroyGeometry(centroid);
      continue; // is continue really appropriate here?
    }

#if GDAL_VERSION_NUM < 3090000
    double referenceArea = OGR_G_Area(intersections->reference);
#else
    double referenceArea = geographic ? OGR_G_GeodesicArea(intersections->reference) : OGR_G_Area(
                             intersections->reference);
#endif
    if (referenceArea == -1) {
      fprintf(stderr, "Failed to calculate reference area\n");
      OGR_G_DestroyGeometry(centroid);
      continue; // is continue really appropriate here?
    }

    double *values = calloc(intersections->intersectionCount, sizeof(double));
    if (values == NULL) {
      perror("calloc");
      OGR_G_DestroyGeometry(centroid);
      continue; // is continue really appropriate here?
    }

    double *weights = calloc(intersections->intersectionCount, sizeof(double));
    if (weights == NULL) {
      perror("calloc");
      OGR_G_DestroyGeometry(centroid);
      continue; // is continue really appropriate here?
    }

    cellGeometryList *temp = intersections->intersectingCells;

    // iterate over all found intersections
    for (size_t i = 0; i < intersections->intersectionCount; i++) {
      values[i] = temp->entry->value; // shit, here I do copy data again...

      OGRGeometryH cellAsOGR = OGR_G_CreateGeometry(wkbPolygon);
      OGR_G_AssignSpatialReference(cellAsOGR,
                                   spatialRef); // todo: should spat ref be assigned AFTER import?

      const unsigned char *geometryAsWkb = GEOSWKBWriter_write(wkbWriter, temp->entry->geometry,
                                           &wkbSize);
      if (geometryAsWkb == NULL) {
        fprintf(stderr, "Failed to export geometry as WKB\n");
        // todo cleanup
        continue; // is continue really appropriate here?
      }

      if (OGR_G_ImportFromWkb(cellAsOGR, (void *) geometryAsWkb, (int) wkbSize) != OGRERR_NONE) {
        fprintf(stderr, "Failed to import WKB to OGR: %s\n", CPLGetLastErrorMsg());
        // todo cleanup
        continue; // is continue really appropriate here?
      }

      OGRGeometryH intersection = OGR_G_Intersection(intersections->reference, cellAsOGR);
      if (intersection == NULL) {
        fprintf(stderr, "Failed to create intersection-polygon: %s\n", CPLGetLastErrorMsg());
        // todo cleanup
        continue; // is continue really appropriate here?
      }
      OGR_G_AssignSpatialReference(intersection, spatialRef);

#if GDAL_VERSION_NUM < 3090000
      double intersectingArea = OGR_G_Area(intersection);
#else
      double intersectingArea = geographic ? OGR_G_GeodesicArea(intersection) : OGR_G_Area(intersection);
#endif

      weights[i] = intersectingArea / referenceArea;

      temp = temp->next;

      GEOSFree((void *) geometryAsWkb);
      OGR_G_DestroyGeometry(intersection);
      OGR_G_DestroyGeometry(cellAsOGR);
    }

    mean_t *meanEntry = calloc(1, sizeof(mean_t));
    if (meanEntry == NULL) {
      perror("calloc");
      // todo cleanup
      free(values);
      free(weights);
      intersections = intersections->next;
      continue; // is continue really appropriate here?
    }

    meanEntry->value = calculateWeightedAverage(values, weights, intersections->intersectionCount);
    meanEntry->x = OGR_G_GetX(centroid, 0);
    meanEntry->y = OGR_G_GetY(centroid, 0);

    if (root == NULL) {
      root = meanEntry;
    } else {
      meanEntry->next = root;
      root = meanEntry;
    }

    intersections = intersections->next;

    free(values);
    free(weights);
    OGR_G_DestroyGeometry(centroid);
  }

  GEOSWKBWriter_destroy(wkbWriter);
  OSRDestroySpatialReference(spatialRef);

  return root;
}

void writeWeightedMeans(mean_t *values, const char *filePath)
{
  assert(values && filePath);

  FILE *outFile = fopen(filePath, "wt");
  if (outFile == NULL) {
    perror("fopen");
    return;
  }

  while (values != NULL) {
    fprintf(
      outFile,
      "%.4lf %.4lf %f ERA\n",
      values->x, values->y, (float) kgsqmTocow(values->value));

    values = values->next;
  }

  fclose(outFile);
  return;
}

void freeWeightedMeans(mean_t *list)
{
  mean_t *next;
  while (list != NULL) {
    next = list->next;
    free(list);
    list = next;
  }
}

double coordinateFromCell(double origin, double axisOfInterest, double pixelExtent,
                          double complementaryAxis, double rotation)
{
  // axisOfInterest is the axis for which coordinate should be caclulated
  return origin + axisOfInterest * pixelExtent + complementaryAxis * rotation;
}

int processDaily(stringList *successfulDownloads, const option_t *options)
{
  while (successfulDownloads != NULL) {
    GDALDatasetH ds = openRaster(successfulDownloads->string);

    if (ds == NULL)
      return -1;

    const char *rasterWkt = extractCRSAsWKT(ds, NULL);

    vectorGeometryList *areasOfInterest = buildGEOSGeometriesFromFile(options->areaOfInterest, NULL,
                                          rasterWkt);

    struct rawData *data = NULL;
    readRasterDataset(ds, &data);
    struct geoTransform transform = {0};
    getRasterMetadata(ds, &transform);
    closeGDALDataset(ds);

    size_t hoursPerDay = countRequestedHours(options);
    size_t processedDays = 0;

    // TODO: now that monthly images are downloaded, I need to loop over days in here
    //  while making sure that I don't over access data for different months (30 days or 28/29 vs 31 max)
    for (int *day = options->days; *day != INITVAL; day++) {

      int currentYear;
      int currentMonth;

      const char *inputFileName = strrchr(successfulDownloads->string, '/');
      if (inputFileName == NULL) {
        fprintf(stderr, "Malformed file path. Could not get final path delimiter\n");
        // todo cleanup
        continue;
      }

      if (sscanf(inputFileName, "/%d-%d.grib", &currentYear, &currentMonth) != 2) {
        fprintf(stderr, "Failed to extract year and month from file name\n");
        // todo cleanup
        continue;
      }

      if (!isValidDate(currentYear, currentMonth, *day)) {
        // check if month has enough days to access current *day, if not we're done processing and can continue
        // no logging because this can happen routinely
        // todo cleanup
        continue;
      }

      struct averagedData *average = NULL;

      averageRawDataWithSizeOffset(data, &average, hoursPerDay, processedDays * hoursPerDay);

      cellGeometryList *rasterCellsAsGEOS = NULL; // todo: document that this should be freed!

      // todo: optionally implement function to crop raster beforehand?

      GEOSSTRtree *rasterTree = buildSTRTreefromRaster(average, &transform, &rasterCellsAsGEOS);

      // a function to query the tree constructed by buildSTRTreefromRaster which somehow gets me for each polygon in areasOfInterest
      // the intersecting polygons of the tree so I can calculate the area-weighted average
      intersection_t *intersections = querySTRTree(areasOfInterest, rasterTree);
      if (intersections == NULL) {
        fprintf(stderr, "No intersections found\n"); // this is not treated as an error
        // not pretty, but: areasOfInterest, data and rasterWKT are not freed here but after for-loop
        freeAverageData(average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        break; // break out of foor loop and continue with next raster dataset
      }

      // a functions that (should be split up into smaller pieces)
      // 1. converts GEOSGeometry back to OGRGeometry (via WKBExport)
      // 2. given two OGRGeometries (Polygons) computes the intersection
      // 3. a) depending on the CRS being geodesic or not, calculating the appropriate area
      // 3. b) query a WKT/dataset for property
      // 4. calculate area-weighted average
      // 5. get centroid of polygon
      mean_t *weightedMeans = calculateAreaWeightedMean(intersections, rasterWkt);
      if (weightedMeans == NULL) {
        fprintf(stderr, "Failed to calculate weighted means\n");
        freeVectorGeometryList(areasOfInterest);
        freeRawData(data);
        freeAverageData(average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        freeIntersections(intersections);
        CPLFree((void* ) rasterWkt);
        return 1;
      }

      // todo: come up with a nicer way to construct file paths?
      int outputFilePathLength = (int) strlen(options->outputDirectory) + 15; // yyyy-mm-dd.txt + \0 = 15

      char *textOutputFilePath = calloc(outputFilePathLength, sizeof(char));
      if (textOutputFilePath == NULL) {
        perror("calloc");
        // todo cleanup
        return 1;
      }

      int charsWritten = snprintf(textOutputFilePath, outputFilePathLength, "%s%.4d-%.2d-%.2d.txt",
                                  options->outputDirectory, currentYear, currentMonth, *day);

      if (charsWritten >= outputFilePathLength || charsWritten < 0) {
        fprintf(stderr, "Failed to construct file path for output text file\n");
        // todo cleanup
        return 1;
      }

      // 6. write tuple (centroid coordinates, average value, ERA5) to a file
      writeWeightedMeans(weightedMeans, textOutputFilePath);

      freeCellGeometryList(rasterCellsAsGEOS);
      freeIntersections(intersections);
      freeWeightedMeans(weightedMeans);

      GEOSSTRtree_destroy(rasterTree);

      freeAverageData(average);
      free(textOutputFilePath);

      processedDays++;
    }

    CPLFree((void* ) rasterWkt);
    freeVectorGeometryList(areasOfInterest);
    freeRawData(data);

    successfulDownloads = successfulDownloads->next;
  }

  return 0;
}

size_t countRequestedHours(const option_t *options)
{
  size_t count = 0;

  int *hours = (int *) options->hours;

  while (*hours != INITVAL) {
    count++;
    hours++;
  }

  return count;
}

bool isValidDate(int year, int month, int day)
{
  static int daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // NOTE: year is only checked for posive value
  bool validYear = year >= 0;
  bool validMonth = month >= 1 && month <= 12;
  bool isLeapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  bool validDay = day >= 1
                  && day <= (month == 2 ? (isLeapYear ? 29 : daysPerMonth[month - 1]) : daysPerMonth[month - 1]);

  return validYear && validMonth && validDay;
}