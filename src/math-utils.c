#include "math-utils.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>

double kgsqmTocow(double x)
{
  return x / 10.0;
}

double calculateWeightedAverage(const double *values, const double *weights, size_t count)
{
  if (values == NULL || weights == NULL) {
    return NAN;
  }

  double numerator = 0.0;
  double denominator = 0.0;

  for (size_t i = 0; i < count; i++) {
    numerator += values[i] * weights[i];
    denominator += weights[i];
  }

  return numerator / denominator;
}
