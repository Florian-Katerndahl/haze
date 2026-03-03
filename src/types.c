#include "types.h"
#include <gdal/gdal.h>
#include <geos_c.h>
#include <stdlib.h>

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
    freeCellGeometry(list->entry);
    node = list;
    list = list->next;
    free(node);
  }
}

void freeIntersections(intersection_t *list)
{
  intersection_t *currentNode;
  cellGeometryList *cellNode;

  while (list != NULL) {
    // NOTE: The elements within the nodes are owned by
    // the list created when constructing the tree!
    // Thus, I need to free the nodes but not their content!
    cellNode = list->intersectingCells;
    while (cellNode != NULL) {
      cellGeometryList *nextCellNode = cellNode->next;
      free(cellNode);
      cellNode = nextCellNode;
    }
    currentNode = list;
    list = list->next;
    free(currentNode);
  }
}

void freeWeightedMeans(mean_t *list)
{
  mean_t *next;
  while (list != NULL) {
    next = list->next;
    free(list);
    list = next;
  }
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
    free(list);
    list = tmp;
  }
}