#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdlib.h>

enum how
{
  CEILING,
  FLOOR
};

int fNudge(const float *x, enum how direction);
int dNudge(const double *x, enum how direction);

double calculateWeightedAverage(const double *values, const double *weights, size_t count);

#endif // MATH_UTILS_H