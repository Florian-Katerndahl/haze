#include "api.h"
#include "types.h"
#include "paths.h"
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
#include <limits.h>
#include <math.h>

struct curl_slist *customHeader(struct curl_slist *list, const option_t *options)
{
  if (options == NULL)
    return NULL;

  char *header = constructURL("PRIVATE-TOKEN:%s", options->authenticationToken);

  if (header == NULL)
    return NULL;

  struct curl_slist *ret = curl_slist_append(list, header);

  if (ret == NULL) {
    curl_slist_free_all(list);
  }

  free(header);

  // returns NULL on error as well
  return ret;
}

int initializeHandle(CURL **handle, const struct curl_slist *headerList)
{
  if (headerList) {
    if (curl_easy_setopt(*handle, CURLOPT_HTTPHEADER, headerList) != CURLE_OK)
      return 1;
  }
  if (curl_easy_setopt(*handle, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
    return 1;
  if (curl_easy_setopt(*handle, CURLOPT_FAILONERROR, 1L) != CURLE_OK)
    return 1;
  if (curl_easy_setopt(*handle, CURLOPT_SSL_VERIFYHOST, 1L) != CURLE_OK)
    return 1;
  // default timeout value of cdsapi is 60 seconds
  if (curl_easy_setopt(*handle, CURLOPT_TIMEOUT, 600L) != CURLE_OK)
    return 1;
  if (curl_easy_setopt(*handle, CURLOPT_USERAGENT, "haze") != CURLE_OK)
    return 1;

  return 0;
}

CURL *newHandleWithOptions(struct curl_slist **list, const option_t *options)
{
  CURL *handle = curl_easy_init();
  if (handle == NULL) {
    fprintf(stderr, "Failed to create CURL handle\n");
    return NULL;
  }

  struct curl_slist *headerAddon = customHeader(*list, options);
  if (headerAddon == NULL) {
    fprintf(stderr, "Failed to create HTTP header for product requests\n");
    curl_easy_cleanup(handle);
    return NULL;
  }

  if (initializeHandle(&handle, headerAddon) == 1) {
    fprintf(stderr, "Failed to initialize cURL handle\n");
    return NULL;
  }

  return handle;
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
    // only valid for lifetime of root!
    return ret;
  } else {
    const char *entryKey;
    json_t *value;
    json_object_foreach(root, entryKey, value) {
      if (json_is_object(value)) {
        if ((ret = getKeyRecursively(value, key))) {
          // only valid for lifetime of root!
          return ret;
        }
      }
    }
  }
  return NULL;
}

json_t *jsonArrayFromIntegers(const int *arr, size_t elements, const char *formatString)
{
  json_t *jsonArray = json_array();
  if (jsonArray == NULL) {
    fprintf(stderr, "Failed to create JSON array\n");
    return NULL;
  }

  json_t *elementString;

  for (size_t i = 0; i < elements; i++) {
    if ((elementString = json_sprintf(formatString, arr[i])) == NULL) {
      fprintf(stderr, "Failed to create JSON string representation of integer\n");
      json_decref(jsonArray);
      return NULL;
    }

    if (json_array_append_new(jsonArray, elementString)) {
      json_decref(elementString);
      json_decref(jsonArray);
      return NULL;
    }
  }

  return jsonArray;
}

char *constructStringRequest(const int *years, const int *months, const int *days, const int *hours,
                             const size_t yearsElements, const size_t monthsElements, const size_t daysElements,
                             const size_t hoursElements, const OGREnvelope *aoi)
{
  json_t *yearsArray = NULL;
  json_t *monthsArray = NULL;
  json_t *daysArray = NULL;
  json_t *hoursArray = NULL;
  json_t *aoiArray = NULL;
  json_t *jsonRequest = NULL;

  if ((yearsArray = jsonArrayFromIntegers(years, yearsElements, "%.4d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested years\n");
    goto cleanup;
  }

  if ((monthsArray = jsonArrayFromIntegers(months, monthsElements, "%.2d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested months\n");
    goto cleanup;
  }

  if ((daysArray = jsonArrayFromIntegers(days, daysElements, "%.2d")) == NULL) {
    fprintf(stderr, "Failed to create JSON array for requested days\n");
    goto cleanup;
  }

  if ((hoursArray = jsonArrayFromIntegers(hours, hoursElements, "%.2d:00")) == NULL) {
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

[[nodiscard]] stringList *download(CURL *handle, const option_t *options,
                                        const OGREnvelope *aoi)
{
  const unsigned int maxAttempts = 12;

  stringList *root = NULL;

  FILE *logFile = fopen(options->logFile, "a");

  if (logFile == NULL) {
    fprintf(stderr, "Failed to open logfile\n");
    return NULL;
  }

  if (options->downloadByDay) {
    for (size_t yearIdx = 0; yearIdx < options->yearsElements; yearIdx++) {
      for (size_t monthIdx = 0; monthIdx < options->monthsElements; monthIdx++) {
        for (size_t dayIdx = 0; dayIdx < options->daysElements; dayIdx++) {
          int year = options->years[yearIdx];
          int month = options->months[monthIdx];
          int day = options->days[dayIdx];

          char *outputPath = constructFilePath("%s/%.4d-%.2d-%.2d.grib", options->outputDirectory, year, month, day);

          if (outputPath == NULL) {
            fprintf(stderr, "Failed to construct local file path\n");
            continue;
          }

          int requestYears[1] = {year};
          int requestMonths[1] = {month};
          int requestDays[1] = {day};

          if (handleDownloadChain(handle, options, aoi, outputPath, requestYears, requestMonths, requestDays, options->hours, 1, 1, 1, options->hoursElements, maxAttempts)) {
            fprintf(stderr, "Failed to download data for %.4d-%.2d-%.2d", year, month, day);
            unlink(outputPath); // no information at what stage the download failed
            free(outputPath);
            continue;
          }

          if (fprintf(logFile, "%s\tDOWNLOADED\n", outputPath) < 0) {
            fprintf(stderr, "Failed to add downloaded file to log file. Deleting file and continuing.\n");
            unlink(outputPath);
            free(outputPath);
            continue;
          }

          stringList *downloadedFile = calloc(1, sizeof(stringList));
          if (downloadedFile == NULL) {
            perror("calloc");
            fprintf(stderr, "Failed to alloc node in linked list for %s. Deleting file and continuing.\n", outputPath);
            unlink(outputPath);
            free(outputPath);
            continue;
          }

          downloadedFile->string = outputPath;

          if (root == NULL) {
            root = downloadedFile;
          } else {
            downloadedFile->next = root;
            root = downloadedFile;
          }
        }
      }
    }
  } else {
    for (size_t yearIdx = 0; yearIdx < options->yearsElements; yearIdx++) {
      for (size_t monthIdx = 0; monthIdx < options->monthsElements; monthIdx++) {
        int year = options->years[yearIdx];
        int month = options->months[monthIdx];

        char *outputPath = constructFilePath("%s/%.4d-%.2d.grib", options->outputDirectory, year, month);

        if (outputPath == NULL) {
          fprintf(stderr, "Failed to construct local file path\n");
          continue;
        }

        int requestYears[1] = {year};
        int requestMonths[1] = {month};

        if (handleDownloadChain(handle, options, aoi, outputPath, requestYears, requestMonths, options->days, options->hours, 1, 1, options->daysElements, options->hoursElements, maxAttempts)) {
          fprintf(stderr, "Failed to download data for %.4d-%.2d", year, month);
          unlink(outputPath); // no information at what stage the download failed
          free(outputPath);
          continue;
        }

        if (fprintf(logFile, "%s\tDOWNLOADED\n", outputPath) < 0) {
          fprintf(stderr, "Failed to add downloaded file to log file. Deleting file and continuing.\n");
          unlink(outputPath);
          free(outputPath);
          continue;
        }

        stringList *downloadedFile = calloc(1, sizeof(stringList));
        if (downloadedFile == NULL) {
          perror("calloc");
          fprintf(stderr, "Failed to alloc node in linked list for %s. Deleting file and continuing.\n", outputPath);
          unlink(outputPath);
          free(outputPath);
          continue;
        }

        downloadedFile->string = outputPath;

        if (root == NULL) {
          root = downloadedFile;
        } else {
          downloadedFile->next = root;
          root = downloadedFile;
        }
      }
    }
  }

  fclose(logFile);

  return root;
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

[[nodiscard]] int handleDownloadChain(CURL *handle, const option_t *options, const OGREnvelope *aoi, const char *outputPath, const int *subsetYears, const int *subsetMonths, const int *subsetDays, const int *subsetHours, const size_t yearsElements, const size_t monthsElements, const size_t daysElements, const size_t hoursElements, const unsigned int maxAttempts) {
  char *requestId = cdsRequestProduct(handle, subsetYears, subsetMonths, subsetDays,
                                          subsetHours, yearsElements, monthsElements,
                                          daysElements, hoursElements, aoi, options);

  if (requestId == NULL) {
    fprintf(stderr, "Failed to request product or extract job id\n");
    return 1;
  }
#ifdef DEBUG
  printf("Posted product request with Id: %s\n", requestId);
#endif

  if (cdsWaitForProduct(handle, requestId, options, maxAttempts)) {
    fprintf(stderr, "Error while waiting for product\n");
    cdsDeleteProductRequest(handle, requestId, options);
    free(requestId);
    return 1;
  }
#ifdef DEBUG
  printf("Waited for product request with Id: %s\n", requestId);
#endif

  if (cdsDownloadProduct(handle, requestId, outputPath, options)) {
    fprintf(stderr, "Failed to download data.\n");
    cdsDeleteProductRequest(handle, requestId, options);
    free(requestId);
    return 1;
  }
#ifdef DEBUG
  printf("Downloaded file for product request %s\n", requestId);
#endif

  cdsDeleteProductRequest(handle, requestId, options);
#ifdef DEBUG
  printf("Deleted product request with Id: %s\n", requestId);
#endif

  free(requestId);

  return 0;
}

char *cdsRequestProduct(CURL *handle, const int *years, const int *months, const int *days,
                        const int *hours, const size_t yearsElements, const size_t monthsElements,
                        const size_t daysElements, const size_t hoursElements,
                        const OGREnvelope *aoi, const option_t *options)
{
  CURL *requestHandle = curl_easy_duphandle(handle);
  if (requestHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before requesting product\n");
    return NULL;
  }

  char *stringRequest = constructStringRequest(years, months, days, hours,
                        yearsElements, monthsElements, daysElements, hoursElements,
                        aoi);
  if (stringRequest == NULL) {
    fprintf(stderr, "Failed to export JSON to string\n");
    curl_easy_cleanup(requestHandle);
    return NULL;
  }

  char *url = constructURL("%s/%s/%s", BASEURL, "retrieve/v1/processes/reanalysis-era5-single-levels",
                           "execution");
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

productStatus cdsGetProductStatus(CURL *handle, const char *requestId, const option_t *options)
{
  CURL *statusHandle = curl_easy_duphandle(handle);
  if (statusHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before checking product status\n");
    return ERROR;
  }

  char *url = constructURL("%s/%s/%s", BASEURL, "retrieve/v1/jobs", requestId);
  if (url == NULL) {
    fprintf(stderr, "Failed to construct url for product status check\n");
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  curlString response = {0};

  curl_easy_setopt(statusHandle, CURLOPT_URL, url);
  curl_easy_setopt(statusHandle, CURLOPT_WRITEFUNCTION, writeString);
  curl_easy_setopt(statusHandle, CURLOPT_WRITEDATA, (void *) &response);

  struct curl_slist *requestHeader = customHeader(NULL, options);
  if (requestHeader == NULL) {
    fprintf(stderr, "Failed to create custom HTTP header for product status retrieval\n");
    free(url);
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  if ((requestHeader = curl_slist_append(requestHeader, "Content-Type: application/json")) == NULL) {
    free(url);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  curl_easy_setopt(statusHandle, CURLOPT_HTTPHEADER, requestHeader);

  CURLcode statusResponse = curl_easy_perform(statusHandle);
  if (statusResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(statusHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to get product status: %s (%ld)\n", curl_easy_strerror(statusResponse),
            httpResponse);
    free(response.string);
    free(url);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(statusHandle);
    return ERROR;
  }

  char *statusWord = slurpAndGetString(response.string, "status");
  if (statusWord == NULL) {
    free(response.string);
    free(url);
    curl_slist_free_all(requestHeader);
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
  curl_slist_free_all(requestHeader);
  curl_easy_cleanup(statusHandle);

  return status;
}

int binaryExponentialBackoff(int attempt)
{
  if (attempt < 0)
    return -1;

  double backoff = pow(2.0, (double) attempt);

  if (backoff > (double) INT_MAX || fabs(backoff) == HUGE_VAL || backoff == 0.0 || isnan(backoff))
    return -1;

  return (int) backoff;
}

int cdsWaitForProduct(CURL *handle, const char *requestId, const option_t *options, unsigned int maxAttempts)
{
  int sleepSeconds;
  unsigned int attempt = 1;
  while (attempt <= maxAttempts) {
    switch (cdsGetProductStatus(handle, requestId, options)) {
      case SUCCESSFUL:
        return 0;
      case ACCEPTED:
        [[fallthrough]];
      case RUNNING:
        sleepSeconds = binaryExponentialBackoff(attempt);
        if (sleepSeconds == -1)
          return 1;
        break;
      case FAILED:
        fprintf(stderr, "Order %s failed\n", requestId);
        return 1;
      case ERROR:
        fprintf(stderr, "General error occurred while waiting for %s\n", requestId);
        return 1;
    }
    sleep(sleepSeconds);
    attempt++;
  };

  return 1;
}

int cdsDownloadProduct(CURL *handle, const char *requestId, const char *outputPath, const option_t *options)
{
  CURL *downloadHandle = curl_easy_duphandle(handle);
  if (downloadHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before product download\n");
    return 1;
  }

  char *url = constructURL("%s/%s/%s/%s", BASEURL, "retrieve/v1/jobs", requestId, "results");
  if (url == NULL) {
    fprintf(stderr, "Failed to construct jobURL\n");
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  curlString response = {0};

  curl_easy_setopt(downloadHandle, CURLOPT_URL, url);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEDATA, (void *) &response);
  curl_easy_setopt(downloadHandle, CURLOPT_WRITEFUNCTION, writeString);

  struct curl_slist *requestHeader = customHeader(NULL, options);
  if (requestHeader == NULL) {
    fprintf(stderr, "Failed to create custom HTTP header for product download\n");
    free(url);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  if ((requestHeader = curl_slist_append(requestHeader, "Content-Type: application/json")) == NULL) {
    free(url);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  curl_easy_setopt(downloadHandle, CURLOPT_HTTPHEADER, requestHeader);

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
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  char *downloadURL = slurpAndGetString(response.string, "href");
  if (downloadURL == NULL) {
    fprintf(stderr, "Failed to get download URL\n");
    free(url);
    free(response.string);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  FILE *outputFile = fopen(outputPath, "wb");
  if (outputFile == NULL) {
    fprintf(stderr, "Could not open file %s for writing\n", outputPath);
    free(url);
    free(response.string);
    curl_slist_free_all(requestHeader);
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
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(downloadHandle);
    return 1;
  }

  fclose(outputFile);
  free(downloadURL);
  free(response.string);
  free(url);
  curl_slist_free_all(requestHeader);
  curl_easy_cleanup(downloadHandle);

  return 0;
}

int cdsDeleteProductRequest(CURL *handle, const char *requestId, const option_t *options)
{
  CURL *deleteHandle = curl_easy_duphandle(handle);
  if (deleteHandle == NULL) {
    fprintf(stderr, "Failed to duplicate CURL handle before deleting product request\n");
    return 1;
  }

  char *url = constructURL("%s/%s/%s", BASEURL, "retrieve/v1/jobs", requestId);
  if (url == NULL) {
    fprintf(stderr, "Failed to construct url for deletion\n");
    curl_easy_cleanup(deleteHandle);
    return 1;
  }

  curl_easy_setopt(deleteHandle, CURLOPT_URL, url);
  curl_easy_setopt(deleteHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(deleteHandle, CURLOPT_WRITEFUNCTION, discardWrite);

  struct curl_slist *requestHeader = customHeader(NULL, options);
  if (requestHeader == NULL) {
    fprintf(stderr, "Failed to create custom HTTP header for product deletion\n");
    free(url);
    curl_easy_cleanup(deleteHandle);
    return 1;
  }

  if ((requestHeader = curl_slist_append(requestHeader, "Content-Type: application/json")) == NULL) {
    free(url);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(deleteHandle);
    return 1;
  }

  curl_easy_setopt(deleteHandle, CURLOPT_HTTPHEADER, requestHeader);

  CURLcode deleteResponse = curl_easy_perform(deleteHandle);
  if (deleteResponse != CURLE_OK) {
    long httpResponse = 0;
    // not checking validity of httpResponse!
    curl_easy_getinfo(deleteHandle, CURLINFO_RESPONSE_CODE, &httpResponse);
    fprintf(stderr, "Failed to delete product request: %s (%ld)\n", curl_easy_strerror(deleteResponse),
            httpResponse);
    curl_slist_free_all(requestHeader);
    curl_easy_cleanup(deleteHandle);
    free(url);
    return 1;
  }

  curl_slist_free_all(requestHeader);
  curl_easy_cleanup(deleteHandle);
  free(url);

  return 0;
}
