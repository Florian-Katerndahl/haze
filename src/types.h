#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <geos_c.h>
#include <gdal/gdal.h>

#define MAXYEAR 100
#define MAXMONTH 12
#define MAXDAY 31
// allocate one hour more than possible maximum to break on INITVAL in case user specifies complete hour range
#define MAXHOUR 25

#define INITVAL -1

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
  OGRGeometryH OGRGeometry;
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
  OGRGeometryH reference;
  cellGeometryList *intersectingCells;
  size_t intersectionCount;
  struct intersection *next;
} intersection_t;

void freeVectorGeometry(struct vectorGeometry *node);

void freeVectorGeometryList(vectorGeometryList *list);

void freeCellGeometry(struct cellGeometry *node);

void freeCellGeometryList(cellGeometryList *list);

void freeIntersections(intersection_t *list);

// options
typedef struct options
{
  bool printHelp;
  int years[MAXYEAR];
  int months[MAXMONTH];
  int days[MAXDAY];
  int hours[MAXHOUR];
  char *areaOfInterest;
  char *outputDirectory;
  char *authenticationToken;
  bool withAllocation;
} option_t;

void freeOption(option_t *option);

// API communication etc
typedef struct curlString {
  char *string;
  size_t length;
} curlString;

typedef enum
{
  ACCEPTED,
  SUCCESSFUL,
  RUNNING,
  FAILED,
  ERROR
} productStatus;

typedef struct stringList {
  char *string;
  struct stringList *next;
} stringList;

void freeStringList(stringList *list);

#endif //TYPES_H