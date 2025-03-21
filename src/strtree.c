#include "strtree.h"
#include "haze.h"
#include "gdal-ops.h"
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
#include <stdio.h>
#include <string.h>
#include <time.h>

[[nodiscard]] char *extractCRSAsWKT(GDALDatasetH dataset, const char *layerName)
{
  GDALDriverH driver = GDALGetDatasetDriver(dataset);
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
    OGRLayerH layer;
    if (layerName == NULL) {
      layer = OGR_DS_GetLayer(dataset, 0);
    } else {
      layer = OGR_DS_GetLayerByName(dataset, layerName);
    }
    if (layer == NULL) {
      fprintf(stderr, "Failed to get dataset layer: %s", CPLGetLastErrorMsg());
      return NULL;
    }
    OGRSpatialReferenceH ref = OGR_L_GetSpatialRef(layer);
    if (ref == NULL) {
      fprintf(stderr, "Failed to get layer CRS: %s", CPLGetLastErrorMsg());
      return NULL;
    }
    OSRExportToWkt(ref, &tempRef);
    return tempRef;
  }
}

[[nodiscard]] vectorGeometryList *buildGEOSGeometriesFromFile(const char *filePath,
    const char *layerName,
    const char *inputReferenceSystem)
{
  // todo: return `vectorGeometryList *` instead? could make it a bit nicer!
  vectorGeometryList *geometries = NULL;

  GDALDatasetH vectorDataset = openVector(filePath);
  if (vectorDataset == NULL) {
    return NULL;
  }

  OGRLayerH  layer = openVectorLayer(vectorDataset, layerName);
  if (layer == NULL) {
    fprintf(stderr, "Failed to get vector layer: %s", CPLGetLastErrorMsg());
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  OGRSpatialReferenceH  layerCRS = OGR_L_GetSpatialRef(layer); // reference is owned by dataset
  if (layerCRS == NULL) {
    fprintf(stderr, "Failed to get layer CRS: %s", CPLGetLastErrorMsg());
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  char *layerWKT;
  if (OSRExportToWktEx(layerCRS, &layerWKT, NULL) != OGRERR_NONE) {
    fprintf(stderr, "Failed to convert CRS to WKT: %s", CPLGetLastErrorMsg());
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  const bool needsReprojection = !EQUAL(inputReferenceSystem, layerWKT);

  OGRCoordinateTransformationH transformation = NULL;

  if (needsReprojection) {
    OGRSpatialReferenceH  targetReferenceSystem = OSRNewSpatialReference(inputReferenceSystem);
    if (targetReferenceSystem == NULL) {
      fprintf(stderr, "Failed to create spatial reference system for input: %s", CPLGetLastErrorMsg());
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    OGRSpatialReferenceH  sourceReferenceSystem = OSRNewSpatialReference(layerWKT);
    if (sourceReferenceSystem == NULL) {
      fprintf(stderr, "Failed to create spatial reference system for target: %s", CPLGetLastErrorMsg());
      OSRDestroySpatialReference(targetReferenceSystem);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    transformation = OCTNewCoordinateTransformationEx(sourceReferenceSystem, targetReferenceSystem,
                     NULL);
    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation between CRS's: %s", CPLGetLastErrorMsg());
      OSRDestroySpatialReference(sourceReferenceSystem);
      OSRDestroySpatialReference(targetReferenceSystem);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    OSRDestroySpatialReference(targetReferenceSystem);
    OSRDestroySpatialReference(sourceReferenceSystem);
  }

  GEOSWKBReader *reader = GEOSWKBReader_create();
  if (reader == NULL) {
    fprintf(stderr, "Failed to create WKB reader\n");
    if (needsReprojection)
      OCTDestroyCoordinateTransformation(transformation);
    CPLFree((void *) layerWKT);
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  OGR_FOR_EACH_FEATURE_BEGIN(feature, layer) {
    OGRGeometryH  geom = OGR_G_Clone(OGR_F_GetGeometryRef(feature)); // take ownership of geometry
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

    GEOSGeometry *geosMBR = GEOSGeom_createRectangle(mBr->MinX, mBr->MinY, mBr->MaxX, mBr->MaxY);
    if (geosMBR == NULL) {
      fprintf(stderr, "Failed to create GEOS rectangle\n");
      freeVectorGeometryList(geometries);
      CPLFree(mBr);
      OGR_G_DestroyGeometry(geom);
      OGR_F_Destroy(feature); // current feature as loop is not finished
      GEOSWKBReader_destroy(reader);
      if (needsReprojection)
        OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    unsigned char *OGRWkb = CPLCalloc(OGR_G_WkbSize(geom), sizeof(unsigned char));

    OGR_G_ExportToIsoWkb(geom, wkbNDR, OGRWkb); // returns OGRERR_NONE in all cases

    struct vectorGeometry *vecGeom = calloc(1, sizeof(struct vectorGeometry));
    if (vecGeom == NULL) {
      perror("calloc");
      freeVectorGeometryList(geometries);
      CPLFree(OGRWkb);
      GEOSGeom_destroy(geosMBR);
      CPLFree(mBr);
      OGR_G_DestroyGeometry(geom);
      OGR_F_Destroy(feature); // current feature as loop is not finished
      GEOSWKBReader_destroy(reader);
      if (needsReprojection)
        OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    vecGeom->geometry = GEOSWKBReader_read(reader, OGRWkb, OGR_G_WkbSize(geom));
    vecGeom->mbr = geosMBR;
    vecGeom->id = OGR_F_GetFID(feature);
    vecGeom->OGRGeometry = geom;
    if (vecGeom == NULL) {
      fprintf(stderr, "Failed to read OGR geometry as WKB into GEOS\n");
      free(vecGeom);
      freeVectorGeometryList(geometries);
      CPLFree(OGRWkb);
      GEOSGeom_destroy(geosMBR);
      CPLFree(mBr);
      OGR_G_DestroyGeometry(geom);
      OGR_F_Destroy(feature); // current feature as loop is not finished
      GEOSWKBReader_destroy(reader);
      if (needsReprojection)
        OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    // now, insert into linked list!
    vectorGeometryList *node = calloc(1, sizeof(vectorGeometryList));
    if (node == NULL) {
      perror("calloc");
      free(vecGeom);
      freeVectorGeometryList(geometries);
      CPLFree(OGRWkb);
      GEOSGeom_destroy(geosMBR);
      CPLFree(mBr);
      OGR_G_DestroyGeometry(geom);
      OGR_F_Destroy(feature); // current feature as loop is not finished
      GEOSWKBReader_destroy(reader);
      if (needsReprojection)
        OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    node->entry = vecGeom;
    node->next = NULL;
    if (geometries == NULL) {
      // first node
      geometries = node;
    } else {
      // append to front
      node->next = geometries;
      geometries = node;
    }

    CPLFree((void *) OGRWkb);
    CPLFree((void*) mBr);
  }
  OGR_FOR_EACH_FEATURE_END(feature);

  GEOSWKBReader_destroy(reader);
  if (needsReprojection)
    OCTDestroyCoordinateTransformation(transformation);
  CPLFree((void *) layerWKT);
  closeGDALDataset(vectorDataset);

  return geometries;
}

[[nodiscard]] GEOSSTRtree *buildSTRTreefromRaster(const struct averagedData *data,
    const struct geoTransform *transformation, cellGeometryList **cells)
{
  unsigned int err = 0;

  GEOSSTRtree *tree = GEOSSTRtree_create(TREE_NODE_CAP);
  if (tree == NULL) {
    fprintf(stderr, "Failed to allocate tree\n");
    return NULL;
  }

  for (size_t x = 0; x < data->columns; x++) {
    for (size_t y = 0; y < data->rows; y++) {
      double x1 = coordinateFromCell(transformation->xOrigin, (double) x, transformation->pixelWidth,
                                     (double) y, transformation->rowRotation);
      double x2 = coordinateFromCell(transformation->xOrigin, ((double) x) + 1.0,
                                     transformation->pixelWidth,
                                     (double) y, transformation->rowRotation);
      double y1 = coordinateFromCell(transformation->yOrigin, (double) y, transformation->pixelHeight,
                                     (double) x, transformation->colRotation);
      double y2 = coordinateFromCell(transformation->yOrigin, ((double) y) + 1.0,
                                     transformation->pixelHeight,
                                     (double) x, transformation->colRotation);

      // the ternary madness is needed because images may not be north-up
      GEOSGeometry *geom = GEOSGeom_createRectangle(
                             MIN(x1, x2),
                             MIN(y1, y2),
                             MAX(x1, x2),
                             MAX(y1, y2)
                           );
      if (geom == NULL) {
        fprintf(stderr, "Failed to create cell geometry\n");
        err = 1;
        break;
      }

      struct cellGeometry *cell = calloc(1, sizeof(struct cellGeometry));
      if (cell == NULL) {
        perror("calloc");
        GEOSGeom_destroy(geom); // free parts of unfinished node
        err = 1;
        break;
      }

      cell->geometry = geom;
      cell->value = *(*data->data + x + y * data->columns);

      cellGeometryList *node = calloc(1, sizeof(cellGeometryList));
      if (node == NULL) {
        perror("calloc");
        freeCellGeometry(cell);
        err = 1;
        break;
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

    if (err)
      break;
  }

  if (err) {
    freeCellGeometryList(*cells);
    GEOSSTRtree_destroy(tree);
    return NULL;
  }

  if (GEOSSTRtree_build(tree) == 0) {
    fprintf(stderr, "Failed to build tree\n");
    freeCellGeometryList(*cells);
    GEOSSTRtree_destroy(tree);
    return NULL;
  }

  return tree;
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

  // NOTE: this fucked me over - Only reference is taken, but to free memory,
  // (i.e. geom) the linked list created while building tree should be freed!
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

[[nodiscard]] intersection_t *querySTRTree(vectorGeometryList *areasOfInterest,
    GEOSSTRtree *rasterTree)
{
  intersection_t *queryResults = NULL;

  while (areasOfInterest != NULL) {
    intersection_t *node = calloc(1, sizeof(intersection_t));
    if (node == NULL) {
      perror("calloc");
      continue;
    }

    // NOTE: no ownership of areasOfInterest->entry->OGRGeometry is taken,
    // owner of `areaOfInterest` is responsible to free object!
    node->reference = areasOfInterest->entry->OGRGeometry;
    node->intersectionCount = 0;
    node->intersectingCells = NULL;

    GEOSSTRtree_query(rasterTree, areasOfInterest->entry->mbr, queryCallback,
                      (void *) &node->intersectingCells);

    if (node->intersectingCells == NULL) {
      fprintf(stderr, "No intersections found for geometry with FID %lld.\n", areasOfInterest->entry->id);
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

  return queryResults;
}
