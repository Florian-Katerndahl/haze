/**
 * @file types.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header files defines function signatures for various rather generic types as well as
 *        all custom types and constants used in haze.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <geos_c.h>
#include <gdal/gdal.h>

#define MAXYEAR 100
#define MAXMONTH 12
#define MAXDAY 31
#define MAXHOUR 24

// from gdal-ops
typedef enum
{
  CRS_GEOGRAPHIC,
  CRS_PROJECTED,
  CRS_UNKNOWN
} CRS_TYPE;

// from haze
struct rawData
{
  size_t bands;
  size_t rows;
  size_t columns;
  double *data;
};

struct averagedData
{
  size_t rows;
  size_t columns;
  double *data;
};

/**
 * @struct geoTransform
 * @brief This structs associates GDAL's geotransfomration information from a raster
 *        dataset with easy to interpret names.
 */
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

typedef struct userdata
{
  const GEOSPreparedGeometry *queryGeometry;
  cellGeometryList *intersectingCells;
  size_t intersectionCount;
} userdata_t;

/**
 * @brief Free a single OGR vector geometry node and all encapsulated fields
 *
 * @param node Node to free
 */
void freeVectorGeometry(struct vectorGeometry *node);

/**
 * @brief Free linked list of OGR vector geometries
 *
 * @param list List to free
 */
void freeVectorGeometryList(vectorGeometryList *list);

/**
 * @brief Free a single cell geometry node and the encapsulated GEOS geometry
 *
 * @param node Node to free
 */
void freeCellGeometry(struct cellGeometry *node);

/**
 * @brief Free linked list of cell geometries
 *
 * @details Each entry consists of a geometry created by GEOS and an associated value
 *
 * @param list
 */
void freeCellGeometryList(cellGeometryList *list);

/**
 * @brief Free linked list of intersecting geometries
 *
 * @details This frees both the entries in list as well as the list of geometries
 *          intersecting each entry in `list`.
 *
 * @param list List to free
 */
void freeIntersections(intersection_t *list);

/**
 * @brief Free list containing centroids of AOI geometries and associated water column value
 *
 * @param list List to free
 */
void freeWeightedMeans(mean_t *list);

// options
typedef struct options
{
  bool printHelp;
  int years[MAXYEAR];
  size_t yearsElements;
  int months[MAXMONTH];
  size_t monthsElements;
  int days[MAXDAY];
  size_t daysElements;
  int hours[MAXHOUR];
  size_t hoursElements;
  char *areaOfInterest;
  char *outputDirectory;
  char *authenticationToken;
} option_t;

/**
 * @brief Free all heap-allocated objects within the `option` object, including the reference itself
 *
 * @param option Object to free
 */
void freeOption(option_t *option);

// API communication etc
typedef struct curlString
{
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

typedef struct stringList
{
  char *string;
  struct stringList *next;
} stringList;

/**
 * @brief Free linked list of strings
 *
 * @param list Nodes to free
 */
void freeStringList(stringList *list);

#endif //TYPES_H