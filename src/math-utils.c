#include "math-utils.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

int fNudge(const float *x, enum how direction)
{
  assert(INT_MIN < *x || *x < INT_MAX);
  if (direction == CEILING)
    return (int) ceilf(*x);
  else {
    return (int) floorf(*x);
  }
}

int dNudge(const double *x, enum how direction)
{
  assert(INT_MIN < *x || *x < INT_MAX);
  if (direction == CEILING)
    return (int) ceil(*x);
  else {
    return (int) floor(*x);
  }
}

double calculateWeightedAverage(const double *values, const double *weights, size_t count)
{
  assert(values && weights);
  double numerator = 0.0;
  double denominator = 0.0;

  for (size_t i = 0; i < count; i++) {
    numerator += values[i] * weights[i];
    denominator += weights[i];
  }

  return numerator / denominator;
}
