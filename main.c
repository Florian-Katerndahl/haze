#include "src/options.h"
#include "src/types.h"
#include <stdio.h>
#include <geos_c.h>
#include <gdal/gdal.h>
#include <curl/curl.h>

static void
geos_msg_handler(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf (fmt, ap);
    va_end(ap);
}

int main(int argc, char *argv[])
{
  /* SETUP EXTERNAL LIBRARIES */
  initGEOS(geos_msg_handler, geos_msg_handler);
  GDALAllRegister();
  curl_global_init(CURL_GLOBAL_ALL);

  /* START OF PROGRAM */
  option_t *opts = parseOptions(argc, argv);

#ifdef DEBUG
  printOptions(opts);
#endif

  // if (downloadProduct(opts) == 1) {
  //   fprintf(stderr, "Failed to download all datasets\n");
  //   return 1;
  // }

  // if (processProduct(opts) == 1) {
  //   fprintf(stderr, "Failed to process all datasets\n");
  //   return 1;
  // }

  freeOption(opts);

  /* TEARDOWN EXTERNAL LIBRARIES */
  finishGEOS();
  curl_global_cleanup();

  return 0;
}
