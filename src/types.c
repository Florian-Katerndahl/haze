#include "types.h"
#include <gdal/gdal.h>
#include <geos_c.h>
#include <stdlib.h>
#include <time.h>

void freeVectorGeometry(struct vectorGeometry *node)
{
  OGR_G_DestroyGeometry(node->OGRGeometry);
  GEOSGeom_destroy(node->geometry);
  GEOSGeom_destroy(node->mbr);
  free(node);
  node = NULL;
}

void freeVectorGeometryList(vectorGeometryList *list)
{
  vectorGeometryList *temp;
  while (list != NULL) {
    freeVectorGeometry(list->entry);
    temp = list;
    list = list->next;
    free(temp);
  }
}

void freeCellGeometry(struct cellGeometry *node)
{
  GEOSGeom_destroy(node->geometry);
  free(node);
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

void freeIntersections(intersection_t *list)
{
  intersection_t *nextIntersection;
  cellGeometryList *nextGeom;

  while (list != NULL) {
    nextIntersection = list->next;

    // NOTE: The elements within the nodes are owned by
    // the list created when constructing the tree!
    while (list->intersectingCells != NULL) {
      nextGeom = list->intersectingCells->next;
      free(list->intersectingCells);
      list->intersectingCells = nextGeom;
    }

    free(list);

    list = nextIntersection;
  }
}

void freeOption(option_t *options)
{
  if (options->withAllocation)
    free(options->authenticationToken);
  free(options->outputDirectory);
  free(options);
}

void freeStringList(stringList *list)
{
  stringList *tmp;
  while (list != NULL) {
    tmp = list->next;
    free(list->string);
    free(list);
    list = tmp;
  }
}