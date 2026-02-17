#include "gdal-ops.h"
#include "types.h"
#include "fscheck.h"
#include <gdal/gdal.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_srs_api.h>
#include <stdio.h>

[[nodiscard]] GDALDatasetH openRasterDataset(const char *filePath)
{
  if (filePath == NULL || fileReadable(filePath) == false) {
    fprintf(stderr, "ERROR: filePath cannot be NULL or file is not readable\n");
    return NULL;
  }

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

[[nodiscard]] GDALDatasetH openVectorDataset(const char *filePath)
{
  if (filePath == NULL || fileReadable(filePath) == false) {
    fprintf(stderr, "ERROR: filePath cannot be NULL or file is not readable\n");
    return NULL;
  }

  GDALDatasetH vector = GDALOpenEx(filePath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
  if (vector == NULL) {
    fprintf(stderr, "Failed to read vector dataset: %s", CPLGetLastErrorMsg());
    return NULL;
  }

  return vector;
}

OGRLayerH openVectorLayer(GDALDatasetH vector, const char *name)
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
  geoTransformation->xOrigin = tmp[0];
  geoTransformation->pixelWidth = tmp[1];
  geoTransformation->rowRotation = tmp[2];
  geoTransformation->yOrigin = tmp[3];
  geoTransformation->colRotation = tmp[4];
  geoTransformation->pixelHeight = tmp[5];

  return 0;
}

CRS_TYPE getCRSType(const char *Wkt)
{
  OGRSpatialReferenceH spatialRef = OSRNewSpatialReference(Wkt);
  if (spatialRef == NULL)
    return CRS_UNKNOWN;

  bool geographic = OSRIsGeographic(spatialRef);

  OSRDestroySpatialReference(spatialRef);

  return geographic == true ? CRS_GEOGRAPHIC : CRS_PROJECTED;
}

OGRCoordinateTransformationH transformationFromWKTs(const char *from, const char *to)
{
  if (from == NULL || to == NULL) {
    return NULL;
  }

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
