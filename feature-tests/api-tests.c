#include "../src/api.h"
#include "../src/types.h"
#include "../src/options.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <gdal/ogr_core.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(void) {
  curl_global_init(CURL_GLOBAL_ALL);

  CURL *handle = curl_easy_init();

  option_t test_opts = {0};
  getAuthenticationFromFile(&test_opts.authenticationToken, NULL);
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
  test_opts.outputDirectory = "/home/florian/git-repos/haze/feature-tests";

  OGREnvelope test_envelope = {-180, 180, -90, 90};


  struct curl_slist *headerAddon = customHeader(NULL, &test_opts);

  initializeHandle(&handle, headerAddon);

  downloadDaily(handle, &test_opts, &test_envelope);
  
  freeCustomHeader(headerAddon);
  free(test_opts.authenticationToken);
  curl_easy_cleanup(handle);
  curl_global_cleanup();

  return EXIT_SUCCESS;
}