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
#include <stdlib.h>
#include <unistd.h>

/* fucntion from GEOS documentation */
// TODO: gibt es einen Grund, warum die Funktion `static` ist? Sonst kann ich die ja auch einfach in eine eigene translation Uni packen.
// VIEL WICHTIGER: aktuell ist das einfach nur `printf`!
static void geos_msg_handler(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vprintf (fmt, ap);
  va_end(ap);
}

int main(int argc, char *argv[])
{
  int exitCode = EXIT_SUCCESS;
  /* SETUP EXTERNAL LIBRARIES */
  initGEOS(geos_msg_handler, geos_msg_handler);
  GDALAllRegister();
  curl_global_init(CURL_GLOBAL_ALL);

  /* START OF PROGRAM */
  CURL *handle = NULL;
  const OGREnvelope *aoi = NULL;
  stringList *downloadedFiles = NULL;

  option_t *opts = parseOptions(argc, argv);

  if (opts == NULL || opts->printHelp) {
    printHelp();
    freeOption(opts);
    exitCode = opts != NULL;
    goto teardown;
  }

#ifdef DEBUG
  printOptions(opts);
#endif

  handle = curl_easy_init();

  if (handle == NULL) {
    fprintf(stderr, "Failed to setup cURL\n");
    goto teardown;
  }

  if (!opts->global && opts->download) {
    aoi = boxFromPath(opts->areaOfInterest, opts->aoiName);
    if (aoi == NULL) {
      exitCode = EXIT_FAILURE;
      goto teardown;
    }
  }

  if (opts->download) {
    if (download(handle, opts, aoi)) {
      fprintf(stderr, "Error downloading files\n");
      exitCode = EXIT_FAILURE;
      goto teardown;
    }
  } else if (opts->process) {
    if (processDaily(downloadedFiles, opts) == 1) {
      fprintf(stderr, "Failed to process all datasets\n");
      exitCode = EXIT_FAILURE;
      goto teardown;
    }
  }

teardown:
  /* END OF PROGRAM, FREE STACK OBJECTS */
  CPLFree((void *) aoi);
  freeOption(opts);
  freeStringList(downloadedFiles);

  /* TEARDOWN EXTERNAL LIBRARIES */
  GDALDestroy();
  finishGEOS();
  curl_easy_cleanup(handle);
  curl_global_cleanup();

  return exitCode;
}
