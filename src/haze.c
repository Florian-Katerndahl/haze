#include "haze.h"
#include "paths.h"
#include "types.h"
#include "gdal-ops.h"
#include "math-utils.h"
#include "strtree.h"
#include <dirent.h>
#include <bits/posix2_lim.h>
#include <gdal/cpl_conv.h>
#include <geos_c.h>
#include <stdbool.h>
#include <stddef.h>
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
#include <libgen.h>

void freeRawData(struct rawData *data)
{
  free(data->data);
}

void freeAverageData(struct averagedData *data)
{
  free(data->data);
}

int readRasterDataset(GDALDatasetH raster, struct rawData *dataBuffer)
{
  dataBuffer->bands = GDALGetRasterCount(raster);
  GDALRasterBandH layer = openRasterBand(raster, 1);

  if (layer == NULL) {
    return 1;
  }

  GDALDataType dType = GDALGetRasterDataType(layer);
  dataBuffer->columns = (size_t) GDALGetRasterBandXSize(layer);
  dataBuffer->rows = (size_t) GDALGetRasterBandYSize(layer);
  size_t byteSize = (size_t) GDALGetDataTypeSizeBytes(dType);

  if (dType != GDT_Float64 || byteSize != sizeof(double)) {
    return 1;
  }

  dataBuffer->data = calloc(dataBuffer->rows * dataBuffer->columns * dataBuffer->bands, sizeof(double));
  if (dataBuffer->data == NULL) {
    perror("calloc");
    return 1;
  }

  // data seems to be BSQ? Or at least it saved into the buffer one scanline at a time
  // could be helpful to manually transform to PIL
  // Doesn't GDAL hide this from me? When requesting bands, I get a band no matter how the underlying data is interleaved! I.e., data returned is always BSQ
  CPLErr readErr = GDALDatasetRasterIOEx(
                     raster, GF_Read, 0, 0,
                     (int) dataBuffer->columns, (int) dataBuffer->rows,
                     (void *) dataBuffer->data, (int) dataBuffer->columns,
                     (int) dataBuffer->rows, dType,
                     dataBuffer->bands, NULL, 0, 0, 0, NULL);

  if (readErr == CE_Failure) {
    fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
    free(dataBuffer->data);
    return 1;
  }

  return 0;
}

int averageRawData(const struct rawData *data, struct averagedData *average)
{
  average->columns = data->columns;
  average->rows = data-> rows;

  average->data = calloc(data->rows * data->columns, sizeof(double));
  if (average->data == NULL) {
    perror("calloc");
    return 1;
  }

  for (size_t row = 0; row < data->rows; row++) {
    size_t rowOffset = row * data->columns;
    for (size_t column = 0; column < data->columns; column++) {
      size_t columnOffset = column;
      double sum = 0.0;
      for (size_t band = 0; band < data->bands; band++) {
        size_t bandOffset = band * data->columns * data->rows;
        sum += data->data[rowOffset + columnOffset + bandOffset];
      }
      average->data[columnOffset + rowOffset] = sum / (double) data->bands;
    }
  }
  return 0;
}

int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData *average,
                                 const size_t size, const size_t offset)
{
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * `day of interest (0-based)`
  size_t startBand = offset;
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + offset;

  if (startBand >= data->bands || boundary >= data->bands) {
    return 1;
  }

  average->columns = data->columns;
  average->rows = data-> rows;

  average->data = calloc(data->rows * data->columns, sizeof(double));
  if (average->data == NULL) {
    perror("calloc");
    return 1;
  }

  for (size_t row = 0; row < data->rows; row++) {
    size_t rowOffset = row * data->columns;
    for (size_t column = 0; column < data->columns; column++) {
      size_t columnOffset = column;
      double sum = 0.0;
      for (size_t band = startBand; band < boundary; band++) {
        size_t bandOffset = band * data->columns * data->rows;
        sum += data->data[columnOffset + rowOffset + bandOffset];
      }
      average->data[rowOffset + columnOffset] = sum / (double) data->bands;
    }
  }
  return 0;
}

int averagePILRawDataWithSizeOffset(const struct rawData *data, struct averagedData *average,
                                    const size_t size, const size_t offset)
{
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * `day of interest (0-based)`
  size_t startBand = offset;
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + offset;

  if (startBand >= data->bands || boundary >= data->bands) {
    return 1;
  }

  average->columns = data->columns;
  average->rows = data-> rows;

  average->data = calloc(data->rows * data->columns, sizeof(double));
  if (average->data == NULL) {
    perror("calloc");
    return 1;
  }

  for (size_t row = 0; row < data->rows; row++) {
    size_t rowOffset = row * data->columns * data->bands;
    for (size_t column = 0; column < data->columns; column++) {
      size_t columnOffset = column * data->bands;
      double sum = 0.0;
      for (size_t band = startBand; band < boundary; band++) {
        sum += data->data[rowOffset + columnOffset + band];
      }
      average->data[column + row * data->columns] = sum / (double) data->bands;
    }
  }
  return 0;
}

int reorderToBandInterleavedByPixel(struct rawData *data)
{
  // hmm, actually unsure if this is beneficial; page faults are not lower with two passes; probably needs testing with longer inputs to fully conclude if this is worth it
  // it becomes apparent however, that IO is THE bottleneck as execution differs only slightly with re-ordered data being 2 seconds faster (probably hot cache paths..)
  // cache misses grew marginally slower for PIL data (did I meant to write that there were fewer cache misses?)
  // BUT without runtime sanitizers and O3: 5 major page faults vs 333 and 152232 minor page faults vs 148721 -> could be worth it for multiple calculations on same data
  double *temporaryArray = calloc(data->columns * data->rows * data->bands, sizeof(double));
  if (temporaryArray == NULL) {
    perror("calloc");
    return 1;
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
        temporaryArray[targetBandOffset + targetXOffset + targetYOffset] = data->data[originYOffset + originXOffset + originBandOffset];
      }
    }
  }

  double *temp = data->data;
  data->data = temporaryArray;
  free(temp);

  return 0;
}

[[nodiscard]] mean_t *calculateAreaWeightedMean(intersection_t *intersections,
    const char *rasterWkt)
{
  mean_t *root = NULL;

  OGRSpatialReferenceH spatialRef = OSRNewSpatialReference(rasterWkt);
  if (spatialRef == NULL) {
    fprintf(stderr, "Could not determine if CRS is projected or not");
    return NULL;
  }

  CRS_TYPE CRSType = getCRSType(rasterWkt);

  if (CRSType == CRS_UNKNOWN) {
    OSRDestroySpatialReference(spatialRef);
    return NULL;
  }

  size_t wkbSize;
  GEOSWKBWriter *wkbWriter = GEOSWKBWriter_create();
  if (wkbWriter == NULL) {
    fprintf(stderr, "Failed to create WKB writer\n");
    freeWeightedMeans(root);
    return NULL;
  }

  while (intersections != NULL) {
    OGRGeometryH centroid = OGR_G_CreateGeometry(wkbPoint);
    if (centroid == NULL) {
      fprintf(stderr, "Failed to create empty centroid\n");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      return NULL;
    }

    if (OGR_G_Centroid(intersections->reference, centroid) == OGRERR_FAILURE) {
      fprintf(stderr, "Failed to calculate centroid\n");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      OGR_G_DestroyGeometry(centroid);
      return NULL;
    }

    double referenceArea = CRSType == CRS_GEOGRAPHIC ? OGR_G_GeodesicArea(
                             intersections->reference) : OGR_G_Area(
                             intersections->reference);
    if (referenceArea == -1) {
      fprintf(stderr, "Failed to calculate reference area\n");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      OGR_G_DestroyGeometry(centroid);
      return NULL;
    }

    double *values = calloc(intersections->intersectionCount, sizeof(double));
    if (values == NULL) {
      perror("calloc");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      OGR_G_DestroyGeometry(centroid);
      return NULL;
    }

    double *weights = calloc(intersections->intersectionCount, sizeof(double));
    if (weights == NULL) {
      perror("calloc");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      OGR_G_DestroyGeometry(centroid);
      free(values);
      return NULL;
    }

    cellGeometryList *temp = intersections->intersectingCells;

    // iterate over all found intersections
    for (size_t i = 0; i < intersections->intersectionCount; i++) {
      values[i] = temp->entry->value; // shit, here I do copy data again...

      OGRGeometryH cellAsOGR = OGR_G_CreateGeometry(wkbPolygon);
      OGR_G_AssignSpatialReference(cellAsOGR, spatialRef);

      const unsigned char *geometryAsWkb = GEOSWKBWriter_write(wkbWriter, temp->entry->geometry,
                                           &wkbSize);
      if (geometryAsWkb == NULL) {
        fprintf(stderr, "Failed to export geometry as WKB\n");
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(root);
        OGR_G_DestroyGeometry(centroid);
        OGR_G_DestroyGeometry(cellAsOGR);
        free(values);
        free(weights);
        return NULL;
      }

      if (OGR_G_ImportFromWkb(cellAsOGR, (void *) geometryAsWkb, (int) wkbSize) != OGRERR_NONE) {
        fprintf(stderr, "Failed to import WKB to OGR: %s\n", CPLGetLastErrorMsg());
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(root);
        OGR_G_DestroyGeometry(centroid);
        OGR_G_DestroyGeometry(cellAsOGR);
        free(values);
        free(weights);
        GEOSFree((void *) geometryAsWkb);
        return NULL;
      }

      OGRGeometryH intersection = OGR_G_Intersection(intersections->reference, cellAsOGR);
      if (intersection == NULL) {
        fprintf(stderr, "Failed to create intersection-polygon: %s\n", CPLGetLastErrorMsg());
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(root);
        OGR_G_DestroyGeometry(centroid);
        OGR_G_DestroyGeometry(cellAsOGR);
        free(values);
        free(weights);
        GEOSFree((void *) geometryAsWkb);
        return NULL;
      }

      OGR_G_AssignSpatialReference(intersection, spatialRef);

      double intersectingArea = CRSType == CRS_GEOGRAPHIC ? OGR_G_GeodesicArea(intersection) : OGR_G_Area(intersection);

      weights[i] = intersectingArea / referenceArea;

      temp = temp->next;

      GEOSFree((void *) geometryAsWkb);
      OGR_G_DestroyGeometry(intersection);
      OGR_G_DestroyGeometry(cellAsOGR);
    }

    mean_t *meanEntry = calloc(1, sizeof(mean_t));
    if (meanEntry == NULL) {
      perror("calloc");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(root);
      free(values);
      free(weights);
      return NULL;
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

int writeWeightedMeans(mean_t *values, const char *filePath)
{
  if (values == NULL || filePath == NULL) {
    return 1;
  }

  FILE *outFile = fopen(filePath, "wt");
  if (outFile == NULL) {
    perror("fopen");
    return 1;
  }

  while (values != NULL) {
    // unfeasable to compute number of characters beforehand, but we can check for errors
    if (fprintf(outFile, "%.4lf %.4lf %f ERA\n",values->x, values->y, (float) kgsqmTocow(values->value)) < 0) {
      fprintf(stderr, "Failed to write weighted meann\n");
      fclose(outFile);
      unlink(filePath);
      return 1;
    }    

    values = values->next;
  }

  fclose(outFile);
  return 0;
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
#ifdef DEBUG
    printf("Processing file %s\n", successfulDownloads->string);
#endif
    GDALDatasetH ds = openRasterDataset(successfulDownloads->string);

    if (ds == NULL)
      return -1;

    const char *rasterWkt = extractCRSAsWKT(ds, NULL);

    vectorGeometryList *areasOfInterest = buildGEOSGeometriesFromFile(options->areaOfInterest, NULL,
                                          rasterWkt);

    struct rawData data = {0};
    if (readRasterDataset(ds, &data)) {
      closeGDALDataset(ds);
      CPLFree((void* ) rasterWkt);
      freeVectorGeometryList(areasOfInterest);
      return -1;
    }

    struct geoTransform transform = {0};
    if (getRasterMetadata(ds, &transform)) {
      fprintf(stderr, "Failed to get geo transformation from dataset %s\n", successfulDownloads->string);
      closeGDALDataset(ds);
      CPLFree((void* ) rasterWkt);
      freeVectorGeometryList(areasOfInterest);
      freeRawData(&data);
      return -1;
    }

    const int nLayers = GDALGetRasterCount(ds);

    closeGDALDataset(ds);

    size_t hoursPerDay = options->hoursElements;
    size_t processedDays = 0;

    for (size_t i = 0; i < options->daysElements
         && (int) (processedDays * hoursPerDay) < nLayers; i++, processedDays++) {
      int day = options->days[i];
#ifdef DEBUG
      printf("%ld/%d\n", processedDays * hoursPerDay, nLayers);
#endif

      int currentYear;
      int currentMonth;

      const char *inputFileName = basename(successfulDownloads->string);
      if (inputFileName == NULL) {
        fprintf(stderr, "Malformed file path. Could not get final path delimiter\n");
        continue;
      }

      if (sscanf(inputFileName, "%d-%d.grib", &currentYear, &currentMonth) != 2) {
        fprintf(stderr, "Failed to extract year and month from file name\n");
        continue;
      }

      if (!isValidDate(currentYear, currentMonth, day)) {
#ifdef DEBUG
        printf("Skipping invalid date %.4d-%.2d-%.2d: %s\n", currentYear, currentMonth, day,
               successfulDownloads->string);
#endif
        continue;
      }

      struct averagedData average = {0};

#ifdef DEBUG
      printf("Averaging bands %ld to %ld\n", processedDays * hoursPerDay,
             processedDays * hoursPerDay + hoursPerDay);
#endif

      if (averageRawDataWithSizeOffset(&data, &average, hoursPerDay, processedDays * hoursPerDay)) {
        fprintf(stderr, "Failed to compute averages\n");
        freeAverageData(&average);
        continue;
      }

      cellGeometryList *rasterCellsAsGEOS = NULL;

      GEOSSTRtree *rasterTree = buildSTRTreefromRaster(&average, &transform, &rasterCellsAsGEOS);

      // a function to query the tree constructed by buildSTRTreefromRaster which somehow gets me for each polygon in areasOfInterest
      // the intersecting polygons of the tree so I can calculate the area-weighted average
      intersection_t *intersections = querySTRTree(areasOfInterest, rasterTree);
      if (intersections == NULL) {
        fprintf(stderr, "No intersections found\n"); // this is not treated as an error
        // not pretty, but: areasOfInterest, data and rasterWKT are not freed here but after for-loop
        freeAverageData(&average);
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
        freeRawData(&data);
        freeAverageData(&average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        freeIntersections(intersections);
        CPLFree((void* ) rasterWkt);
        return 1;
      }

      char *textOutputFilePath = constructFilePath("%s/%.4d-%.2d-%.2d.txt", options->outputDirectory,
                                 currentYear, currentMonth, day);

      if (textOutputFilePath == NULL) {
        fprintf(stderr, "Failed to construct file path for output text file\n");
        freeVectorGeometryList(areasOfInterest);
        freeRawData(&data);
        freeAverageData(&average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        freeIntersections(intersections);
        CPLFree((void* ) rasterWkt);
        return 1;
      }

      // 6. write tuple (centroid coordinates, average value, ERA5) to a file
      writeWeightedMeans(weightedMeans, textOutputFilePath);

      freeCellGeometryList(rasterCellsAsGEOS);
      freeIntersections(intersections);
      freeWeightedMeans(weightedMeans);

      GEOSSTRtree_destroy(rasterTree);

      freeAverageData(&average);
      free(textOutputFilePath);
    }

    CPLFree((void* ) rasterWkt);
    freeVectorGeometryList(areasOfInterest);
    freeRawData(&data);

    successfulDownloads = successfulDownloads->next;
  }

  return 0;
}

bool isValidDate(int year, int month, int day)
{
  static int daysPerMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  // NOTE: year is only checked for posive value
  bool validYear = year >= 0;
  bool validMonth = month >= 1 && month <= 12;
  bool isLeapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  bool validDay = day >= 1
                  && day <= (month == 2 ? (isLeapYear ? daysPerMonth[month - 1] + 1 : daysPerMonth[month - 1]) :
                             daysPerMonth[month - 1]);

  return validYear && validMonth && validDay;
}
