#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <geos_c.h>
#include <gdal/gdal.h>

// from haze
struct rawData
{
  size_t bands;
  size_t rows;
  size_t columns;
  double **data;
};

struct averagedData
{
  size_t rows;
  size_t columns;
  double **data;
};

struct geoTransform
{
  double xOrigin;
  double pixelWidth;
  double rowRotation;
  double yOrigin;
  double colRotation;
  double pixelHeight;
};

typedef struct mean
{
  double x;
  double y;
  double value;
  struct mean *next;
} mean_t;

// from strtree
struct vectorGeometry
{
  GEOSGeometry *mbr;
  GEOSGeometry *geometry;
  OGRGeometryH *OGRGeometry;
  GIntBig id;
};

typedef struct vectorGeometryList
{
  struct vectorGeometry *entry;
  struct vectorGeometryList *next;
} vectorGeometryList;

struct cellGeometry
{
  GEOSGeometry *geometry;
  double value;
};

typedef struct cellGeometryList
{
  struct cellGeometry *entry;
  struct cellGeometryList *next;
} cellGeometryList;

typedef struct intersection
{
  OGRGeometryH *reference;
  cellGeometryList *intersectingCells;
  size_t intersectionCount;
  struct intersection *next;
} intersection_t;

#endif //TYPES_H