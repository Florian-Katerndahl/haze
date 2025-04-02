#include "src/aoi.h"
#include "src/api.h"
#include "src/options.h"
#include "src/types.h"
#include "src/haze.h"
#include <stdio.h>
#include <stddef.h>
#include <geos_c.h>
#include <gdal/gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/ogr_core.h>
#include <curl/curl.h>

/* fucntion from GEOS documentation */
static void geos_msg_handler(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vprintf (fmt, ap);
  va_end(ap);
}

int main(int argc, char *argv[])
{
  int exit = EXIT_SUCCESS;
  /* SETUP EXTERNAL LIBRARIES */
  initGEOS(geos_msg_handler, geos_msg_handler);
  GDALAllRegister();
  curl_global_init(CURL_GLOBAL_ALL);

  /* START OF PROGRAM */
  option_t *opts = parseOptions(argc, argv);

#ifdef DEBUG
  printOptions(opts);
#endif

  const OGREnvelope *aoi = boxFromPath(opts->areaOfInterest, NULL);
  if (aoi == NULL) {
    exit = EXIT_FAILURE;
    goto teardown;
  }

  if (downloadDaily(opts, aoi)) {
    fprintf(stderr, "Failed to download all datasets\n");
    exit = EXIT_FAILURE;
    goto teardown;
  }

  if (processDaily(opts) == 1) {
    fprintf(stderr, "Failed to process all datasets\n");
    exit = EXIT_FAILURE;
    goto teardown;
  }

teardown:
  /* END OF PROGRAM, FREE STACK OBJECTS */
  CPLFree((void *) aoi);
  freeOption(opts);

  /* TEARDOWN EXTERNAL LIBRARIES */
  finishGEOS();
  curl_global_cleanup();

  return exit;
}
