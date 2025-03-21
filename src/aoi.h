#ifndef AOI_H
#define AOI_H

#include <gdal/ogr_core.h>

[[nodiscard]] OGREnvelope *boxFromPath(const char *filePath, const char *layerName);

void printBoundingBox(const OGREnvelope *box);

#endif // AOI_H