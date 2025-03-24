#include "aoi.h"
#include "gdal-ops.h"
#include "fscheck.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gdal/gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/cpl_error.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <time.h>

[[nodiscard]] OGREnvelope *boxFromPath(const char *filePath, const char *layerName)
{
  assert(fileReadable(filePath));

  GDALDatasetH aoi = openVector(filePath);
  if (aoi == NULL) {
    return NULL;
  }

  if (GDALDatasetGetLayerCount(aoi) == 0) {
    fprintf(stderr, "Provided vector file does not conain layers\n");
    closeGDALDataset(aoi);
    return NULL;
  }

  OGRLayerH layer = openVectorLayer(aoi, layerName);
  if (layer == NULL) {
    fprintf(stderr, "Failed to get layer from dataset\n");
    closeGDALDataset(aoi);
    return NULL;
  }

  OGRSpatialReferenceH layerRef = OGR_L_GetSpatialRef(layer);
  if (layerRef == NULL) {
    fprintf(stderr, "Spatial reference not available\n");
    closeGDALDataset(aoi);
    return NULL;
  }

  OGREnvelope *mbr = CPLCalloc(1, sizeof(OGREnvelope));
  if (OGR_L_GetExtent(layer, mbr, 1) == OGRERR_FAILURE) {
    fprintf(stderr, "Failed to get layer extent: %s", CPLGetLastErrorMsg());
    closeGDALDataset(aoi);
    return NULL;
  }

  if (mbr->MaxX <= mbr->MinX || mbr->MaxY <= mbr->MinY) {
    fprintf(stderr, "Only north up bounding boxes supported\n");
    CPLFree(mbr);
    closeGDALDataset(aoi);
    return NULL;
  }

  char *layerWKT;
  if (OSRExportToWktEx(layerRef, &layerWKT, NULL) != OGRERR_NONE) {
    fprintf(stderr, "Failed to export layer WKT\n");
    CPLFree(mbr);
    closeGDALDataset(aoi);
    return NULL;
  }

  if (!EQUAL(layerWKT, SRS_WKT_WGS84_LAT_LONG)) {
    // FIXME: handle crossing of meridians
    const char *const wgs84WKT = SRS_WKT_WGS84_LAT_LONG;

    OGRSpatialReferenceH wgs84Ref = OSRNewSpatialReference(wgs84WKT);
    if (wgs84Ref == NULL) {
      fprintf(stderr, "Failed to create spatial reference object for WGS84 WKT: %s",
              CPLGetLastErrorMsg());
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    OGRCoordinateTransformationH *transformation = OCTNewCoordinateTransformationEx(layerRef, wgs84Ref,
      NULL);
    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation object: %s", CPLGetLastErrorMsg());
      OSRDestroySpatialReference(wgs84Ref);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    OGREnvelope *tmp = CPLCalloc(1, sizeof(OGREnvelope));

    if (OCTTransformBounds(
          transformation,
          mbr->MinX, mbr->MinY,
          mbr->MaxX, mbr->MaxY,
          &(tmp->MinX), &(tmp->MinY),
          &(tmp->MaxX), &(tmp->MaxY),
          21) == FALSE) {
      fprintf(stderr, "Failed to transform bounding box\n");
      CPLFree(tmp);
      OCTDestroyCoordinateTransformation(transformation);
      OSRDestroySpatialReference(wgs84Ref);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    CPLFree(mbr);
    mbr = tmp;

    OSRDestroySpatialReference(wgs84Ref);
    OCTDestroyCoordinateTransformation(transformation);
  }

  CPLFree(layerWKT);
  closeGDALDataset(aoi);

  return mbr;
}

void printBoundingBox(const OGREnvelope *box)
{
  printf(
    "bounding box: %lf (top), %lf (right), %lf (bottom), %lf (left)\n",
    box->MaxY, box->MaxX, box->MinY, box->MinX
  );
  return;
}
