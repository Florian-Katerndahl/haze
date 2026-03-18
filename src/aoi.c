#include "aoi.h"
#include "gdal-ops.h"
#include "fscheck.h"
#include <stdio.h>
#include <gdal/gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/cpl_error.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>

// NOTE: the output envelope follows cartesian logic what minx, miny, maxx, maxy mean.
// input coordinates may differ because of CRS axis mapping => latter only relevant when
// reprojecting as OGR_L_Get... honors axis mapping
[[nodiscard]] OGREnvelope *boxFromPath(const char *filePath, const char *layerName)
{
  if (fileReadable(filePath) == false) {
    return NULL;
  }

  GDALDatasetH aoi = openVectorDataset(filePath);
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
  if (OSRExportToWktEx(layerRef, &layerWKT, NULL) != OGRERR_NONE) {
    fprintf(stderr, "Failed to export layer WKT\n");
    CPLFree(mbr);
    closeGDALDataset(aoi);
    return NULL;
  }

  if (!EQUAL(layerWKT, SRS_WKT_WGS84_LAT_LONG)) {
    OGRCoordinateTransformationH transformation = transformationFromWKTs(layerWKT,
      SRS_WKT_WGS84_LAT_LONG);

    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation object: %s", CPLGetLastErrorMsg());
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
    mean: -59 lon => 59 west and 58 lat => 58 north

    It's needed because I work with the coordinates directly, would I only be interested in the geometry, e.g. for
    area calculation, this wouldn't be necessary!
    */
    int nAxes = 0;
    const int *dataAxisToSRS = OSRGetDataAxisToSRSAxisMapping(layerRef, &nAxes);
    if (nAxes != 2) {
      fprintf(stderr, "3D CRS are not supported\n");
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    // after getting data axis to srs axis, we still need to query the axis definition;
    // for now we only know the index in the WKT
    OGRAxisOrientation orientation;

    // get first axis; axes themselves are 1 indexed
    if (OSRGetAxis(layerRef, NULL, dataAxisToSRS[0] - 1, &orientation) == NULL) {
      fprintf(stderr, "Failed to get first coordinate axis form CRS\n");
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
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

    /// TODO: make sure that X and Y indeed relate to the coordinate tuple (X, Y) where the
    ///       axis order then tells what each X and Y mean; given the notes above, it seems so
    if (orientation == OAO_North || orientation == OAO_South) {
      // first axis is northing/southing
      minFirstIn = mbr->MinY;
      minSecondIn = mbr->MinX;
      maxFirstIn = mbr->MaxY;
      maxSecondIn = mbr->MaxX;
    } else if (orientation == OAO_East || orientation == OAO_West) {
      // first axis is easting/westing
      minFirstIn = mbr->MinX;
      minSecondIn = mbr->MinY;
      maxFirstIn = mbr->MaxX;
      maxSecondIn = mbr->MaxY;
    } else {
      fprintf(stderr, "Unsupported coordinate ordering\n");
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree(layerWKT);
      CPLFree(mbr);
      closeGDALDataset(aoi);
      return NULL;
    }

    const int defaultDensification = 21;

    if (OCTTransformBounds(
          transformation,
          minFirstIn, minSecondIn,
          maxFirstIn, maxSecondIn,
          &minFirstOut, &minSecondOut,
          &maxFirstOut, &maxSecondOut,
          defaultDensification) == FALSE) {
      fprintf(stderr, "Failed to transform bounding box\n");
      OCTDestroyCoordinateTransformation(transformation);
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

    if (mbr->MaxX < mbr->MinX)
      mbr->MaxX += 360.0; // cds API allows requests with x [-360,360]

    OCTDestroyCoordinateTransformation(transformation);
  }

  CPLFree(layerWKT);
  closeGDALDataset(aoi);

  if (mbr->MaxX <= mbr->MinX || mbr->MaxY <= mbr->MinY) {
    fprintf(stderr, "Only north up bounding boxes supported\n");
    CPLFree(mbr);
    closeGDALDataset(aoi);
    return NULL;
  }
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
