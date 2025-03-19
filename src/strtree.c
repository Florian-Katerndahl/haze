#include "strtree.h"
#include "fscheck.h"
#include "aoi.h"
#include "types.h"
#include <assert.h>
#include <float.h>
#include <gdal/cpl_conv.h>
#include <gdal/cpl_error.h>
#include <gdal/cpl_port.h>
#include <gdal/cpl_string.h>
#include <gdal/gdal.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_srs_api.h>
#include <geos_c.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

bool nearEqual(double a, double b)
{
  double epsilon = 128.0 * DBL_EPSILON;
  double abs_th = DBL_MIN;

  if (a == b)
    return true;

  double absDiff = fabs(a - b);
  double norm = fmin(fabs(a) + fabs(b), DBL_MAX);

  return absDiff < fmax(abs_th, epsilon * norm);
}

bool gte(double a, double b)
{
  return a > b || nearEqual(a, b);
}

bool lte(double a, double b)
{
  return a < b || nearEqual(a, b);
}

bool intersect(const struct boundingBox *a, const struct boundingBox *b)
{
  // TODO: actually, I need the boundingBox to be able to hold data; either
  //       the value directly or some pointer :thinking_face:
  return lte(a->left, b->right) && gte(a->right, b->left) &&
         lte(a->top, b->bottom) && gte(a->bottom, b->top);
}

void freeVectorGeometry(struct vectorGeometry *node)
{
  OGR_G_DestroyGeometry(node->OGRGeometry);
  GEOSGeom_destroy(node->geometry);
  GEOSGeom_destroy(node->mbr);
  free(node);
  node = NULL;
}

char *extractCRSAsWKT(GDALDatasetH *dataset, const char *layerName)
{
  GDALDriverH *driver = GDALGetDatasetDriver(dataset);
  char **driverMetadata = GDALGetMetadata(driver, NULL);
  char *tempRef;
  if (CSLFetchBoolean(driverMetadata, GDAL_DCAP_RASTER, FALSE)) {
    // raster dataset
    tempRef = (char *) GDALGetProjectionRef(dataset);
    if (tempRef == NULL) {
      fprintf(stderr, "Failed to get dataset CRS: %s", CPLGetLastErrorMsg());
      return NULL;
    }
    return CPLStrdup(tempRef);
  } else {
    OGRLayerH *layer;
    if (layerName == NULL) {
      layer = OGR_DS_GetLayer(dataset, 0);
    } else {
      layer = OGR_DS_GetLayerByName(dataset, layerName);
    }
    if (layer == NULL) {
      fprintf(stderr, "Failed to get dataset layer: %s", CPLGetLastErrorMsg());
      return NULL;
    }
    OGRSpatialReferenceH *ref = OGR_L_GetSpatialRef(layer);
    if (ref == NULL) {
      fprintf(stderr, "Failed to get layer CRS: %s", CPLGetLastErrorMsg());
      return NULL;
    }
    OSRExportToWkt(ref, &tempRef);
    return tempRef;
  }
}

int buildGEOSGeometriesFromGDAL(const char *filePath, const char *layerName,
                                vectorGeometryList **geometries, const char *inputReferenceSystem)
{
  int retVal = 0;
  assert(fileReadable(filePath));

  GDALAllRegister();

  GDALDatasetH *vectorDataset = GDALOpenEx(filePath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL,
                                NULL);
  if (vectorDataset == NULL) {
    fprintf(stderr, "Failed to read vector dataset: %s", CPLGetLastErrorMsg());
    retVal  = 1; // stupid, but keep it consistent
    return retVal;
  }

  OGRLayerH *layer;
  if (layerName == NULL) {
    layer = OGR_DS_GetLayer(vectorDataset, 0);
  } else {
    layer = OGR_DS_GetLayerByName(vectorDataset, layerName);
  }
  if (layer == NULL) {
    fprintf(stderr, "Failed to get vector layer: %s", CPLGetLastErrorMsg());
    //todo cleanup
    return 1;
  }

  OGRSpatialReferenceH *layerCRS = OGR_L_GetSpatialRef(layer);
  if (layerCRS == NULL) {
    fprintf(stderr, "Failed to get layer CRS: %s", CPLGetLastErrorMsg());
    // TODO: cleanup
    return 1;
  }

  char *layerWKT;
  if (OSRExportToWktEx(layerCRS, &layerWKT, NULL) != OGRERR_NONE) {
    fprintf(stderr, "Failed to convert CRS to WKT: %s", CPLGetLastErrorMsg());
    // todo cleanup
    return 1;
  }

  bool needsReprojection = !EQUAL(inputReferenceSystem, layerWKT);
  OGRCoordinateTransformationH *transformation = NULL;
  if (needsReprojection) {
    OGRSpatialReferenceH *targetReferenceSystem = OSRNewSpatialReference(inputReferenceSystem);
    if (targetReferenceSystem == NULL) {
      fprintf(stderr, "Failed to create spatial reference system for input: %s", CPLGetLastErrorMsg());
      // todo cleanup
      return 1;
    }
    OGRSpatialReferenceH *sourceReferenceSystem = OSRNewSpatialReference(layerWKT);
    if (sourceReferenceSystem == NULL) {
      fprintf(stderr, "Failed to create spatial reference system for target: %s", CPLGetLastErrorMsg());
      // todo cleanup
      return 1;
    }
    transformation = OCTNewCoordinateTransformationEx(sourceReferenceSystem, targetReferenceSystem,
                     NULL);
    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation between CRS's: %s", CPLGetLastErrorMsg());
      // todo cleanup
      return 1;
    }
    OSRDestroySpatialReference(targetReferenceSystem);
    OSRDestroySpatialReference(sourceReferenceSystem);
  }

  // IMPORTANT TODO: GDAL has OGR_G_GeodesicArea -> could be used for geodesic frational area calculation!
  // if crs is projected can be queried using OSRGetCRSInfoListFromDatabase, if not not geodesic, use OGR_G_Area

  GEOSWKBReader *reader = GEOSWKBReader_create();
  OGR_FOR_EACH_FEATURE_BEGIN(feature, layer) {
    OGRGeometryH *geom = OGR_G_Clone(OGR_F_GetGeometryRef(feature)); // take ownership of geometry
    if (wkbFlatten(OGR_G_GetGeometryType(geom)) != wkbPolygon) {
      printf("Feature with fid %lld is not a polygon, skipping\n", OGR_F_GetFID(feature));
      OGR_G_DestroyGeometry(geom);
      continue;
    }

    // WARNING: OGR_G_Transform does not handle the antimeridian (see docs)!!
    if (needsReprojection && OGR_G_Transform(geom, transformation) != OGRERR_NONE) {
      fprintf(stderr, "Failed to transform geometry: %s\n", CPLGetLastErrorMsg());
      OGR_G_DestroyGeometry(geom);
      continue;
    }

    OGREnvelope *mBr = CPLCalloc(1, sizeof(OGREnvelope));
    OGR_G_GetEnvelope(geom, mBr);

    GEOSGeometry *geosMBR = GEOSGeom_createRectangle(mBr->MinX, mBr->MinY, mBr->MaxX,
                            mBr->MaxY); // todo how is error handling done?
    unsigned char *OGRWkb = CPLCalloc(OGR_G_WkbSize(geom), sizeof(unsigned char));
    OGR_G_ExportToIsoWkb(geom, wkbNDR, OGRWkb); // returns OGRERR_NONE in all cases

    struct vectorGeometry *vecGeom = calloc(1, sizeof(struct vectorGeometry));
    if (vecGeom == NULL) {
      perror("calloc");
      OGR_G_DestroyGeometry(geom);
      CPLFree(mBr);
      GEOSGeom_destroy(geosMBR);
      CPLFree(OGRWkb);
      retVal = 1;
      break; // don't return but free stuff further down below
    }

    vecGeom->geometry = GEOSWKBReader_read(reader, OGRWkb,
                                           OGR_G_WkbSize(geom)); // todo how is error handling done?
    vecGeom->mbr = geosMBR;
    vecGeom->id = OGR_F_GetFID(feature);
    vecGeom->OGRGeometry = geom;

    // now, insert into linked list!
    vectorGeometryList *node = calloc(1, sizeof(vectorGeometryList));
    if (node == NULL) {
      perror("calloc");
      OGR_G_DestroyGeometry(geom);
      CPLFree(mBr);
      GEOSGeom_destroy(geosMBR);
      CPLFree(OGRWkb);
      free(vecGeom);
      retVal = 1;
      break; // don't return but free stuff further down below
    }
    node->entry = vecGeom;
    node->next = NULL;
    if (*geometries == NULL) {
      // first node
      *geometries = node;
    } else {
      // append to front
      node->next = *geometries;
      *geometries = node;
    }

    CPLFree((void *) OGRWkb);
    CPLFree((void*) mBr);
  }
  OGR_FOR_EACH_FEATURE_END(feature);

  GEOSWKBReader_destroy(reader);
  if (needsReprojection)
    OCTDestroyCoordinateTransformation(transformation);
  CPLFree((void *) layerWKT);
  GDALClose(vectorDataset);

  return retVal;
}

GEOSSTRtree *buildSTRTreefromRaster(const struct averagedData *data,
                                    const struct geoTransform *transformation, cellGeometryList **cells)
{
  GEOSSTRtree *tree = GEOSSTRtree_create(TREE_NODE_CAP);
  if (tree == NULL) {
    fprintf(stderr, "Failed to allocate tree\n");
    return NULL;
  }

  for (size_t x = 0; x < data->columns; x++) {
    for (size_t y = 0; y < data->rows; y++) {
      double x1 = transformation->xOrigin + (double) x * transformation->pixelWidth +
                  (double) y * transformation->rowRotation;
      double x2 = transformation->xOrigin + ((double) x + 1.0) * transformation->pixelWidth +
                  (double) y * transformation->rowRotation;
      double y1 = transformation->yOrigin + (double) x * transformation->colRotation +
                  (double) y * transformation->pixelHeight;
      double y2 = transformation->yOrigin + (double) x * transformation->colRotation + ((
                    double) y + 1.0) * transformation->pixelHeight;

      // the ternary madness is needed because images may not be north-up
      GEOSGeometry *geom = GEOSGeom_createRectangle(
                             x1 > x2 ? x2 : x1,
                             y1 > y2 ? y2 : y1,
                             x1 > x2 ? x1 : x2,
                             y1 > y2 ? y1 : y2
                           );
      if (geom == NULL) {
        fprintf(stderr, "Failed to create cell geometry\n");
        // todo cleanup
        return NULL;
      }

      struct cellGeometry *cell = calloc(1, sizeof(struct cellGeometry));
      if (cell == NULL) {
        perror("calloc");
        // todo cleanup
        return NULL;
      }

      cell->geometry = geom;
      cell->value = *data->data + x + y *
                    data->columns; // todo make clear, that ownership is not passed and dataset handle is stillresponsible for freeing data

      cellGeometryList *node = calloc(1, sizeof(cellGeometryList));
      if (node == NULL) {
        perror("calloc");
        // todo cleanup
        return NULL;
      }

      node->entry = cell;
      node->next = NULL;

      if (*cells == NULL) {
        *cells = node;
      } else {
        node->next = *cells;
        *cells = node;
      }

      GEOSSTRtree_insert(tree, cell->geometry, (void *) cell);
    }
  }

  if (GEOSSTRtree_build(tree) == 0) {
    fprintf(stderr, "Failed to build tree\n");
    // todo cleanup
    return NULL;
  }

  return tree;
}

void freeCellGeometryList(cellGeometryList *list)
{
  cellGeometryList *node;
  while (list != NULL) {
    GEOSGeom_destroy(list->entry->geometry);
    free(list->entry);
    node = list;
    list = list->next;
    free(node);
  }
}

void queryCallback(void *item, void *userdata)
{
  // fprintf(stderr, "Found intersecting bbox..");
  cellGeometryList **l = (cellGeometryList **) userdata;
  struct cellGeometry *geom = (struct cellGeometry *) item;

  cellGeometryList *node = calloc(1, sizeof(cellGeometryList));
  if (node == NULL) {
    perror("calloc");
    return;
  }

  node->entry = geom;
  node->next = NULL;

  if (*l == NULL) {
    *l = node;
  } else {
    node->next = *l;
    *l = node;
  }

  return;
}

intersection_t *querySTRTree(vectorGeometryList *areasOfInterest, GEOSSTRtree *rasterTree)
{
  // TODO: a linked list is probably not the smartest data structure to use here...
  intersection_t *queryResults = NULL;

  while (areasOfInterest != NULL) {
    intersection_t *node = calloc(1, sizeof(intersection_t));
    if (node == NULL) {
      perror("calloc");
      continue;
    }

    node->reference = areasOfInterest->entry->OGRGeometry;
    node->intersectionCount = 0;
    node->intersectingCells = NULL;

    GEOSSTRtree_query(rasterTree, areasOfInterest->entry->mbr, queryCallback,
                      (void *) &node->intersectingCells);

    if (node->intersectingCells == NULL) {
      fprintf(stderr, "Failed to get intersecting cells for item %lld\n", areasOfInterest->entry->id);
      free(node);
      continue;
    } else {
      cellGeometryList *temp = node->intersectingCells;
      while (temp != NULL) {
        node->intersectionCount++;
        temp = temp->next;
      }
    }

    if (queryResults == NULL) {
      queryResults = node;
    } else {
      node->next = queryResults;
      queryResults = node;
    }

    areasOfInterest = areasOfInterest->next;
  }

  fprintf(stderr, "checked all geometries\n");

  return queryResults;
}
