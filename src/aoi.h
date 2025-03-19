#ifndef AOI_H
#define AOI_H

struct boundingBox
{
  double left;
  double top;
  double right;
  double bottom;
};

struct boundingBox *allocBoundingBox(void);

void freeBoundingBox(struct boundingBox *box);

struct boundingBox *boxFromPath(const char *filePath, const char *layerName);

void printBoundingBox(const struct boundingBox *box);

#endif // AOI_H