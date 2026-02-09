#include "../src/haze.h"
#include "../src/types.h"
#include "../src/options.h"
#include <gdal/gdal.h>
#include <geos_c.h>

int main(void)
{
  initGEOS(NULL, NULL);
  GDALAllRegister();
  fprintf(stderr, "HI\n");
  option_t test_opts = {0};
  // getAuthenticationFromFile(&test_opts.authenticationToken, NULL);
  test_opts.years[0] = 2009;
  test_opts.years[1] = 2010;
  test_opts.years[2] = -1;
  test_opts.months[0] = 1;
  test_opts.months[1] = 2;
  test_opts.months[2] = -1;
  test_opts.days[0] = 24;
  test_opts.days[1] = -1;
  test_opts.hours[0] = 0;
  test_opts.hours[1] = 1;
  test_opts.hours[2] = 2;
  test_opts.hours[3] = 3;
  test_opts.hours[4] = -1;
  test_opts.outputDirectory = "/home/florian/git-repos/haze/feature-tests/gribs";
  test_opts.areaOfInterest = "/home/florian/git-repos/haze/data/grid.kml";


  printf("%ld\n", countRequestedHours(&test_opts));

  // processDaily(&test_opts);

  free(test_opts.authenticationToken);
  finishGEOS();
  return 0;
}