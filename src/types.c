#include "types.h"
#include <gdal/gdal.h>
#include <geos_c.h>
#include <stdlib.h>

void freeVectorGeometry(struct vectorGeometry *node)
{
  OGR_G_DestroyGeometry(node->OGRGeometry);
  GEOSGeom_destroy(node->geometry);
  GEOSGeom_destroy(node->mbr);
}

void freeVectorGeometryList(vectorGeometryVector *vector)
{
  for (size_t i = 0; i < vector->size; i++) {
    freeVectorGeometry(&vector->entries[i]);
  }
  free(vector->entries);
  free(vector);
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
    freeCellGeometry(list->entry);
    node = list;
    list = list->next;
    free(node);
  }
}

void freeIntersections(intersectionVector *vector)
{
  cellGeometryList *cellNode;

  for (size_t i = 0; i < vector->size; i++) {
    // NOTE: The elements within the nodes are owned by
    // the list created when constructing the tree!
    // Thus, I need to free the nodes but not their content!
    cellNode = vector->entries[i].intersectingCells;
    while (cellNode != NULL) {
      cellGeometryList *nextCellNode = cellNode->next;
      free(cellNode);
      cellNode = nextCellNode;
    }
  }

  free(vector->entries);
  free(vector);
}

void freeWeightedMeans(meanVector *vector)
{
  free(vector->entries);
  free(vector);
}

void freeOption(option_t *options)
{
  if (!options)
    return;

  free(options->authenticationToken);
  free(options);
}

void freeStringList(stringList *list)
{
  stringList *tmp;
  while (list != NULL) {
    tmp = list->next;
    free(list->string);
    free(list->status);
    free(list);
    list = tmp;
  }
}