#include "aoi.h"
#include "haze.h"
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

struct boundingBox *allocBoundingBox(void)
{
  return calloc(1, sizeof(struct boundingBox));
}

void freeBoundingBox(struct boundingBox *box)
{
  free(box);
  box = NULL;
}

struct boundingBox *boxFromPath(const char *filePath, const char *layerName)
{
  // bigger TODO: each step could be packed into its own function which would make this one more readable
  struct boundingBox *box;

  assert(fileReadable(filePath));

  if ((box = allocBoundingBox()) == NULL) {
    perror("calloc");
    return NULL;
  }

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

  char *layerWKT;
  OGRErr wktErr = OSRExportToWktEx(layerRef, &layerWKT, NULL);
  if (wktErr != OGRERR_NONE) {
    fprintf(stderr, "Failed to export layer WKT\n");
    //TODO cleanup
    return NULL;
  }

  if (EQUAL(layerWKT, SRS_WKT_WGS84_LAT_LONG)) {
    box->left   = mbr->MinX;
    box->top    = mbr->MaxY;
    box->right  = mbr->MaxX;
    box->bottom = mbr->MinY;
  } else {
    // FIXME: handle crossing of meridians
    const char *wgs84WKT = SRS_WKT_WGS84_LAT_LONG;
    OGRSpatialReferenceH wgs84Ref = OSRNewSpatialReference(wgs84WKT);
    if (wgs84Ref == NULL) {
      fprintf(stderr, "Failed to create spatial reference object for WGS84 WKT: %s",
              CPLGetLastErrorMsg());
      // TODO cleanup
      return NULL;
    }
    OGRCoordinateTransformationH *transformation = OCTNewCoordinateTransformationEx(layerRef, wgs84Ref,
      NULL);
    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation object: %s", CPLGetLastErrorMsg());
      // TODO cleanup
      return NULL;
    }
    if (OCTTransformBounds(
          transformation,
          mbr->MinX, mbr->MinY,
          mbr->MaxX, mbr->MaxY,
          &(box->left), &(box->bottom),
          &(box->right), &(box->top),
          21) == FALSE) {
      fprintf(stderr, "Failed to transform bounding box\n");
      // todo cleanup
      return NULL;
    }

    OSRDestroySpatialReference(wgs84Ref);
    OCTDestroyCoordinateTransformation(transformation);
  }

  CPLFree(layerWKT);
  CPLFree(mbr);
  closeGDALDataset(aoi);

  return box;
}

void printBoundingBox(const struct boundingBox *box)
{
  printf(
    "bounding box: %lf (top), %lf (right), %lf (bottom), %lf (left)\n",
    box->top, box->right, box->bottom, box->left
  );
  return;
}
