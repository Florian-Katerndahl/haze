#ifndef API_H
#define API_H

#include "types.h"
#include <gdal/ogr_core.h>
#include <stddef.h>
#include <curl/curl.h>
#include <jansson.h>

#define BASEURL "https://cds.climate.copernicus.eu/api"

// passing NULL as first arg creates new list
struct curl_slist *customHeader(struct curl_slist *list, const option_t *options);

void freeCustomHeader(struct curl_slist *list);

CURL *initializeHandle(CURL **handle, const struct curl_slist *headerList);

char *constructURL(const char *basePath, const char *endPoint, const char *requestId);

size_t writeString(char *ptr, size_t size, size_t nmemb, void *userdata);

size_t discardWrite(char *ptr, size_t size, size_t nmemb, void *userdata);

const char *cdsRequestProduct(CURL *handle, const int *years, const int *months, const int *days, const int *hours, const OGREnvelope *aoi, const option_t *options);

productStatus cdsGetProductStatus(CURL *handle, const char *requestId); // Result.update

cdsGetServiceStatus();

int cdsWaitForProduct(CURL *handle, const char *requestId);

cdsDownloadProduct(); // same URL as product status but with /results appended which then returns the actual download URL

int cdsDeleteProductRequest(CURL *handle, const char *requestId); // Result.delete

#endif // API_H