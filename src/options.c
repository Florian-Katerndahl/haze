#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include "options.h"
#include "fscheck.h"
#include "types.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <getopt.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>

void printHelp(void)
{
  printf("haze - Integrate reprocessed ERA5 single level data into FORCE for atmospheric correction\n");
  printf("usage: haze [-h|--help] --year --month --day --hour aoi outdir\n\n");
  printf("Optional arguments:\n");
  printf("\t-h|--help: Print help and exit.\n");
  printf("Mandatory keyword arguments (either as start:stop or comma seperated list. In the first case, endpoints are inclusive.):\n");
  printf("\t--year:  Years for which data should be downloaded.\n");
  printf("\t--month: Months for which data should be downloaded.\n");
  printf("\t--day:   Days for which data should be downloaded.\n");
  printf("\t--hour:  Hours for which data should be downloaded (zero-based).\n");
  printf("Mandatory positional arguments:\n");
  printf("\taoi:     File path to OGR-readble file containing one or more polygons for which to extract data. First layer is read.\n");
  printf("\toutdir:  Directory into which output CSVs are written.\n");
}

[[nodiscard]] option_t *parseOptions(int argc, char *argv[])
{
  option_t *userOptions = calloc(1, sizeof(option_t));
  if (userOptions == NULL) {
    perror("calloc");
    fputc('\n', stderr);
    return NULL;
  }

  userOptions->yearsElements = 0;
  userOptions->monthsElements = 0;
  userOptions->daysElements = 0;
  userOptions->hoursElements = 0;

  static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"year", required_argument, 0, 'y'},
    {"month", required_argument, 0, 'm'},
    {"day", required_argument, 0, 'd'},
    {"hour", required_argument, 0, 't'},
    {0, 0, 0, 0}
  };

  int opt;

  while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        userOptions->printHelp = true;
        return userOptions;
      case 'y':
        if (parseIntegers(userOptions->years, MAXYEAR, &userOptions->yearsElements, optarg, 1940, 2039)) {
          fprintf(stderr, "Failed to parse years or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 'm':
        if (parseIntegers(userOptions->months, MAXMONTH, &userOptions->monthsElements, optarg, 1, 12)) {
          fprintf(stderr, "Failed to parse months or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 'd':
        if (parseIntegers(userOptions->days, MAXDAY, &userOptions->daysElements, optarg, 1, 31)) {
          fprintf(stderr, "Failed to parse days or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 't':
        if (parseIntegers(userOptions->hours, MAXHOUR, &userOptions->hoursElements, optarg, 0, 23)) {
          fprintf(stderr, "Failed to parse hours or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case '?': [[fallthrough]];
      default:
        fprintf(stderr, "Unknown return value: %c. Continuing regardless.\n\n", opt);
        break;
    }
  }

  int positionalArguments = argc - optind;

  if (positionalArguments == 2) {
    userOptions->areaOfInterest = argv[optind];
    optind++;
    userOptions->outputDirectory = argv[optind];
  } else {
    fprintf(stderr, "Missing positional arguments\n\n");
    freeOption(userOptions);
    return NULL;
  }

  if (!(fileExists(userOptions->areaOfInterest) && fileReadable(userOptions->areaOfInterest))) {
    fprintf(stderr, "AOI file not readable\n\n");
    freeOption(userOptions);
    return NULL;
  }

  if (!(fileExists(userOptions->outputDirectory) && fileWritable(userOptions->outputDirectory))) {
    fprintf(stderr, "Output directory not writable\n\n");
    freeOption(userOptions);
    return NULL;
  }

  // TODO propagate changes through code base that output directory is now guaruanteed to end without slash
  forceNoTrailingSlash(userOptions);

  if (getAuthentication(&userOptions->authenticationToken, NULL) == 1) {
    fprintf(stderr, "Failed to get authentication token from enironment or $HOME/.cdsapirc\n\n");
    freeOption(userOptions);
    return NULL;
  }

  return userOptions;
}

int parseIntegers(int *arr, size_t capacity, size_t *elements, char *argString, const int min, const int max)
{
  if (strchr(argString, ':') != NULL) {
    if (parseRange(arr, capacity, elements, argString))
      return 1;
  } else if (strchr(argString, ',') != NULL) {
    if (parseList(arr, capacity, elements, argString))
      return 1;
  } else {
    if (parseSingle(arr, elements, argString))
      return 1;
  }

  if (validateArray(arr, *elements, min, max) == false) {
    return 1;
  }

  return 0;
}

int convertPositiveIntegerSafely(const char *string)
{
  if (string == NULL) {
    return -1;
  }

  errno = 0;
  char *endptr;
  long value = strtol(string, &endptr, 10);

  if ((errno == ERANGE && (value == LONG_MAX || value == LONG_MIN))
      || (errno != 0 && value == 0)
      || (endptr == string)
      || (value < INT_MIN || value > INT_MAX)) {
        return -1;
      }

  return (int) value;
}

int parseRange(int *arr, size_t capacity, size_t *elements, const char *argString)
{
  if (argString == NULL) {
    return 1;
  }

  const char *sep = strchr(argString, ':');
  if (sep == NULL) {
    return 1;
  }

  sep++;

  int min = convertPositiveIntegerSafely(argString);
  int max = convertPositiveIntegerSafely(sep);

  if (max < min || max == -1 || min == -1) {
    return 1;
  }

  int val = min;

  for (size_t i = 0; i < capacity && val <= max; i++, val++) {
    arr[i] = val;
    (*elements)++;
  }

  // range not exhausted, i.e. array cannot hold anymore values
  if (val < max && capacity == *elements) {
    return 1;
  }

  return 0;
}

int parseList(int *arr, size_t capacity, size_t *elements, char *argString)
{
  if (argString == NULL) {
    return 1;
  }

  int val;
  const char *token;

  for (size_t i = 0; i < capacity; i++, argString = NULL) {
    token = strtok(argString, ",");
    if (token == NULL) {
      break;
    }
    
    if ((val = convertPositiveIntegerSafely(token)) == -1) {
      return 1;
    }
    
    arr[i] = val;
    (*elements)++;
  }

  // no token found, immediately jumped to end of string
  if (*elements == 0) {
    return 1;
  }

  return 0;
}

int parseSingle(int *arr, size_t *elements, const char *argString)
{
  arr[0] = convertPositiveIntegerSafely(argString);
  if (arr[0] == -1) {
    return 1;
  }

  (*elements)++;

  return 0;
}

bool validateArray(int *arr, const size_t elements, const int min, const int max)
{
  if (elements == 0) {
    return false;
  }

  // bounds check
  for (size_t i = 0; i < elements; i++) {
    if (min > arr[i] || arr[i] > max) {
      return false;
    }
  }

  // as array stores numeric information, it can be sorted without loss of semantic information
  qsort(arr, elements, sizeof(int), intcmp);

  // check for uniqueness of values
  for (ssize_t l = -1, c = 0, r = 1; c < elements; l++, c++, r++) {
    if ((c == 0 && r < elements && arr[c] == arr[r]) ||
        (c == elements - 1 && arr[l] == arr[c]) ||
        (c > 0 && c < elements - 1 && (arr[l] == arr[c] || arr[c] == arr[r]))) {
          return false;
        }
  }

  return true;
}

int getAuthentication(char **authenticationToken, const char *filePath)
{
  if (getAuthenticationFromEnvironment(authenticationToken) == 0) {
    return 0;
  }

  if (getAuthenticationFromFile(authenticationToken, filePath) == 0) {
    return 0;
  }

  return 1;
}

int getAuthenticationFromEnvironment(char **authenticationToken)
{
  *authenticationToken = strdup(getenv("ADSAUTH"));

  return (*authenticationToken != NULL) ? 0 : 1;;
}

int getAuthenticationFromFile(char **authenticationToken, const char *filePath)
{
  const char *cdsapirc = filePath;
  bool heapAlloced = false;

  if (!cdsapirc) {
    const char *home = getenv("HOME");

    if (home == NULL) {
      fprintf(stderr, "Failed to get home directory path\n");
      return 1;
    }

    cdsapirc = constructFormattedPath("%s/%s", home, ".cdsapirc");
    heapAlloced = true;

    if (cdsapirc == NULL) {
      fprintf(stderr, "Failed to assemble file path for default location of '.cdsapirc'\n");
      free((void *) cdsapirc);
      return 1;
    }
  }

  *authenticationToken = extractKey(cdsapirc);

  if (heapAlloced) free((void *) cdsapirc);

  return (*authenticationToken != NULL) ? 0 : 1;
}

[[nodiscard]] char *extractKey(const char *cdsapirc) {
    if (!cdsapirc) return NULL;

    if (fileReadable(cdsapirc) == false) return NULL;

    FILE *f = fopen(cdsapirc, "r");

    if (f == NULL) {
      perror("fopen");
      return NULL;
    }

    char *lineptr = NULL;
    char *key = NULL;
    size_t n = 0;
    ssize_t charsRead;
    
    while ((charsRead = getline(&lineptr, &n, f)) != -1) {
        // does the line read start with the delimeter/parameter we're after?
        char *startOfDelim = strstr(lineptr, "key: ");
        
        if (startOfDelim == NULL) {
            free(lineptr);
            lineptr = NULL;
            continue;
        }

        // valid because check above already determined that the line includes whitespace
        char *startOfKey = startOfDelim + 5;

        // cannot return NULL because getline 0-terminates line
        char *endOfKey = strchr(lineptr, '\0');
        
        // length of key
        ptrdiff_t stringLength = (ptrdiff_t) endOfKey - (ptrdiff_t) startOfKey;

        // if line is terminated by newline, actual key is one byte less then extracted line
        // assumes no additional padding of file
        if (*(endOfKey - 1) == '\n') stringLength--;

        key = strndup(startOfKey, stringLength);
                
        break;
    }
    
    free(lineptr);

    fclose(f);

    return key;
}

void forceNoTrailingSlash(const option_t *options) {
  size_t ouputDirectoryLength = strlen(options->outputDirectory);

  if (options->outputDirectory[ouputDirectoryLength] == '/') {
    options->outputDirectory[ouputDirectoryLength] = '\0';
  }

  return;
}

void printOptions(const option_t *options)
{
  printf("YEAR: ");
  for (int i = 0; i < options->yearsElements; i++) {
    printf("%d ", options->years[i]);
  }

  printf("\nMONTH: ");
  for (int i = 0; i < options->monthsElements; i++) {
    printf("%d ", options->months[i]);
  }

  printf("\nDAY: ");
  for (int i = 0; i < options->daysElements; i++) {
    printf("%d ", options->days[i]);
  }

  printf("\nHOUR: ");
  for (int i = 0; i < options->hoursElements; i++) {
    printf("%d ", options->hours[i]);
  }
  printf("\n");

  printf("Auth token: '%s'\n", options->authenticationToken);

  printf("aoi file: %s\n", options->areaOfInterest);

  printf("out directory: %s\n", options->outputDirectory);
}

int intcmp(const void *a, const void *b)
{
  int aInt = *(int *) a;
  int bInt = *(int *) b;

  return aInt - bInt;
}
