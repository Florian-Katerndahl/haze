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
  int exit = EXIT_SUCCESS;
  /* SETUP EXTERNAL LIBRARIES */
  initGEOS(geos_msg_handler, geos_msg_handler);
  GDALAllRegister();
  curl_global_init(CURL_GLOBAL_ALL);

  /* START OF PROGRAM */
  const OGREnvelope *aoi = NULL;
  option_t *opts = parseOptions(argc, argv);

  if (opts == NULL || opts->printHelp) {
    printHelp();

    freeOption(opts);
    
    /* TEARDOWN EXTERNAL LIBRARIES */
    finishGEOS();
    curl_global_cleanup();
    return opts != NULL;
  }

#ifdef DEBUG
  printOptions(opts);
#endif

  CURL *handle = curl_easy_init();

  if (handle == NULL) {
    fprintf(stderr, "Failed to setup cURL\n");
    goto teardown;
  }

  if (!opts->global) {
    aoi = boxFromPath(opts->areaOfInterest, opts->aoiName);
    if (aoi == NULL) {
      exit = EXIT_FAILURE;
      goto teardown;
    }
  }

  stringList *downloadedFiles = NULL;
  if ((downloadedFiles = download(handle, opts, aoi)) == NULL) {
    fprintf(stderr, "Error while downloading files\n");
    exit = EXIT_FAILURE;
    goto teardown;
  }

  if (processDaily(downloadedFiles, opts) == 1) {
    fprintf(stderr, "Failed to process all datasets\n");
    exit = EXIT_FAILURE;
    goto teardown;
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

  return exit;
}
