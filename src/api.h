#ifndef API_H
#define API_H

#include "types.h"
#include <gdal/ogr_core.h>
#include <stddef.h>
#include <curl/curl.h>
#include <jansson.h>

#define BASEURL "https://cds.climate.copernicus.eu/api"

struct curl_slist *customHeader(const option_t *options);

void freeCustomHeader(struct curl_slist *list);

CURL *initializeHandle(CURL **handle, const struct curl_slist *headerList);

char *constructURL(const char *basePath, const char *endPoint, const char *requestId);

size_t writeString(char *ptr, size_t size, size_t nmemb, void *userdata);

char *cdsRequestProduct(CURL *handle, const int *years, const int *months, const int *days, const int *hours, const OGREnvelope *aoi);

productStatus cdsGetProductStatus(CURL *handle, const char *requestId); // Result.update

cdsGetServiceStatus();

int cdsWaitForProduct(CURL *handle, const char *requestId);

cdsDownloadProduct();

int cdsDeleteProductRequest(CURL *handle, const char *requestId); // Result.delete

#endif // API_H