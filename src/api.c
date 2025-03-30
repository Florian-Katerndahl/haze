#include "api.h"
#include "types.h"
#include <assert.h>
#include <curl/easy.h>
#include <gdal/ogr_core.h>
#include <jansson.h>
#include <curl/curl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct curl_slist *customHeader(struct curl_slist *list, const option_t *options)
{
  assert(options);
  char header[256];
  int charsWritten = snprintf(header, sizeof(header), "PRIVATE-TOKEN:%s",
                              options->authenticationToken);
  if (charsWritten >= 256 || charsWritten < 0) {
    fprintf(stderr, "Failed to assemble custom header\n");
    exit(EXIT_FAILURE); // if login won't be possible, no need to do anything else...
  }

  struct curl_slist *ret = curl_slist_append(list, header);
  if (ret == NULL) {
    fprintf(stderr, "Failed to generate custom HTTP header\n");
    exit(EXIT_FAILURE); // if login won't be possible, no need to do anything else...
  }

  return ret;
}

void freeCustomHeader(struct curl_slist *list)
{
  curl_slist_free_all(list);
}

CURL *initializeHandle(CURL **handle, const struct curl_slist *headerList)
{
  // HOLY MOLY, for whatever reason, the LegacyApiClient is needed to access data even though the cdsapirc file has to follow the "old" format - the fuck!
  if (headerList) {
    curl_easy_setopt(*handle, CURLOPT_HTTPHEADER, headerList);
  }
  curl_easy_setopt(*handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(*handle, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(*handle, CURLOPT_SSL_VERIFYHOST, 1L);
  curl_easy_setopt(*handle, CURLOPT_TIMEOUT, 600); // default timeout value of cdsapi is 60 seconds
  curl_easy_setopt(*handle, CURLOPT_USERAGENT, "haze");

  return *handle;
}

char *constructURL(const char *basePath, const char *endPoint, const char *requestId, size_t addon)
{
  int basePathLength = (int) strlen(basePath);
  int endPointLength = (int) strlen(endPoint);
  int requestIdLength = (int) strlen(requestId);
  int totalLength = basePathLength + endPointLength + requestIdLength + addon + 1;

  bool baseWithSlash = basePath[basePathLength - 1] == '/';
  totalLength += baseWithSlash ? 1 : 2;

  char *url = calloc(totalLength, sizeof(char));
  if (url == NULL) {
    perror("calloc");
    return NULL;
  }

  int charsWritten;
  if (baseWithSlash) {
    charsWritten = snprintf(url, totalLength, "%s%s/%s", basePath, endPoint, requestId);
  } else {
    charsWritten = snprintf(url, totalLength, "%s/%s/%s", basePath, endPoint, requestId);
  }
  if (charsWritten >= totalLength || charsWritten < 0) {
    fprintf(stderr, "Failed to assemble URL\n");
    free(url);
    return NULL;
  }

  return url;
}

size_t writeString(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  curlString *response = (curlString *) userdata;
  size_t chunkSize = size * nmemb;

  char *messagePointer = realloc(response->string, response->length + chunkSize + 1);
  if (messagePointer == NULL) {
    perror("realloc");
    return 0;
  }

  response->string = messagePointer;
  memcpy(&response->string[response->length], ptr, chunkSize);
  response->length += chunkSize;
  response->string[response->length] = '\0';

  return chunkSize;
}

size_t discardWrite(__attribute__((unused)) char *ptr, size_t size, size_t nmemb,
                    __attribute__((unused)) void *userdata)
{
  return size * nmemb;
}

json_t *getKeyRecursively(json_t *root, const char *key)
{
  json_t *ret;
  if ((ret = json_object_get(root, key))) {
    return ret;
  } else {
    const char *entryKey;
    json_t *value;
    json_object_foreach(root, entryKey, value) {
      if (json_is_object(value)) {
        if ((ret = getKeyRecursively(value, key))) {
          return ret;
        }
      }
    }
  }
  return NULL;
}

int downloadDaily(const option_t *options, const OGREnvelope *aoi)
{
  CURL *handle = curl_easy_init();
  if (handle == NULL) {
    fprintf(stderr, "Failed to create CURL handle\n");
    return -1;
  }

  struct curl_slist *headerAddon = customHeader(NULL, options);

  initializeHandle(&handle, headerAddon);

  for (int *year = (int *) options->years; *year != INITVAL; year++) {
    for (int *month = (int *) options->months; *month != INITVAL; month++) {
      for (int *day = (int *) options->days; *day != INITVAL; day++) {
        int fileNameLength = 16;
        char *dateString = calloc(fileNameLength, sizeof(char));
        if (dateString == NULL) {
          perror("calloc");
          curl_easy_cleanup(handle);
          return -1;
        }

        int charsWritten = snprintf(dateString, fileNameLength, "%.4d-%.2d-%.2d.grib", *year, *month, *day);
        if (charsWritten >= fileNameLength || charsWritten < 0) {
          fprintf(stderr, "Failed to convert date to string format\n");
          free(dateString);
          curl_easy_cleanup(handle);
          return -1;
        }

        int numCharacters = (int) strlen(options->outputDirectory) + fileNameLength;
        bool endsWithSlash = options->outputDirectory[strlen(options->outputDirectory) - 1] == '/';

        if (!endsWithSlash) {
          numCharacters++;
        }

        char *outputPath = calloc(numCharacters, sizeof(char));
        if (outputPath == NULL) {
          perror("calloc");
          free(dateString);
          curl_easy_cleanup(handle);
          return -1;
        }

        if (endsWithSlash)
          charsWritten = snprintf(outputPath, numCharacters, "%s%s", options->outputDirectory, dateString);
        else
          charsWritten = snprintf(outputPath, numCharacters, "%s/%s", options->outputDirectory, dateString);

        if (charsWritten >= numCharacters || charsWritten < 0) {
          fprintf(stderr, "Failed to construct local file path\n");
          free(dateString);
          free(outputPath);
          curl_easy_cleanup(handle);
          return -1;
        }

        int requestYears[2] = {*year, INITVAL};
        int requestMonths[2] = {*month, INITVAL};
        int requestDays[2] = {*day, INITVAL};

        char *requestId = cdsRequestProduct(handle, requestYears, requestMonths, requestDays,
                                            options->hours, aoi, options);
        if (requestId == NULL) {
          fprintf(stderr, "Failed to request product or extract job id\n");
          // todo cleanup
          return -1;
        }
#ifdef DEBUG
        printf("Posted product request with Id: %s\n", requestId);
#endif

        if (cdsWaitForProduct(handle, requestId)) {
          fprintf(stderr, "Error while waiting for product\n");
          free(requestId);
          free(dateString);
          free(outputPath);
          curl_easy_cleanup(handle)          ;
          return -1;
        }
#ifdef DEBUG
        printf("Waited for product request with Id: %s\n", requestId);
#endif

        cdsDownloadProduct(handle, requestId, outputPath);
#ifdef DEBUG
        printf("Downloaded file for product request %s\n", requestId);
#endif

        cdsDeleteProductRequest(handle, requestId);
#ifdef DEBUG
        printf("Deleted product request with Id: %s\n", requestId);
#endif

        free(requestId);
        free(dateString);
        free(outputPath);
      }
    }
  }

  freeCustomHeader(headerAddon);
  curl_easy_cleanup(handle);
  return 0;
}

json_t *jsonArrayFromIntegers(const int *arr, int stopVal, const char *formatString)
{
  json_t *jsonArray = json_array();
  if (jsonArray == NULL) {
    fprintf(stderr, "Failed to create JSON array\n");
    return NULL;
  }

  json_t *elementString;
  while (*arr != stopVal) {
    if ((elementString = json_sprintf(formatString, *arr)) == NULL) {
      fprintf(stderr, "Failed to create JSON string representation of integer\n");
      json_decref(jsonArray);
      return NULL;
    }

    if (json_array_append_new(jsonArray, elementString)) {
      json_decref(elementString);
      json_decref(jsonArray);
      return NULL;
    }

    arr++;
  }

  return jsonArray;
}

char *constructStringRequest(const int *years, const int *months, const int *days, const int *hours,
                             const OGREnvelope *aoi)
{
  json_t *yearsArray = NULL;
  json_t *monthsArray = NULL;
  json_t *daysArray = NULL;
  json_t *hoursArray = NULL;
  json_t *aoiArray = NULL;
  json_t *jsonRequest = NULL;

  if ((yearsArray = jsonArrayFromIntegers(years, INITVAL, "%.4d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested years\n");
    goto cleanup;
  }

  if ((monthsArray = jsonArrayFromIntegers(months, INITVAL, "%.2d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested months\n");
    goto cleanup;
  }

  if ((daysArray = jsonArrayFromIntegers(days, INITVAL, "%.2d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested days\n");
    goto cleanup;
  }

  if ((hoursArray = jsonArrayFromIntegers(hours, INITVAL, "%.2d:00")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested hours\n");
    goto cleanup;
  }

  if (aoi == NULL) {
    aoiArray = NULL;
  } else {
    if ((aoiArray = json_array()) == NULL) {
      fprintf(stderr, "Failed to create JSON array for requested aoi\n");
      goto cleanup;
    }

    if (json_array_append_new(aoiArray, json_real(aoi->MaxY)) ||
        json_array_append_new(aoiArray, json_real(aoi->MinX)) ||
        json_array_append_new(aoiArray, json_real(aoi->MinY)) ||
        json_array_append_new(aoiArray, json_real(aoi->MaxX)))
      goto cleanup;
  }

  // steals references to JSON objects
  jsonRequest = json_pack("{s: {s:[s], s:o, s:o, s:o, s:o, s:o*, s:s, s:s, s:[s]}}",
                          "inputs", "product_type", "reanalysis", "year", yearsArray, "month", monthsArray, "day", daysArray,
                          "time",
                          hoursArray, "area", aoiArray, "data_format", "grib", "download_format", "unarchived", "variable",
                          "total_column_water_vapour");

  if (jsonRequest == NULL) {
    fprintf(stderr, "Failed to craete complete JSON request\n");
    goto cleanup;
  }

  char *stringRequest = json_dumps(jsonRequest, 0);

  json_decref(jsonRequest);

  // callee checks for success
  return stringRequest;

// jansson checks for NULL before accessing object members
cleanup:
  json_decref(hoursArray);
  json_decref(daysArray);
  json_decref(monthsArray);
  json_decref(yearsArray);
  json_decref(jsonRequest);
  return NULL;

}

// gets first matching key, depth first
char *slurpAndGetString(const char *input, const char *key)
{
  json_error_t error;
  json_t *root = json_loads(input, 0, &error);
  if (root == NULL) {
    fprintf(stderr, "Failed to parse JSON response on line %d: %s\n", error.line, error.text);
    return NULL;
  }

  if (!json_is_object(root)) {
    fprintf(stderr, "Supplied JSON must be an object.\n");
    json_decref(root);
    return NULL;
  }

  json_t *keyEntry = getKeyRecursively(root, key);
  if (keyEntry == NULL) {
    fprintf(stderr, "Could not find key '%s'.\n", key);
    json_decref(root);
    return NULL;
  }

  const char *value = json_string_value(keyEntry);
  if (value == NULL) {
    fprintf(stderr, "Failed to get string value from key '%s'\n", key);
    json_decref(root);
    return NULL;
  }

  char *ret = strdup(value);
  if (ret == NULL) {
    fprintf(stderr, "Failed to dupicate string value\n");
    json_decref(root);
    return NULL;
  }

  json_decref(root);

  return ret;
}

char *cdsRequestProduct(CURL *handle, const int *years, const int *months, const int *days,
                        const int *hours, const OGREnvelope *aoi, const option_t *options)
{
  CURL *requestHandle = curl_easy_duphandle(handle);
  if (requestHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before requesting product\n");
    return NULL;
  }

  char *stringRequest = constructStringRequest(years, months, days, hours, aoi);
  if (stringRequest == NULL) {
    fprintf(stderr, "Failed to export JSON to string\n");
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  char *url = constructURL(BASEURL, "retrieve/v1/processes/reanalysis-era5-single-levels",
                           "execution", 0);
  if (url == NULL) {
    fprintf(stderr, "Failed to assemble request URL\n");
    free(stringRequest);
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  curlString requestResponse = {0};

  curl_easy_setopt(requestHandle, CURLOPT_URL, url);
  curl_easy_setopt(requestHandle, CURLOPT_POST, 1L);
  curl_easy_setopt(requestHandle, CURLOPT_POSTFIELDS, stringRequest);
  curl_easy_setopt(requestHandle, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(requestHandle, CURLOPT_WRITEDATA, (void *) &requestResponse);

  struct curl_slist *requestHeader = customHeader(NULL, options);
  if (requestHeader == NULL) {
    fprintf(stderr, "Failed to create custom HTTP header for product request\n");
    free(url);
    free(stringRequest);
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  if ((requestHeader = curl_slist_append(requestHeader, "Content-Type: application/json")) == NULL) {
    free(url);
    curl_slist_free_all(requestHeader);
    free(stringRequest);
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  curl_easy_setopt(requestHandle, CURLOPT_HTTPHEADER, requestHeader);

  CURLcode requestResponseCode = curl_easy_perform(requestHandle);
  if (requestResponseCode != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(requestHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to get product status: %s (%ld)\n", curl_easy_strerror(requestResponseCode),
            httpResponse);
    fprintf(stderr, "Server message:\n%s\n", requestResponse.string);
    free(requestResponse.string);
    free(url);
    curl_slist_free_all(requestHeader);
    free(stringRequest);
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  char *jobId = slurpAndGetString(requestResponse.string, "jobID");

  free(requestResponse.string);
  free(url);
  curl_slist_free_all(requestHeader);
  free(stringRequest);
  curl_easy_cleanup(requestHandle);

  return jobId;
}

productStatus cdsGetProductStatus(CURL *handle, const char *requestId)
{
  CURL *statusHandle = curl_easy_duphandle(handle);
  if (statusHandle ==  NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before checking product status\n");
    return ERROR;
  }

  char *url = constructURL(BASEURL, "retrieve/v1/jobs", requestId, 0);
  if (url == NULL) {
    fprintf(stderr, "Failed to construct url for product status check\n");
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  curlString response = {0};

  curl_easy_setopt(statusHandle, CURLOPT_URL, url);
  curl_easy_setopt(statusHandle, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(statusHandle, CURLOPT_WRITEDATA, (void *) &response);

  CURLcode statusResponse = curl_easy_perform(statusHandle);
  if (statusResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(statusHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to get product status: %s (%ld)\n", curl_easy_strerror(statusResponse),
            httpResponse);
    free(response.string);
    free(url);
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  char *statusWord = slurpAndGetString(response.string, "status");
  if (statusWord == NULL) {
    free(response.string);
    free(url);
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  productStatus status;
  if (strcmp(statusWord, "accepted") == 0)
    status = ACCEPTED;
  else if (strcmp(statusWord, "successful") == 0)
    status = SUCCESSFUL;
  else if (strcmp(statusWord, "running") == 0)
    status = RUNNING;
  else if (strcmp(statusWord, "failed") == 0)
    status = FAILED;
  else
    status = ERROR;

  free(statusWord);
  free(response.string);
  free(url);
  curl_easy_cleanup(statusHandle);

  return status;
}

int cdsWaitForProduct(CURL *handle, const char *requestId)
{
  unsigned int sleepSeconds = 10;
  unsigned int attempt = 1;
  do {
    switch (cdsGetProductStatus(handle, requestId)) {
      case SUCCESSFUL:
        return 0;
      case ACCEPTED: // fallthrough
      case RUNNING:
        sleep(attempt * sleepSeconds);
        attempt++;
        break;
      case FAILED:
        fprintf(stderr, "Order %s failed\n", requestId);
        return 1;
      case ERROR:
        fprintf(stderr, "General error occurred while waiting for %s\n", requestId);
        return 1;
    }
  } while (1);

  return 0;
}

int cdsDownloadProduct(CURL *handle, const char *requestId, const char *outputPath)
{
  CURL *downloadHandle = curl_easy_duphandle(handle);
  if (downloadHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before product download\n");
    return 1;
  }

  char *url = constructURL(BASEURL, "retrieve/v1/jobs", requestId, 8);
  if (url == NULL) {
    fprintf(stderr, "Failed to construct jobURL\n");
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  strcat(url, "/results");

  curlString response = {0};

  curl_easy_setopt(downloadHandle, CURLOPT_URL, url);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEDATA, (void *) &response);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEFUNCTION, writeString);

  CURLcode downloadResponse = curl_easy_perform(downloadHandle);
  if (downloadResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(downloadHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to get product status: %s (%ld)\n", curl_easy_strerror(downloadResponse),
            httpResponse);
    fprintf(stderr, "Server message:\n%s\n", response.string);
    free(response.string);
    free(url);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  char *downloadURL = slurpAndGetString(response.string, "href");
  if (downloadURL == NULL) {
    fprintf(stderr, "Failed to get download URL\n");
    free(url);
    free(response.string);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  FILE *outputFile = fopen(outputPath, "wb");
  if (outputFile == NULL) {
    fprintf(stderr, "Could not open file %s for writing\n", outputPath);
    free(url);
    free(response.string);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  curl_easy_setopt(downloadHandle, CURLOPT_URL, downloadURL);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEDATA, (void *) outputFile);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEFUNCTION, NULL);

  downloadResponse = curl_easy_perform(downloadHandle);
  if (downloadResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(downloadHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to get product status: %s (%ld)\n", curl_easy_strerror(downloadResponse),
            httpResponse);
    fprintf(stderr, "Server message:\n%s\n", response.string);
    fclose(outputFile);
    unlink(outputPath);
    free(response.string);
    free(url);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  fclose(outputFile);
  free(downloadURL);
  free(response.string);
  free(url);
  curl_easy_cleanup(downloadHandle);

  return 0;
}

int cdsDeleteProductRequest(CURL *handle, const char *requestId)
{
  CURL *deleteHandle = curl_easy_duphandle(handle);
  if (deleteHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before deleting product request\n");
    return 1;
  }

  char *url = constructURL(BASEURL, "retrieve/v1/jobs", requestId, 0);
  if (url == NULL) {
    fprintf(stderr, "Failed to construct url for deletion\n");
    curl_easy_cleanup(deleteHandle);
    return 1;
  }

  curl_easy_setopt(deleteHandle, CURLOPT_URL, url);
  curl_easy_setopt(deleteHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(deleteHandle, CURLOPT_WRITEFUNCTION, discardWrite);

  CURLcode deleteResponse = curl_easy_perform(deleteHandle);
  if (deleteResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(deleteHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to delete product request: %s (%ld)\n", curl_easy_strerror(deleteResponse),
            httpResponse);
    curl_easy_cleanup(deleteHandle);
    return 1;
  }

  curl_easy_cleanup(deleteHandle);
  free(url);

  return 0;
}