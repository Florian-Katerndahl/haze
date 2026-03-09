/**
 * @file api.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures to interact with ECMWF's CDS API.
 * @version 0.1
 * @date 2026-02-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef API_H
#define API_H

#include "types.h"
#include <gdal/ogr_core.h>
#include <stddef.h>
#include <curl/curl.h>
#include <jansson.h>

#define BASEURL "https://cds.climate.copernicus.eu/api"

/**
 * @brief Create a new header list for cURL with CDS API token field
 *
 * @note After the function returns, the caller owns the returned object and must free it with `curl_slist_free_all` after use.
 *
 * @note This function may free the object pointed to by `list` when encountering an error.
 *
 * @param list Reference to existing `curl_slist` to append to or `NULL` to create a new list.
 * @param options Reference to parsed options.
 * @return struct curl_slist* Reference to cURL header list, NULL on error.
 */
struct curl_slist *customHeader(struct curl_slist *list, const option_t *options);

/**
 * @brief Initialize cURL handle in place
 *
 * @details This function sets the following options: CURLOPT_FOLLOWLOCATION to 1, CURLOPT_FAILONERROR
 *          to 1, CURLOPT_SSL_VERIFYHOST to 1, CURLOPT_TIMEOUT to 600 seconds (default value of cdsapi package),
 *          CURLOPT_USERAGENT to haze and possibly the additional headers.
 *
 * @param handle Indirect reference to a pre-allocated cURL handle.
 * @param headerList Reference to additional cURL headers or NULL.
 * @return int 0 on success, 1 on failure.
 */
int initializeHandle(CURL **handle, const struct curl_slist *headerList);

/**
 * @brief Create a new cURL handle and initialize it
 *
 * @note After the function returns, the caller owns the returned object and must free it with after use.
 *
 * @param list Indirect reference to list storing extra headers, needed beacuse cURL doesn't copy the list.
 * @param options Reference to parsed options, used to access authentication token.
 * @return CURL* Reference to newly created cURL handle, NULL on error.
 */
CURL *newHandleWithOptions(struct curl_slist **list, const option_t *options);

/**
 * @brief Concatenate chunked response from API request into string
 *
 * @note This function is used as a callback for cURL
 *
 * @param ptr Reference to data chunk delivered to cURL.
 * @param size Size of a byte in bytes, i.e. 1.
 * @param nmemb Number of bytes delivered to cURL.
 * @param userdata User data struct to store data in (here: `curlString)`.
 * @return size_t Number of bytes handled by the callback.
 */
size_t writeString(char *ptr, size_t size, size_t nmemb, void *userdata);

/**
 * @brief Discard delivered data from API request
 *
 * @note This function is used as a callback for cURL and always returns `size * nmemb`.
 *
 * @param ptr Reference to data chunk delivered to cURL.
 * @param size Size of a byte in bytes, i.e. 1.
 * @param nmemb Number of bytes delivered to cURL.
 * @param userdata User data struct to store data in.
 * @return size_t Number of bytes handled by the callback.
 */
size_t discardWrite(char *ptr, size_t size, size_t nmemb, void *userdata);

/**
 * @brief Search JSON object recursively for a key with DFS and return first occurence
 *
 * @note The returned reference is borrowed but its reference count is not chnaged. Thus,
 *       the object is only valid for the lifetime of `root`!
 *
 * @param root Reference to root of JSON object.
 * @param key Key to search for.
 * @return json_t* Reference to JSON object or NULL if key is not present.
 */
json_t *getKeyRecursively(json_t *root, const char *key);

/**
 * @brief Convert an array of integers to their string representations with customizable formats
 *
 * @note After the function returns, the caller owns the returned object and musst free it.
 *
 * @param arr Reference to array of integers.
 * @param elemnts Number of entries in respective array.
 * @param formatString Format string to use when converting integers to their string representation, used for each element.
 * @return json_t* Reference to newly allocated JSON array or NULL on error.
 */
json_t *jsonArrayFromIntegers(const int *arr, size_t elements, const char *formatString);

/**
 * @brief Construct a JSON object for CDS API product request
 *
 * @details This function constructs a JSON object to use in API requests containing
 *          information about the years, months, days, hours and AOI to query a particular
 *          CDS API endpoint.
 *          This function is agnostic to any particular product or API endpoint as it simply
 *          packs above-mentioned data.
 *          If the parameter `aoi` is set to NULL, data for the entire globe is queried.
 *
 * @note After the function returns, the caller owns the returned object and musst free it.
 *
 * @param years Reference to array of integers giving years to include in query.
 * @param months Reference to array of integers giving months to include in query.
 * @param days Reference to array of integers giving days to include in query.
 * @param hours Reference to array of integers giving hours to include in query.
 * @param yearsElements Number of entries in respective array.
 * @param monthsElements Number of entries in respective array.
 * @param daysElements Number of entries in respective array.
 * @param hoursElements Number of entries in respective array.
 * @param aoi Reference to a north-up bounding box with EPSG:4326 coordinates to restrict AOI, possibly NULL.
 * @return char* Reference to JSON-formatted product request or NULL on error.
 */
char *constructStringRequest(const int *years, const int *months, const int *days, const int *hours,
                             const size_t yearsElements, const size_t monthsElements, const size_t daysElements,
                             const size_t hoursElements, const OGREnvelope *aoi);

/**
 * @brief Perform product request and download of ERA-5 products
 *
 * @details This function takes options specified by the user and an envelope (possibly NULL)
 *          to query the CDS API, wait for successful processing and downloads file.
 *          A cURL handle is allocated and de-allocated for the scope of this function
 *
 * @param handle Reference to allocated and initialized cURL handle
 * @param options Reference to parsed options.
 * @param aoi Reference to a north-up bounding box with EPSG:4326 coordinates to restrict AOI, possibly NULL.
 * @return stringList* Reference to linked list containing file paths of downloaded files.
 */
[[nodiscard]] stringList *download(CURL *handle, const option_t *options,
                                        const OGREnvelope *aoi);

/**
 * @brief Construct a JSON object from a JSON string and search for a key in DFS
 *
 * @relates getKeyRecursively
 *
 * @note After the function returns, the caller owns the returned object and musst free it.
 *
 * @param input Reference to string representation of JSON object.
 * @param key Key to search for.
 * @return char* Reference to value associated with key or NULL on error.
 */
char *slurpAndGetString(const char *input, const char *key);

/**
 * @brief Wrapper around chain of API requests needed to perform a download
 *
 * @note Time frames for download are to be passed explicitly via arguments
 *       and not the options struct.
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param options Reference to parsed options.
 * @param aoi Reference to a north-up bounding box with EPSG:4326 coordinates to restrict AOI, possibly NULL.
 * @param outputPath Reference to file path, no ownership is taken.
 * @param subsetYears Reference to array of integers giving years to post request for.
 * @param subsetMonths Reference to array of integers giving months to post request for.
 * @param subsetDays Reference to array of integers giving days to post request for.
 * @param subsetHours Reference to array of integers giving hours to post request for.
 * @param yearsElements Number of entries in respective array.
 * @param monthsElements Number of entries in respective array.
 * @param daysElements Number of entries in respective array.
 * @param hoursElements Number of entries in respective array.
 * @param maxAttempts Maximum number of status requests made before request is deemed faulty.
 * @return int 0 on success, 1 on error.
 */
[[nodiscard]] int handleDownloadChain(CURL *handle, const option_t *options, const OGREnvelope *aoi, const char *outputPath, const int *subsetYears, const int *subsetMonths, const int *subsetDays, const int *subsetHours, const size_t yearsElements, const size_t monthsElements, const size_t daysElements, const size_t hoursElements, const unsigned int maxAttempts);

/**
 * @brief Perform product request with CDS API
 *
 * @details This function performs the acutal request to the CDS API by constructing
 *          a suitable JSON payload and contact the correct API endpoint.
 *          The function returns the job/request ID assigned by CDS.
 *
 * @note After the function returns, the caller owns the returned object and musst free it.
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param years Reference to array of integers giving years to post request for.
 * @param months Reference to array of integers giving months to post request for.
 * @param days Reference to array of integers giving days to post request for.
 * @param hours Reference to array of integers giving hours to post request for.
 * @param yearsElements Number of entries in respective array.
 * @param monthsElements Number of entries in respective array.
 * @param daysElements Number of entries in respective array.
 * @param hoursElements Number of entries in respective array.
 * @param aoi Reference to a north-up bounding box with EPSG:4326 coordinates to restrict AOI, possibly NULL.
 * @param options Reference to parsed options.
 * @return char* Job/Request ID, NULL on error.
 */
char *cdsRequestProduct(CURL *handle, const int *years, const int *months, const int *days,
                        const int *hours, const size_t yearsElements, const size_t monthsElements,
                        const size_t daysElements, const size_t hoursElements, const OGREnvelope *aoi,
                        const option_t *options);

/**
 * @brief Query the CDS API for the status of a previously created product request
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param requestId Request ID to query.
 * @param options Reference to parsed options.
 * @return productStatus Product status.
 */
productStatus cdsGetProductStatus(CURL *handle, const char *requestId, const option_t *options); // Result.update

/**
 * @brief Compute binary exponential backoff
 *
 * @param attempt Attempt for which binary exponential backoff should be computed.
 * @return int Seconds to wait until next attempt, -1 on overflow/underflow or when attempt is negative.
 */
int binaryExponentialBackoff(int attempt);

/**
 * @brief Wait for a product request to be downloadable with a binary exponential backoff
 *
 * @note An error is returned when the product request failed or a
 *       general error is encountered.
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param requestId Request ID to wait on.
 * @param options Reference to parsed options.
 * @param maxAttempts Maximum number of connections attempts to perform. This should be a rather
 *                    high value as binary exponential backoff is used to increase time between
 *                    connection requests.
 * @return int 0 on success, 1 on error.
 */
int cdsWaitForProduct(CURL *handle, const char *requestId, const option_t *options, unsigned int maxAttempts);

/**
 * @brief Download data related to previously made request to local file
 *
 * @note This function assumes the order is already processed and ready to download.
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param requestId Request ID to download.
 * @param outputPath File path to download file to.
 * @param options Reference to parsed options.
 * @return int 0 on success, 1 on failure.
 */
int cdsDownloadProduct(CURL *handle, const char *requestId,
                       const char *outputPath, const option_t *options); // same URL as product status but with /results appended which then returns the actual download URL

/**
 * @brief Delete a prodcut request from personal CDS space
 *
 * @param handle Reference to existing cURL handle used for request after duplication.
 * @param requestId Request ID to delete.
 * @param options Reference to parsed options.
 * @return int 0 on success, 1 on failure.
 */
int cdsDeleteProductRequest(CURL *handle, const char *requestId, const option_t *options); // Result.delete

#endif // API_H