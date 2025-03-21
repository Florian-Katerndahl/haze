#include "gdal-ops.h"
#include "types.h"
#include "fscheck.h"
#include <assert.h>
#include <gdal/gdal.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_srs_api.h>
#include <stdio.h>
#include <time.h>

[[nodiscard]] GDALDatasetH openRaster(const char *filePath)
{
  if (filePath == NULL) {
    fprintf(stderr, "ERROR: filePath cannot be NULL\n");
    return NULL;
  }

  assert(fileReadable(filePath));

  // TODO could read through entire directory when using sibling files?
  GDALDatasetH raster = GDALOpenEx(filePath, GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
  if (raster == NULL) {
    fprintf(stderr, "ERROR: Failed to open raster dataset %s\n", filePath);
    return NULL;
  }

  return raster;
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

[[nodiscard]] GDALDatasetH openVector(const char *filePath)
{
  if (filePath == NULL) {
    fprintf(stderr, "ERROR: filePath cannot be NULL\n");
    return NULL;
  }

  assert(fileReadable(filePath));

  GDALDatasetH vector = GDALOpenEx(filePath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
  if (vector == NULL) {
    fprintf(stderr, "Failed to read vector dataset: %s", CPLGetLastErrorMsg());
    return NULL;
  }

  return vector;
}

OGRLayerH  openVectorLayer(GDALDatasetH vector, const char *name)
{
  if (name) {
    return OGR_DS_GetLayerByName(vector, name);
  } else {
    return OGR_DS_GetLayer(vector, 0);
  }
}

void closeGDALDataset(GDALDatasetH dataset)
{
  if (GDALClose(dataset) != CE_None) {
    exit(1); // I mean, if this fails why continue at all?
  }
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

OGRCoordinateTransformationH transformationFromWKTs(const char *from, const char *to)
{
  assert(from && to);

  OGRCoordinateTransformationH transform = NULL;

  OGRSpatialReferenceH sourceReferenceSystem = OSRNewSpatialReference(from);
  if (sourceReferenceSystem == NULL) {
    fprintf(stderr, "Failed to create spatial reference system for source: %s", CPLGetLastErrorMsg());
    return NULL;
  }

  OGRSpatialReferenceH targetReferenceSystem = OSRNewSpatialReference(to);
  if (targetReferenceSystem == NULL) {
    fprintf(stderr, "Failed to create spatial reference system for target: %s", CPLGetLastErrorMsg());
    OSRDestroySpatialReference(sourceReferenceSystem);
    return NULL;
  }

  transform = OCTNewCoordinateTransformationEx(
                sourceReferenceSystem,
                targetReferenceSystem,
                NULL);
  if (transform == NULL) {
    fprintf(stderr, "Failed to create transformation between CRS's: %s", CPLGetLastErrorMsg());
    OSRDestroySpatialReference(targetReferenceSystem);
    OSRDestroySpatialReference(sourceReferenceSystem);
    return NULL;
  }

  OSRDestroySpatialReference(targetReferenceSystem);
  OSRDestroySpatialReference(sourceReferenceSystem);

  return transform;
}
