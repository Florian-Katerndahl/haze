#include "haze.h"
#include "types.h"
#include "fscheck.h"
#include "math-utils.h"
#include <assert.h>
#include <bits/posix2_lim.h>
#include <gdal/cpl_conv.h>
#include <geos_c.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <gdal/gdal.h>
#include <gdal/gdal_version.h>
#include <gdal/cpl_error.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <gdal/ogr_api.h>
#include <unistd.h>

struct rawData *allocateRawData(void)
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

struct averagedData *allocateAverageData(void)
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

[[nodiscard]] GDALDatasetH openRaster(const char *filePath)
{
  if (filePath == NULL) {
    fprintf(stderr, "ERROR: filePath cannot be NULL\n");
    return NULL;
  }

  assert(fileReadable(filePath));

  GDALAllRegister();
  // TODO could read through entire directory when using sibling files?
  GDALDatasetH raster = GDALOpenEx(filePath, GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
  if (raster == NULL) {
    fprintf(stderr, "ERROR: Failed to open raster dataset %s\n", filePath);
    return NULL;
  }

  return raster;
}

[[nodiscard]] GDALDatasetH openVector(const char *filePath)
{
  if (filePath == NULL) {
    fprintf(stderr, "ERROR: filePath cannot be NULL\n");
    return NULL;
  }

  assert(fileReadable(filePath));

  GDALAllRegister(); // todo: this should be called by the application, not the library - right?

  GDALDatasetH vector = GDALOpenEx(filePath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
  if (vector == NULL) {
    fprintf(stderr, "Failed to read vector dataset: %s", CPLGetLastErrorMsg());
    return NULL;
  }

  return vector;
}

void closeGDALDataset(GDALDatasetH dataset)
{
  if (GDALClose(dataset) != CE_None) {
    exit(1); // I mean, if this fails why continue at all?
  }
}

OGRLayerH  openVectorLayer(GDALDatasetH vector, const char *name)
{
  if (name) {
    return OGR_DS_GetLayerByName(vector, name);
  } else {
    return OGR_DS_GetLayer(vector, 0);
  }
}

GDALRasterBandH openRasterBand(GDALDatasetH raster, int index)
{
  int layerCount = GDALGetRasterCount(raster);
  if (index < 1 || index > layerCount) {
    fprintf(stderr, "Index out of range\n");
    return NULL;
  }

  GDALRasterBandH band = GDALGetRasterBand(raster, index);
  if (band == NULL) {
    fprintf(stderr, "ERROR: Failed to fetch raster band %d\n", index);
    return NULL;
  }
  return band;
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

int averageRawDataWithSizeOffset(const struct rawData *data, struct averagedData **average,
                                 const size_t size, const size_t offset)
{
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * nDays
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
  // now, daily averages can be calculated by setting the size to number of observations per day (see query) and offset to nObservations * nDays
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

int getRasterMetadata(GDALDatasetH raster, struct geoTransform *geoTransformation)
{
  double tmp[6];
  if (GDALGetGeoTransform(raster, tmp) == CE_Failure)
    return 1;
  geoTransformation->xOrigin     = tmp[0];
  geoTransformation->pixelWidth  = tmp[1];
  geoTransformation->rowRotation = tmp[2];
  geoTransformation->yOrigin     = tmp[3];
  geoTransformation->colRotation = tmp[4];
  geoTransformation->pixelHeight = tmp[5];

  return 0;
}

bool isProjected(const char *Wkt)
{
  OGRSpatialReferenceH  spatialRef = OSRNewSpatialReference(Wkt);
  if (spatialRef == NULL) {
    fprintf(stderr, "Could not determine if CRS is projected or not");
    exit(-1); // todo: doesn't make sense to continue if this fails
  }

  bool isProjected = OSRIsProjected(spatialRef);

  OSRDestroySpatialReference(spatialRef);

  return isProjected;
}

mean_t *calculateAreaWeightedMean(intersection_t *intersections, const char *rasterWkt)
{
  mean_t *root = NULL;

  OGRSpatialReferenceH  spatialRef = OSRNewSpatialReference(rasterWkt);
  if (spatialRef == NULL) {
    fprintf(stderr, "Could not determine if CRS is projected or not");
    return NULL;
  }

#if GDAL_VERSION_NUM >= 3090000
  const bool isGeodesic = !isProjected(rasterWkt);
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
    double referenceArea = isGeodesic ? OGR_G_GeodesicArea(intersections->reference) : OGR_G_Area(
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
      double intersectingArea = isGeodesic ? OGR_G_GeodesicArea(intersection) : OGR_G_Area(intersection);
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
      values->x, values->y, (float) values->value);

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
