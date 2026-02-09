#include "math-utils.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

double kgsqmTocow(double x)
{
  return x / 10.0;
}

double calculateWeightedAverage(const double *values, const double *weights, size_t count)
{
  // This assertion is not nice, better to return +/- INF in this case or even NAN; I like the latter approach the best,
  // potentially together with some warning message from the caller?
  assert(values && weights);
  double numerator = 0.0;
  double denominator = 0.0;

  for (size_t i = 0; i < count; i++) {
    numerator += values[i] * weights[i];
    denominator += weights[i];
  }

  return numerator / denominator;
}
