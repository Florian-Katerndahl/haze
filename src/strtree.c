#include "strtree.h"
#include "haze.h"
#include "gdal-ops.h"
#include "types.h"
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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/// TODO: move to gdal-ops
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

[[nodiscard]] vectorGeometryVector *buildGEOSGeometriesFromFile(const char *filePath,
    const char *layerName,
    const char *inputReferenceSystem)
{
  vectorGeometryVector *geometries = malloc(sizeof(vectorGeometryVector));

  if (geometries == NULL) {
    fprintf(stderr, "Failed to allocate memory for vector of GEOS geometries generated from AOI file\n");
    return NULL;
  }

  GDALDatasetH vectorDataset = openVectorDataset(filePath);
  if (vectorDataset == NULL) {
    return NULL;
  }

  OGRLayerH layer = openVectorLayer(vectorDataset, layerName);
  if (layer == NULL) {
    fprintf(stderr, "Failed to get vector layer: %s", CPLGetLastErrorMsg());
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  switch (wkbFlatten(OGR_L_GetGeomType(layer))) {
    // allowed geometry types
    case wkbPolygon:
      [[fallthrough]];
    case wkbMultiPolygon:
      break;

    default:
      fprintf(stderr, "Layer has unsupported geometry type. "
              "Allowed types are: Polygon and Multipolygon.\n");
      closeGDALDataset(vectorDataset);
      return NULL;
  }

  size_t featureCount = OGR_L_GetFeatureCount(layer, 1);

  OGRSpatialReferenceH layerCRS = OGR_L_GetSpatialRef(layer); // reference is owned by dataset
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

  geometries->entries = malloc(featureCount * sizeof(struct vectorGeometry));
  geometries->size = featureCount;

  if (geometries->entries == NULL) {
    fprintf(stderr, "Failed to allocate memory for array of vector geometries\n");
    free(geometries); /// TODO: dedicated function to free vectorGeometryVector later on?
    closeGDALDataset(vectorDataset);
    return NULL;
  }

  OGRCoordinateTransformationH transformation = NULL;
  CSLConstList transformerAddonOptions = NULL;
  OGRGeomTransformerH transformer = NULL;

  if (needsReprojection) {
    transformation = transformationFromWKTs(layerWKT, (char *) inputReferenceSystem, false);

    if (transformation == NULL) {
      fprintf(stderr, "Failed to create transformation between CRS's: %s", CPLGetLastErrorMsg());
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    /// FIXME: Not sure anymore if wrapdateline makes sense. If splitting is done the same for downlading and processing, ok. Otherwise: split at both or at none
    if ((transformerAddonOptions = CSLAddStringMayFail(transformerAddonOptions,
                                   "WRAPDATELINE=YES")) == NULL) {
      fprintf(stderr, "Failed to create CRS transformer options\n");
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    transformer = OGR_GeomTransformer_Create(transformation, transformerAddonOptions);

    if (transformer == NULL) {
      fprintf(stderr, "Failed to create coordinate transformer object\n");
      CSLDestroy(transformerAddonOptions);
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }
  }

  size_t featureIndex = 0;

  OGR_FOR_EACH_FEATURE_BEGIN(feature, layer) {
    // take ownership of geometry both because it's possibly reprojected and inserted to linked list
    // with longer lifetime then the original feature layer
    OGRGeometryH geom = OGR_G_Clone(OGR_F_GetGeometryRef(feature));

    if (needsReprojection) {
      OGRGeometryH transformedGeometry = OGR_GeomTransformer_Transform(transformer, geom);

      // Always destroy original `geom`: `geom` is either replaced, or we encountered an error,
      // in which case we jump to top of loop and must destroy it as well
      OGR_G_DestroyGeometry(geom);

      if (transformedGeometry == NULL) {
        fprintf(stderr, "Failed to transform geometry: %s\n", CPLGetLastErrorMsg());
        continue;
      }

      geom = transformedGeometry;
    }

    geometries->entries[featureIndex].geometry = OGRToGEOS(geom);
    geometries->entries[featureIndex].mbr = boundingBoxOfOGRToGEOS(geom);
    geometries->entries[featureIndex].id = OGR_F_GetFID(feature);
    geometries->entries[featureIndex].OGRGeometry = geom;
    if (geometries->entries[featureIndex].geometry == NULL || geometries->entries[featureIndex].mbr == NULL) {
      fprintf(stderr, "Failed to convert OGR geometry to GEOS\n");
      GEOSGeom_destroy(geometries->entries[featureIndex].geometry);
      GEOSGeom_destroy(geometries->entries[featureIndex].mbr);
      freeVectorGeometryList(geometries);
      OGR_G_DestroyGeometry(geom);
      OGR_F_Destroy(feature); // current feature as loop is not finished
      CSLDestroy(transformerAddonOptions);
      OGR_GeomTransformer_Destroy(transformer);
      OCTDestroyCoordinateTransformation(transformation);
      CPLFree((void *) layerWKT);
      closeGDALDataset(vectorDataset);
      return NULL;
    }

    featureIndex++;

    if (featureIndex > geometries->size) {
      fprintf(stderr, "Encountered more iterations than features exist\n");
      /// TODO: cleanup
      return NULL;
    }
  }
  OGR_FOR_EACH_FEATURE_END(feature);

  CSLDestroy(transformerAddonOptions);
  OGR_GeomTransformer_Destroy(transformer);
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

      // as per GDAL's RFC 73, the raster drivers use *gis-friendly* axis ordering; no further changes needed here!
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
      cell->value = data->data[x + y * data->columns];

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

void trackIntersectingGeometries(void *item, void *userdata)
{
  userdata_t *ud = (userdata_t *) userdata;
  struct cellGeometry *geom = (struct cellGeometry *) item;

  switch (GEOSPreparedIntersects(ud->queryGeometry, geom->geometry)) {
    case 0:
      return; // actual geometries do not intersect, nothing to do
    case 1:
      break; // actual geometries do intersect
    case 2:
      fprintf(stderr, "Failed to test for intersection of geometries.\n");
      return;
    default:
      __builtin_unreachable();
  }

  ud->intersectionCount++;

  cellGeometryList *node = calloc(1, sizeof(cellGeometryList));
  if (node == NULL) {
    perror("calloc");
    return;
  }

  // NOTE: this fucked me over - Only reference is taken, but to free memory,
  // (i.e. geom) the linked list created while building tree should be freed!
  node->entry = geom;
  node->next = NULL;

  if (ud->intersectingCells == NULL) {
    ud->intersectingCells = node;
  } else {
    node->next = ud->intersectingCells;
    ud->intersectingCells = node;
  }

  return;
}

[[nodiscard]] intersectionVector *querySTRTree(vectorGeometryVector *areasOfInterest,
    GEOSSTRtree *rasterTree)
{
  intersectionVector *queryResults = malloc(sizeof(intersectionVector));
  if (queryResults == NULL) {
    fprintf(stderr, "Failed to allocate memory for vector of STRTree query results\n");
    return NULL;
  }

  queryResults->entries = malloc(areasOfInterest->size * sizeof(struct i));
  queryResults->size = areasOfInterest->size;
  size_t queryResultsEntries = 0;

  if (queryResults->entries == NULL) {
    fprintf(stderr, "Failed to allocate memory for array of STRTRee query results\n");
    /// TODO: implement proper cleanup for intersectionVector
    free(queryResults);
    return NULL;
  }

  for (size_t i = 0; i < areasOfInterest->size; i++) {
    userdata_t userdata = {
      .queryGeometry = GEOSPrepare(areasOfInterest->entries[i].geometry),
      .intersectingCells = NULL,
      .intersectionCount = 0
    };

    if (userdata.queryGeometry == NULL) {
      fprintf(stderr, "Failed to prepare geometry for FID %lld\n", areasOfInterest->entries[i].id);
      continue;
    }

    GEOSSTRtree_query(rasterTree, areasOfInterest->entries[i].mbr, trackIntersectingGeometries,
                      (void *) &userdata);

    GEOSPreparedGeom_destroy(userdata.queryGeometry);

    if (userdata.intersectingCells == NULL) {
      fprintf(stderr, "No intersections found for geometry with FID %lld.\n", areasOfInterest->entries[i].id);
      continue;
    }

    /// NOTE: no ownership of areasOfInterest->entry->OGRGeometry is taken,
    ///       owner of `areaOfInterest` is responsible to free object!
    queryResults->entries[queryResultsEntries].reference = areasOfInterest->entries[i].OGRGeometry;
    queryResults->entries[queryResultsEntries].referenceFID = areasOfInterest->entries[i].id;
    queryResults->entries[queryResultsEntries].intersectionCount = userdata.intersectionCount;
    queryResults->entries[queryResultsEntries].intersectingCells = userdata.intersectingCells;

    queryResultsEntries++;
  }

  // forcefully reallocate/shrink array if not all AOI geometries intersected with raster/STRTree
  if (queryResults->size - queryResultsEntries > 0) {
    struct i *tmp = reallocarray(queryResults->entries, queryResultsEntries, sizeof(struct i));
    
    if (tmp == NULL) {
      fprintf(stderr, "Failed to reallocate array of STRTree query results. Continuing with larger array, shadowing additional entries\n");
      queryResults->size = queryResultsEntries;
    } else {
      queryResults->entries = tmp;
      queryResults->size = queryResultsEntries;
    }
  }

  return queryResults;
}

[[nodiscard]] GEOSGeometry *boundingBoxOfOGRToGEOS(const OGRGeometryH geom)
{
  if (geom == NULL) {
    return NULL;
  }

  OGREnvelope envelope;

  OGR_G_GetEnvelope(geom, &envelope);

  GEOSGeometry *returnGeometry = GEOSGeom_createRectangle(envelope.MinX, envelope.MinY,
                                 envelope.MaxX, envelope.MaxY);

  return returnGeometry;
}

[[nodiscard]] GEOSGeometry *OGRToGEOS(const OGRGeometryH geom)
{
  if (geom == NULL) {
    return NULL;
  }

  /// FIXME: hoist reader out of loop/function!
  GEOSWKBReader *reader = GEOSWKBReader_create();
  if (reader == NULL) {
    fprintf(stderr, "Failed to create GEOS WKB reader\n");
    return NULL;
  }

  unsigned char *OGRWkb = calloc(OGR_G_WkbSize(geom), sizeof(unsigned char));
  if (OGRWkb == NULL) {
    perror("calloc");
    GEOSWKBReader_destroy(reader);
    return NULL;
  }

  OGR_G_ExportToIsoWkb(geom, wkbNDR, OGRWkb); // returns OGRERR_NONE in all cases

  GEOSGeometry *returnGeometry = GEOSWKBReader_read(reader, OGRWkb, OGR_G_WkbSize(geom));

  free(OGRWkb);
  GEOSWKBReader_destroy(reader);

  return returnGeometry;
}
