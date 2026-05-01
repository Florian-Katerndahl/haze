#include "area.h"
#include <gdal/ogr_api.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_srs_api.h>
#include <geodesic.h>
#include <math.h>

double fastLinearRingGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g)
{
  double area = -1.0;

  int ringPointCount = OGR_G_GetPointCount(geometry);

  double *x = malloc(ringPointCount * sizeof(double));
  double *y = malloc(ringPointCount * sizeof(double));

  if (x == NULL || y == NULL) {
    fprintf(stderr, "Failed to allocate memory for coordinates of linear ring\n");
    return -1.0;
  }

  if (OGR_G_GetPoints(geometry, x, sizeof(double), y, sizeof(double), NULL, 0) != ringPointCount) {
    free(x);
    free(y);
    fprintf(stderr, "Failed to extract points from linear ring geometry\n");
    return -1.0;
  }

  geod_polygonarea(g, y, x, ringPointCount, &area, NULL);

  free(x);
  free(y);

  return fabs(area);
}

double fastPolygonialGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g)
{
  double area = 0.0;

  int ringCount = OGR_G_GetGeometryCount(geometry);

  OGRGeometryH exteriorRing = OGR_G_GetGeometryRef(geometry, 0);

  area = fastLinearRingGeodesicArea(exteriorRing, g);

  if (area < 0) {
    fprintf(stderr, "Failed to compute area of exterior ring of polygon\n");
    return -1.0;
  }

  for (int interiorRingIndex = 1; interiorRingIndex < ringCount; interiorRingIndex++) {
    OGRGeometryH interiorRing = OGR_G_GetGeometryRef(geometry, interiorRingIndex);

    double subArea = fastLinearRingGeodesicArea(interiorRing, g);

    if (subArea < 0) {
      fprintf(stderr, "Failed to compute arae of interior ring of polygon\n");
      return -1.0;
    }

    area -= subArea;
  }

  return area;
}

double fastMultipolygonialGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g)
{
  double area = 0.0;

  int subPolygonCount = OGR_G_GetGeometryCount(geometry);

  for (int subPolygonIndex = 0; subPolygonIndex < subPolygonCount; subPolygonIndex++) {
    OGRGeometryH subPolygon = OGR_G_GetGeometryRef(geometry, subPolygonIndex);

    double subArea = fastPolygonialGeodesicArea(subPolygon, g);

    if (subArea < 0) {
      fprintf(stderr, "Failed to compute area of sub-polygon in multipolygon\n");
      return -1.0;
    }

    area += subArea;
  }

  return area;
}

double fastGeodesicArea(const OGRGeometryH geometry, const OGRSpatialReferenceH spatialReference)
{
  // did we REALLY get a valid geometry?
  if (!OGR_G_IsValid(geometry)) {
    fprintf(stderr, "Input geometry is not valid\n");
    return -1.0;
  }

  OGRErr semiMajorError = OGRERR_NONE;
  OGRErr inverseFlatteningError = OGRERR_NONE;

  double semiMajor = OSRGetSemiMajor(spatialReference, &semiMajorError);
  double inverseFlattening = OSRGetInvFlattening(spatialReference, &inverseFlatteningError);

  if (semiMajorError != OGRERR_NONE || inverseFlatteningError != OGRERR_NONE) {
    fprintf(stderr, "Failed to extract semi-major and inverse flattening from CRS\n");
    return -1.0;
  }

  struct geod_geodesic g;

  geod_init(&g, semiMajor, inverseFlattening != 0 ? 1.0 / inverseFlattening : 0.0);

  if (OGR_G_GetGeometryType(geometry) == wkbPolygon
      || OGR_G_GetGeometryType(geometry) == wkbPolygon25D) {
    return fastPolygonialGeodesicArea(geometry, &g);
  } else if (OGR_G_GetGeometryType(geometry) == wkbMultiPolygon
             || OGR_G_GetGeometryType(geometry) == wkbMultiPolygon25D) {
    return fastMultipolygonialGeodesicArea(geometry, &g);
  }

  fprintf(stderr, "Got unexpected geometry type '%s'\n", OGR_G_GetGeometryName(geometry));

  return -1.0;
}
