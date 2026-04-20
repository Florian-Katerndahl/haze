#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "haze.h"
#include "paths.h"
#include "types.h"
#include "gdal-ops.h"
#include "math-utils.h"
#include "strtree.h"
#include <dirent.h>
#include <bits/posix2_lim.h>
#include <geos_c.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <gdal/gdal.h>
#include <gdal/gdal_version.h>
#include <gdal/cpl_error.h>
#include <gdal/cpl_conv.h>
#include <gdal/cpl_port.h>
#include <gdal/cpl_string.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <gdal/ogr_api.h>
#include <unistd.h>

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

  dataBuffer->data = calloc(dataBuffer->rows * dataBuffer->columns * dataBuffer->bands,
                            sizeof(double));
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
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + startBand;

  if (startBand >= data->bands || boundary > data->bands) {
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
  size_t boundary = size == 0 && startBand == 0 ? data->bands : size + startBand;

  if (startBand >= data->bands || boundary > data->bands) {
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
        temporaryArray[targetBandOffset + targetXOffset + targetYOffset] = data->data[originYOffset +
          originXOffset + originBandOffset];
      }
    }
  }

  double *temp = data->data;
  data->data = temporaryArray;
  free(temp);

  return 0;
}

[[nodiscard]] OGRGeometryH mergeFootprintSplitAtDateline(const OGRGeometryH splitFootprint)
{
  OGRGeometryH shiftedMultiPolygon = OGR_G_CreateGeometry(OGR_G_GetGeometryType(splitFootprint));

  if (shiftedMultiPolygon == NULL) {
    fprintf(stderr, "Failed to create new geometry to shift\n");
    return NULL;
  }

  int numberOfSubPolygons = OGR_G_GetGeometryCount(splitFootprint);

  for (int subPolygonIdx = 0; subPolygonIdx < numberOfSubPolygons; subPolygonIdx++) {
    OGRGeometryH subPolygon = OGR_G_GetGeometryRef(splitFootprint, subPolygonIdx);

    if (subPolygon == NULL) {
      fprintf(stderr, "Failed to clone sub-ring\n");
      OGR_G_DestroyGeometry(shiftedMultiPolygon);
      return NULL;
    }

    OGRGeometryH shiftedSubPolygon = OGR_G_CreateGeometry(OGR_G_GetGeometryType(subPolygon));

    if (shiftedSubPolygon == NULL) {
      fprintf(stderr, "Failed to create new geometry for shifted sub-polygon\n");
      OGR_G_DestroyGeometry(shiftedMultiPolygon);
      return NULL;
    }

    int numberOfRings = OGR_G_GetGeometryCount(subPolygon);

    for (int ringCountIdx = 0; ringCountIdx < numberOfRings; ringCountIdx++) {
      OGRGeometryH subRing = OGR_G_GetGeometryRef(subPolygon, ringCountIdx);

      int numberOfPoints = OGR_G_GetPointCount(subRing);

      /// NOTE: If I understand the docs correctly, this must always be a wkbLinearRing
      OGRGeometryH shiftedSubRing = OGR_G_CreateGeometry(wkbLinearRing);

      if (shiftedSubRing == NULL) {
        /// TODO: message and cleanup
        fprintf(stderr, "Failed to create linear ring for shifted sub-ring of sub-polygon\n");
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      double *xPoints = NULL;
      double *yPoints = NULL;
      double *zPoints = NULL;
      double *mPoints = NULL;

      size_t xStride = sizeof(double);
      size_t yStride = sizeof(double);
      size_t zStride = 0;
      size_t mStride = 0;

      bool is3D = wkbHasZ(OGR_G_GetGeometryType(subRing));
      bool isMeasured = wkbHasM(OGR_G_GetGeometryType(subRing));

      xPoints = malloc(numberOfPoints * sizeof(double));
      yPoints = malloc(numberOfPoints * sizeof(double));

      if (is3D) {
        zPoints = malloc(numberOfPoints * sizeof(double));
        zStride = sizeof(double);
      }

      if (isMeasured) {
        mPoints = malloc(numberOfPoints * sizeof(double));
        mStride = sizeof(double);
      }

      if (xPoints == NULL || yPoints == NULL) {
        fprintf(stderr, "Failed to allocate memory for x and y coorindates of multi-polygon sub-ring\n");
        free(xPoints);
        free(yPoints);
        free(zPoints);
        free(mPoints);
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      if (is3D && zPoints == NULL) {
        fprintf(stderr, "Failed to allocate memory for z coorindates of multi-polygon sub-ring\n");
        free(xPoints);
        free(yPoints);
        free(zPoints);
        free(mPoints);
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      if (isMeasured && mPoints == NULL) {
        fprintf(stderr, "Failed to allocate memory for m coorindates of multi-polygon sub-ring\n");
        free(xPoints);
        free(yPoints);
        free(zPoints);
        free(mPoints);
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      if (OGR_G_GetPointsZM(subRing, xPoints, xStride, yPoints, yStride, zPoints, zStride, mPoints,
                            mStride) != numberOfPoints) {
        fprintf(stderr, "Failed to read all x coordinates from sub-ring of multi-polygon\n");
        free(xPoints);
        free(yPoints);
        free(zPoints);
        free(mPoints);
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      for (int idx = 0; idx < numberOfPoints; idx++) {
        if (xPoints[idx] >= 0.0)
          continue;
        xPoints[idx] += 360.0;
      }

      OGR_G_SetPointsZM(shiftedSubRing, numberOfPoints, xPoints, xStride, yPoints, yStride, zPoints,
                        zStride, mPoints, mStride);

      if (OGR_G_AddGeometry(shiftedSubPolygon, shiftedSubRing) != OGRERR_NONE) {
        fprintf(stderr, "Failed to add linear ring geometry to shifted sub polygon\n");
        free(xPoints);
        free(yPoints);
        free(zPoints);
        free(mPoints);
        OGR_G_DestroyGeometry(shiftedSubPolygon);
        OGR_G_DestroyGeometry(shiftedMultiPolygon);
        return NULL;
      }

      OGR_G_DestroyGeometry(shiftedSubRing);

      free(xPoints);
      free(yPoints);
      free(zPoints);
      free(mPoints);
    }

    if (OGR_G_AddGeometry(shiftedMultiPolygon, shiftedSubPolygon) != OGRERR_NONE) {
      fprintf(stderr, "Failed to add ring geometry to shifted polygon");
      OGR_G_DestroyGeometry(shiftedMultiPolygon);
      return NULL;
    }

    OGR_G_DestroyGeometry(shiftedSubPolygon);
  }

  // because shift is without offset, geometry intersects with itself which is not allowed
  OGRGeometryH validShiftedMultiPolygon = OGR_G_MakeValid(shiftedMultiPolygon);

  OGR_G_DestroyGeometry(shiftedMultiPolygon);

  if (validShiftedMultiPolygon == NULL) {
    fprintf(stderr, "Failed to create a valid shifted multipolygon\n");
    OGR_G_DestroyGeometry(shiftedMultiPolygon);
    return NULL;
  }

  OGRGeometryH shiftedAndMerged = OGR_G_UnaryUnion(validShiftedMultiPolygon);

  if (shiftedAndMerged == NULL) {
    fprintf(stderr, "Failed to create unary union of shifted geometry\n");
    OGR_G_DestroyGeometry(validShiftedMultiPolygon);
    OGR_G_DestroyGeometry(shiftedMultiPolygon);
    return NULL;
  }

  OGR_G_DestroyGeometry(validShiftedMultiPolygon);

  return shiftedAndMerged;
}

[[nodiscard]] meanVector *calculateAreaWeightedMean(intersectionVector *intersections,
    const char *rasterWkt, bool geometriesAreFootprints)
{
  meanVector *means = malloc(sizeof(meanVector));

  if (means == NULL) {
    fprintf(stderr, "Failed to allocate memory for vector of mean values\n");
    return NULL;
  }

  means->entries = malloc(intersections->size * sizeof(struct m));
  means->size = intersections->size;

  if (means->entries == NULL) {
    fprintf(stderr, "Failed to allocate memory for array of mean values\n");
    /// TODO: proper cleanup for meanVector
    free(means);
    return NULL;
  }

  OGRSpatialReferenceH spatialRef = OSRNewSpatialReference(rasterWkt);
  if (spatialRef == NULL) {
    fprintf(stderr, "Could not create new OGRSpatialReferenceH from WKT\n");
    return NULL;
  }

  // disregard SRS axis ordering in favor of hard coded long/lat ordering
  // WKT/WKB order the data as tuples of x/long and y/lat. When reading them into OGRGeometryH-objects, this order is preserved and no axis
  // swapping is performed. Assigning a spatial reference system to a geometry object assumes the coordinate fields are already correctly
  // ordered.
  OSRSetAxisMappingStrategy(spatialRef, OAMS_TRADITIONAL_GIS_ORDER);

  CRS_TYPE CRSType = getCRSType(rasterWkt);

  if (CRSType == CRS_UNKNOWN) {
    OSRDestroySpatialReference(spatialRef);
    return NULL;
  }

  size_t wkbSize;
  GEOSWKBWriter *wkbWriter = GEOSWKBWriter_create();
  if (wkbWriter == NULL) {
    fprintf(stderr, "Failed to create WKB writer\n");
    freeWeightedMeans(means);
    return NULL;
  }

#ifdef DEBUG
  char pwd[PATH_MAX];
  if (getcwd(pwd, sizeof(pwd)) == NULL) {
    fprintf(stderr, "Failed to get current working directory\n");
    freeWeightedMeans(means);
    return NULL;
  }

  const char *debugOutputPath = constructFilePath("%s/debug-%ld.gpkg", pwd, time(NULL));
  const char *debugOutputLayerName = "intersections";

  fprintf(stderr, "Exporting intersecting geometries in debug mode at %s\n", debugOutputPath);

  GDALDriverH *debugOutputDriver = GDALGetDriverByName("GPKG");
  if (debugOutputDriver == NULL) {
    fprintf(stderr, "Failed to get GPKG driver. Aborting.\n");
    freeWeightedMeans(means);
    free((char*) debugOutputPath);
    return NULL;
  }

  GDALDatasetH debugOutputDataset = GDALCreate(debugOutputDriver, debugOutputPath, 0, 0, 0,
                                    GDT_Unknown, NULL);
  if (debugOutputDataset == NULL) {
    fprintf(stderr, "Failed to create output dataset %s. Aborting.\n", debugOutputPath);
    /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
    freeWeightedMeans(means);
    free((char*) debugOutputPath);
    return NULL;
  }

  OGRLayerH debugOutputLayer = GDALDatasetCreateLayer(debugOutputDataset, debugOutputLayerName,
                               spatialRef, wkbMultiPolygon, NULL);
  if (debugOutputLayer == NULL) {
    fprintf(stderr, "Failed to create output layer. Aborting\n");
    GDALClose(debugOutputDataset);
    /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
    freeWeightedMeans(means);
    unlink(debugOutputPath);
    free((char*) debugOutputPath);
    return NULL;
  }

  OGRFieldDefnH parentIdDefinition = OGR_Fld_Create("parentFID", OFTInteger64);
  if (OGR_L_CreateField(debugOutputLayer, parentIdDefinition, true) != OGRERR_NONE) {
    fprintf(stderr, "Failed to create field\n");
    OGR_Fld_Destroy(parentIdDefinition);
    GDALClose(debugOutputDataset);
    /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
    freeWeightedMeans(means);
    unlink(debugOutputPath);
    free((char*) debugOutputPath);
    return NULL;
  }

  OGR_Fld_Destroy(parentIdDefinition);

  OGRFieldDefnH valueDefinition = OGR_Fld_Create("waterVapor", OFTReal);
  if (OGR_L_CreateField(debugOutputLayer, valueDefinition, true) != OGRERR_NONE) {
    fprintf(stderr, "Failed to create field\n");
    OGR_Fld_Destroy(valueDefinition);
    GDALClose(debugOutputDataset);
    /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
    freeWeightedMeans(means);
    unlink(debugOutputPath);
    free((char*) debugOutputPath);
    return NULL;
  }

  OGR_Fld_Destroy(valueDefinition);
#endif

  for (size_t referenceIndex = 0; referenceIndex < intersections->size; referenceIndex ++) {
    OGRGeometryH centroid = OGR_G_CreateGeometry(wkbPoint);
    if (centroid == NULL) {
      fprintf(stderr, "Failed to create empty centroid\n");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(means);
#ifdef DEBUG
      GDALClose(debugOutputDataset);
      /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
      unlink(debugOutputPath);
      free((char*) debugOutputPath);
#endif
      return NULL;
    }

    if (geometriesAreFootprints
        && wkbFlatten(OGR_G_GetGeometryType(intersections->entries[referenceIndex].reference)) ==
        wkbMultiPolygon) {
      OGRGeometryH shiftedPolygon = mergeFootprintSplitAtDateline(
                                      intersections->entries[referenceIndex].reference);

      if (shiftedPolygon == NULL) {
        fprintf(stderr, "Failed to merge split multipolygon into polygon\n");
        OGR_G_DestroyGeometry(centroid);
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }

      if (OGR_G_Centroid(shiftedPolygon, centroid) == OGRERR_FAILURE) {
        fprintf(stderr, "Failed to calculate centroid\n");
        OGR_G_DestroyGeometry(shiftedPolygon);
        OGR_G_DestroyGeometry(centroid);
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }

      OGR_G_DestroyGeometry(shiftedPolygon);
    } else {
      if (OGR_G_Centroid(intersections->entries[referenceIndex].reference, centroid) == OGRERR_FAILURE) {
        fprintf(stderr, "Failed to calculate centroid\n");
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }
    }

    double referenceArea = CRSType == CRS_GEOGRAPHIC ? OGR_G_GeodesicArea(
                             intersections->entries[referenceIndex].reference) : OGR_G_Area(
                             intersections->entries[referenceIndex].reference);
    if (referenceArea == -1) {
      fprintf(stderr, "Failed to calculate reference area\n");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(means);
      OGR_G_DestroyGeometry(centroid);
#ifdef DEBUG
      GDALClose(debugOutputDataset);
      /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
      unlink(debugOutputPath);
      free((char*) debugOutputPath);
#endif
      return NULL;
    }

    double *values = calloc(intersections->entries[referenceIndex].intersectionCount, sizeof(double));
    if (values == NULL) {
      perror("calloc");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(means);
      OGR_G_DestroyGeometry(centroid);
#ifdef DEBUG
      GDALClose(debugOutputDataset);
      /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
      unlink(debugOutputPath);
      free((char*) debugOutputPath);
#endif
      return NULL;
    }

    double *weights = calloc(intersections->entries[referenceIndex].intersectionCount, sizeof(double));
    if (weights == NULL) {
      perror("calloc");
      GEOSWKBWriter_destroy(wkbWriter);
      OSRDestroySpatialReference(spatialRef);
      freeWeightedMeans(means);
      OGR_G_DestroyGeometry(centroid);
      free(values);
#ifdef DEBUG
      GDALClose(debugOutputDataset);
      /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
      unlink(debugOutputPath);
      free((char*) debugOutputPath);
#endif
      return NULL;
    }

    cellGeometryList *temp = intersections->entries[referenceIndex].intersectingCells;

    // iterate over all found intersections
    for (size_t i = 0; i < intersections->entries[referenceIndex].intersectionCount; i++) {
      values[i] = temp->entry->value; // shit, here I do copy data again...

      OGRGeometryH cellAsOGR;

      const unsigned char *geometryAsWkb = GEOSWKBWriter_write(wkbWriter, temp->entry->geometry,
                                           &wkbSize);
      if (geometryAsWkb == NULL) {
        fprintf(stderr, "Failed to export geometry as WKB\n");
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
        free(values);
        free(weights);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }

      if (OGR_G_CreateFromWkb(geometryAsWkb, spatialRef, &cellAsOGR, wkbSize) != OGRERR_NONE
          || cellAsOGR == NULL) {
        fprintf(stderr, "Failed to import WKB to OGR: %s\n", CPLGetLastErrorMsg());
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
        OGR_G_DestroyGeometry(cellAsOGR);
        free(values);
        free(weights);
        GEOSFree((void *) geometryAsWkb);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }

      OGRGeometryH intersection = OGR_G_Intersection(intersections->entries[referenceIndex].reference,
                                  cellAsOGR);
      if (intersection == NULL) {
        fprintf(stderr, "Failed to create intersection-polygon: %s\n", CPLGetLastErrorMsg());
        GEOSWKBWriter_destroy(wkbWriter);
        OSRDestroySpatialReference(spatialRef);
        freeWeightedMeans(means);
        OGR_G_DestroyGeometry(centroid);
        OGR_G_DestroyGeometry(cellAsOGR);
        free(values);
        free(weights);
        GEOSFree((void *) geometryAsWkb);
#ifdef DEBUG
        GDALClose(debugOutputDataset);
        /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
        unlink(debugOutputPath);
        free((char*) debugOutputPath);
#endif
        return NULL;
      }

      // Intersection is missing its SRS, even though both input geometries have it
      OGR_G_AssignSpatialReference(intersection, spatialRef);

      if (OGR_G_GetGeometryType(intersection) == wkbPolygon
          || OGR_G_GetGeometryType(intersection) == wkbMultiPolygon) {
#ifdef DEBUG
        OGRFeatureH feature = OGR_F_Create(OGR_L_GetLayerDefn(debugOutputLayer));

        OGR_F_SetFieldInteger(feature, OGR_F_GetFieldIndex(feature, "parentFID"),
                              intersections->entries[referenceIndex].referenceFID);
        OGR_F_SetFieldDouble(feature, OGR_F_GetFieldIndex(feature, "waterVapor"), values[i]);
        OGR_F_SetGeometry(feature, intersection);

        if (OGR_L_CreateFeature(debugOutputLayer, feature) != OGRERR_NONE) {
          GEOSWKBWriter_destroy(wkbWriter);
          OSRDestroySpatialReference(spatialRef);
          freeWeightedMeans(means);
          OGR_G_DestroyGeometry(centroid);
          OGR_G_DestroyGeometry(cellAsOGR);
          free(values);
          free(weights);
          GEOSFree((void *) geometryAsWkb);
          GDALClose(debugOutputDataset);
          /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
          unlink(debugOutputPath);
          free((char*) debugOutputPath);
          return NULL;
        }

        OGR_F_Destroy(feature);
#endif
        double intersectingArea = CRSType == CRS_GEOGRAPHIC ? OGR_G_GeodesicArea(intersection) : OGR_G_Area(
                                    intersection);

        if (isnan(intersectingArea) || isnan(referenceArea) || intersectingArea < 0.0
            || referenceArea < 0.0) {
          fprintf(stderr, "Area of intersecting geometry or area of reference is invalid\n");
          /// FIXME: cleanup
        }

        weights[i] = intersectingArea / referenceArea;
      } else if (OGR_G_GetGeometryType(intersection) == wkbPoint) {
        fprintf(stderr, "Intersection resulted in point geometry. Setting both value and weight to 0.\n");
        values[i] = 0.0;
        weights[i] = 0.0;
      } else {
        fprintf(stderr, "Got unexpected geometry type: %s\n", OGR_G_GetGeometryName(intersection));
#ifdef DEBUG
        OGR_G_DumpReadable(intersections->entries[referenceIndex].reference, stdout, NULL);
        OGR_G_DumpReadable(cellAsOGR, stdout, NULL);
        OGR_G_DumpReadable(intersection, stdout, NULL);
#endif
      }

      temp = temp->next;

      GEOSFree((void *) geometryAsWkb);
      OGR_G_DestroyGeometry(intersection);
      OGR_G_DestroyGeometry(cellAsOGR);
    }

    means->entries[referenceIndex].value = calculateWeightedAverage(values, weights,
                                           intersections->entries[referenceIndex].intersectionCount);
    means->entries[referenceIndex].x = OGR_G_GetX(centroid, 0);
    means->entries[referenceIndex].y = OGR_G_GetY(centroid, 0);

    // constrain to +/- 180°
    if (means->entries[referenceIndex].x > 180.0) {
      means->entries[referenceIndex].x -= 360.0;
    } else if (means->entries[referenceIndex].x < -180.0) {
      means->entries[referenceIndex].x += 360.0;
    }

    free(values);
    free(weights);
    OGR_G_DestroyGeometry(centroid);
  }

#ifdef DEBUG
  GDALClose(debugOutputDataset);
  /// NOTE: Destroying the output driver crashes `GDALDestroy`, thus leaving it.
  free((char*) debugOutputPath);
#endif

  GEOSWKBWriter_destroy(wkbWriter);
  OSRDestroySpatialReference(spatialRef);

  return means;
}

int writeWeightedMeans(meanVector *values, const char *filePath)
{
  if (values == NULL || filePath == NULL) {
    return 1;
  }

  FILE *outFile = fopen(filePath, "wt");
  if (outFile == NULL) {
    perror("fopen");
    return 1;
  }

  for (size_t i = 0; i < values->size; i++) {
    // unfeasable to compute number of characters beforehand, but we can check for errors
    if (fprintf(outFile, "%.4lf %.4lf %f ERA\n", values->entries[i].x, values->entries[i].y,
                (float) kgsqmTocow(values->entries[i].value)) < 0) {
      fprintf(stderr, "Failed to write weighted meann\n");
      fclose(outFile);
      unlink(filePath);
      return 1;
    }
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

stringList *parseLogFile(const char *filePath)
{
  stringList *out = NULL, *tail;

  if (filePath == NULL) {
    return NULL;
  }

  FILE *f = fopen(filePath, "r");
  if (f == NULL) {
    return NULL;
  }

  char *lineptr = NULL;
  size_t n = 0;

  while (getline(&lineptr, &n, f) != -1) {
    // remove newline
    size_t length = strlen(lineptr);

    if (lineptr[length - 1] == '\n') {
      lineptr[length - 1] = '\0';
    }

    char *downloadedFile = strtok(lineptr, "\t");
    char *status = strtok(NULL, "\t");

    if (downloadedFile == NULL || status == NULL) {
      fprintf(stderr, "Failed to parse line in log file\n");
      freeStringList(out);
      free(lineptr);
      fclose(f);
      return NULL;
    }

    stringList *node = calloc(1, sizeof(stringList));
    if (node == NULL) {
      fprintf(stderr, "Failed to allocate memory for stringList node\n");
      freeStringList(out);
      free(lineptr);
      fclose(f);
      return NULL;
    }

    node->string = strdup(downloadedFile);
    node->status = strdup(status);

    if (node->string == NULL || node->status == NULL) {
      fprintf(stderr, "Failed to store fields in node\n");
      free(node->string);
      free(node->status);
      freeStringList(out);
      free(lineptr);
      fclose(f);
      return NULL;
    }

    if (out == NULL) {
      out = tail = node;
    } else {
      tail->next = node;
      tail = node;
    }

    free(lineptr);
    n = 0;
  }

  if (!feof(f)) {
    fprintf(stderr, "An error occurred while reading %s\n", filePath);
    freeStringList(out);
    out = NULL;
  }

  free(lineptr);
  fclose(f);

  return out;
}

int writeUpdatedLogFile(stringList *list, const char *filePath)
{
  if (list == NULL || filePath == NULL) {
    return 1;
  }

  FILE *f = fopen(filePath, "w");
  if (f == NULL) {
    fprintf(stderr, "Failed to open log file for writing\n");
    return 1;
  }

  for (stringList *ptr = list; ptr != NULL; ptr = ptr->next) {
    if (fprintf(f, "%s\t%s\n", ptr->string, ptr->status) < 0) {
      fprintf(stderr,
              "Encountered error while writing updated log file. Start all over at this point...\n");
      fclose(f);
      return 1;
    }
  }

  fclose(f);

  return 0;
}

int backFillOptions(option_t *options, GDALDatasetH dataset)
{
  // make sure to start fresh options for new file
  memset(options->years, 0, sizeof(int) * options->yearsElements);
  memset(options->months, 0, sizeof(int) * options->monthsElements);
  memset(options->days, 0, sizeof(int) * options->daysElements);
  memset(options->hours, 0, sizeof(int) * options->hoursElements);
  options->daysElements = 0;
  options->hoursElements = 0;

  const int nLayers = GDALGetRasterCount(dataset);

  // back-fill fields of options struct with one strong assumptions: only ever fill back hours and potentially days;
  // each file is thus assumed to only contain data for either an entire day or entire month
  options->yearsElements = 1;
  options->monthsElements = 1;
  struct tm time;

  int daysHistogram[31] = {0};
  int hoursHistogram[24] = {0};

  for (int i = 1; i <= nLayers; i++) {
    GDALRasterBandH band = openRasterBand(dataset, i);

    if (band == NULL) {
      /// FIXME: ERROR HANDLING
    }

    const char *refTime = GDALGetMetadataItem(band, "GRIB_REF_TIME", NULL);
    if (refTime == NULL) {
      /// FIXME: ERROR HANDLING
    }

    memset(&time, 0, sizeof(time));

    if (strptime(refTime, "%s", &time) == NULL) {
      /// FIXME: ERROR HANDLING
    }

    options->years[0] = time.tm_year + 1900;
    options->months[0] = time.tm_mon + 1;
    daysHistogram[time.tm_mday - 1] += 1;
    hoursHistogram[time.tm_hour] += 1;
  }

  for (int i = 0; i < 31; i++) {
    if (daysHistogram[i]) {
      options->days[options->daysElements] = i + 1;
      options->daysElements += 1;
    }
  }

  for (int i = 0; i < 24; i++) {
    if (hoursHistogram[i]) {
      options->hours[options->hoursElements] = i;
      options->hoursElements += 1;
    }
  }

  return 0;
}

int process(option_t *options)
{
  bool someErrors;

  stringList *logFileList = parseLogFile(options->logFile);

  if (logFileList == NULL) {
    return 1;
  }

  // WKT of ERA5 is assumed to be set to WGS84 and won't change over time; former information from:
  // https://confluence.ecmwf.int/display/CKB/ERA5%3A+data+documentation#heading-SpatialreferencesystemsandEarthmodel and
  // https://gis.stackexchange.com/a/380251
  vectorGeometryVector *areasOfInterest = buildGEOSGeometriesFromFile(options->areaOfInterest,
                                          options->aoiName,
                                          SRS_WKT_WGS84_LAT_LONG);

  if (areasOfInterest == NULL) {
    fprintf(stderr, "Failed to process area of interest\n");
    freeStringList(logFileList);
    return 1;
  }

  for (stringList *ptr = logFileList; ptr != NULL; ptr = ptr->next) {
    someErrors = false;

    if (strcmp("DOWNLOADED", ptr->status) != 0) {
      continue;
    }

#ifdef DEBUG
    printf("Processing file %s\n", ptr->string);
#endif
    GDALDatasetH ds = openRasterDataset(ptr->string);

    if (ds == NULL)
      return -1;

    const int nLayers = GDALGetRasterCount(ds);

    if (backFillOptions(options, ds)) {
      fprintf(stderr, "Failed to extract temporal information from dataset\n");
      closeGDALDataset(ds);
      continue;
    }

    struct rawData data = {0};
    if (readRasterDataset(ds, &data)) {
      closeGDALDataset(ds);
      continue;
    }

    struct geoTransform transform = {0};
    if (getRasterMetadata(ds, &transform)) {
      fprintf(stderr, "Failed to get geo transformation from dataset %s\n", ptr->string);
      closeGDALDataset(ds);
      freeRawData(&data);
      continue;
    }

    closeGDALDataset(ds);

    size_t hoursPerDay = options->hoursElements;
    size_t processedDays = 0;

    int currentYear = options->years[0];
    int currentMonth = options->months[0];

    for (size_t i = 0; i < options->daysElements
         && (int) (processedDays * hoursPerDay) < nLayers; i++, processedDays++) {
      int day = options->days[i];
#ifdef DEBUG
      printf("%ld/%d\n", processedDays * hoursPerDay, nLayers);
#endif

      if (!isValidDate(currentYear, currentMonth, day)) {
#ifdef DEBUG
        printf("Skipping invalid date %.4d-%.2d-%.2d: %s\n", currentYear, currentMonth, day, ptr->string);
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
        someErrors = true;
        break;
      }

      cellGeometryList *rasterCellsAsGEOS = NULL;

      GEOSSTRtree *rasterTree = buildSTRTreefromRaster(&average, &transform, &rasterCellsAsGEOS);
      if (rasterTree == NULL || rasterCellsAsGEOS == NULL) {
        fprintf(stderr, "Failed to construct STRTree from raster file %s", ptr->string);
        /// TODO: cleanup
        continue;
      }

      // a function to query the tree constructed by buildSTRTreefromRaster which somehow gets me for each polygon in areasOfInterest
      // the intersecting polygons of the tree so I can calculate the area-weighted average
      intersectionVector *intersections = querySTRTree(areasOfInterest, rasterTree);
      if (intersections == NULL) {
        fprintf(stderr, "No intersections found\n"); // this is not treated as an error
        // not pretty, but: areasOfInterest, data and rasterWKT are not freed here but after for-loop
        freeAverageData(&average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        someErrors = true;
        break;
      }

      // a functions that (should be split up into smaller pieces)
      // 1. converts GEOSGeometry back to OGRGeometry (via WKBExport)
      // 2. given two OGRGeometries (Polygons) computes the intersection
      // 3. a) depending on the CRS being geodesic or not, calculating the appropriate area
      // 3. b) query a WKT/dataset for property
      // 4. calculate area-weighted average
      // 5. get centroid of polygon
      meanVector *weightedMeans = calculateAreaWeightedMean(intersections, SRS_WKT_WGS84_LAT_LONG,
                                  options->footprint);
      if (weightedMeans == NULL) {
        fprintf(stderr, "Failed to calculate weighted means\n");
        freeAverageData(&average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        freeIntersections(intersections);
        someErrors = true;
        break;
      }

      char *textOutputFilePath = constructFilePath("%s/%.4d-%.2d-%.2d.txt", options->outputDirectory,
                                 currentYear, currentMonth, day);

      if (textOutputFilePath == NULL) {
        fprintf(stderr, "Failed to construct file path for output text file\n");
        freeAverageData(&average);
        freeCellGeometryList(rasterCellsAsGEOS);
        GEOSSTRtree_destroy(rasterTree);
        freeIntersections(intersections);
        someErrors = true;
        break;
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

    freeRawData(&data);

    if (!someErrors) {
#ifndef DEBUG
      char *msg = strdup("PROCESSED");
      if (msg == NULL) {
        fprintf(stderr, "Failed to allocate memory for new message. Not marking %s as processed\n",
                ptr->string);
      } else {
        free(ptr->status);
        ptr->status = msg;
      }
#endif
    } else {
      fprintf(stderr, "Encountered errors while processing %s\n", ptr->string);
    }
  }

  freeVectorGeometryList(areasOfInterest);

  if (writeUpdatedLogFile(logFileList, options->logFile)) {
    fprintf(stderr,
            "Failed to update log file. Log file and output directory are inconsistent now, clean up manually\n");
    freeStringList(logFileList);
    return 1;
  }

  freeStringList(logFileList);

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
