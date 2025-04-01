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

// NOTE: the output envelope follows cartesian logic what minx, miny, maxx, maxy mean.
// input coordinates may differ because of CRS axis mapping => latter only relevant when
// reprojecting as OGR_L_Get... honors axis mapping
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

    OGRCoordinateTransformationH transformation = OCTNewCoordinateTransformationEx(layerRef, wgs84Ref,
      NULL);
    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation object: %s", CPLGetLastErrorMsg());
      OSRDestroySpatialReference(wgs84Ref);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    /* the data axis to crs mapping describes which number describing a certain feature (e.g. point)
    correspond to the corresponding CRS axis. Thus an axis mapping of 2,1, a point (-59.596902314751 58.314548064863)
    and the following WKT snippet
      WKT CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
          ORDER[1],
          ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
          ORDER[2],
          ANGLEUNIT["degree",0.0174532925199433]],
      mean: -59 lon => 60 west and 58 lat => 58 north
    */
    int nAxes = 0;
    const int *dataAxisToSRS = OSRGetDataAxisToSRSAxisMapping(layerRef, &nAxes);
    if (nAxes != 2) {
      fprintf(stderr, "3D CRS are not supported\n");
      // todo cleanup
      return NULL;
    }

    // after getting data axis to srs axis, we still need to query the axis definition; for now
    // we only know the index in the WKT
    OGRAxisOrientation orientation;
    // get first axis; axes themselves are 1 indexed
    OSRGetAxis(layerRef, NULL, dataAxisToSRS[0] - 1, &orientation);

    if (orientation == OAO_West || orientation == OAO_South) {
      fprintf(stderr, "First CRS axis is neither 'north' or 'east'\n");
      // todo cleanup
      return NULL;
    }

    double minFirstIn = 0;
    double minSecondIn = 0;
    double maxFirstIn = 0;
    double maxSecondIn = 0;
    double minFirstOut = 0;
    double minSecondOut = 0;
    double maxFirstOut = 0;
    double maxSecondOut = 0;

    if (orientation == OAO_North) {
      // first axis is latitude/northing
      fprintf(stderr, "First data axis is latitude/northing\n");
      minFirstIn = mbr->MinY;
      minSecondIn = mbr->MinX;
      maxFirstIn = mbr->MaxY;
      maxSecondIn = mbr->MaxX;
    } else if (orientation == OAO_East) {
      // first axis is longitude/easting
      fprintf(stderr, "First data axis is longitude/easting\n");
      minFirstIn = mbr->MinX;
      minSecondIn = mbr->MinY;
      maxFirstIn = mbr->MaxX;
      maxSecondIn = mbr->MaxY;
    }

    // check if we cross the antimeridian following strictly the docs from GDAL (which I find confusing and don't understand)
    // only applies if destination is geographic (which it always is in this case)
    // if ((orientation == OAO_East && maxFirstIn < minFirstIn) || (orientation == OAO_North && maxSecondIn < minSecondIn)) {
    //   fprintf(stderr, "Bounds crossed antimeridian\n");
    //   // todo cleanup
    //   return NULL;
    // }

    if (OCTTransformBounds(
          transformation,
          minFirstIn, minSecondIn,
          maxFirstIn, maxSecondIn,
          &minFirstOut, &minSecondOut,
          &maxFirstOut, &maxSecondOut,
          21) == FALSE) {
      fprintf(stderr, "Failed to transform bounding box\n");
      OCTDestroyCoordinateTransformation(transformation);
      OSRDestroySpatialReference(wgs84Ref);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    // for the output, we don't need to check which axis is first since we stay in WGS84
    // here, first axis (i.e. Y, see above WKT snippet) is always latitude as long as GDAL does not
    // change the definition string; data to axis mapping does not apply here.
    mbr->MinY = minFirstOut;
    mbr->MinX = minSecondOut;
    mbr->MaxY = maxFirstOut;
    mbr->MaxX = maxSecondOut;

    // printf("%lf %lf %lf %lf\n", mbr->MinX, mbr->MinY, mbr->MaxX, mbr->MaxY);

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
