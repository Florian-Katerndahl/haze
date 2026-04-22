#define _POSIX_C_SOURCE 200809L
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "paths.h"
#include "options.h"
#include "fscheck.h"
#include "types.h"
#include "math-utils.h"
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
#include <libgen.h>

void printHelp(void)
{
  printf("haze - Integrate reprocessed ERA5 single level data into FORCE for atmospheric correction\n");
  printf("Usage: haze <subprogram> <options>\n");
  printf("Where <subprogram> is either 'download' to download data from CDS or 'process' to process downloaded files\n");
  printf("<options> is of the form: [-h|--help] [-g|--global] [-l|--layer] [-d|--daily] --year --month --day --hour --logfile aoi outdir\n");
  printf("\nGlobal optional keyword arguments (valid for both subprograms):\n");
  printf("\t-h|--help:  Print help and exit.\n");
  printf("\t-l|--layer: Layer to open from AOI dataset.\n");
  printf("Global mandatory keyword arguments:\n");
  printf("\t--logfile: Path to logfile storing successful downloads and processing statuses.\n");
  printf("\nOptional keyword arguments valid for download subprogram:\n");
  printf("\t-g|--global: Request product worldwide instead of using an AOI dataset.\n");
  printf("\t-d|--daily:  Group product requests by day instead of month.\n");
  printf("\nOptional keyword arguments valid for processing subprogram:\n");
  printf("\t--footprint: If specified, multipolygons are considered footprint geometries and those cut at the dateline are merged to a polygon to compute centroid.\n");
  printf("Mandatory keyword arguments valid for download subprogram (either scalar vlaue, start:stop or comma seperated list. In the first case, endpoints are inclusive.):\n");
  printf("\t--year:  Years for which data should be downloaded.\n");
  printf("\t--month: Months for which data should be downloaded.\n");
  printf("\t--day:   Days for which data should be downloaded.\n");
  printf("\t--hour:  Hours for which data should be downloaded (zero-based).\n");
  printf("Mandatory positional arguments:\n");
  printf("\taoi:    File path to OGR-readble file containing one or more polygons for which to extract data. Either `layer` or the first layer is read. Mandatory for processing.\n");
  printf("\toutdir: Directory into which output data products and CSVs are written.\n");
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
  userOptions->downloadByDay = false;
  userOptions->global = false;
  userOptions->logFile = NULL;
  userOptions->areaOfInterest = NULL;
  userOptions->aoiName = NULL;
  userOptions->outputDirectory = NULL;
  userOptions->authenticationToken = NULL;
  userOptions->download = false;
  userOptions->process = false;
  userOptions->footprint = false;

  static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"year", required_argument, NULL, 'y'},
    {"month", required_argument, NULL, 'm'},
    {"day", required_argument, NULL, 69},
    {"hour", required_argument, NULL, 't'},
    {"logfile", required_argument, NULL, 67},
    {"global", no_argument, NULL, 'g'},
    {"layer", required_argument, NULL, 'l'},
    {"daily", no_argument, NULL, 'd'},
    {"footprint", no_argument, NULL, 'f'},
    {0, 0, 0, 0}
  };

  int opt;

  while ((opt = getopt_long(argc, argv, "hl:gd", long_options, NULL)) != -1) {
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
      case 69:
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
      case 67:
        userOptions->logFile = optarg;
        break;
      case 'g':
        userOptions->global = true;
        break;
      case 'l':
        userOptions->aoiName = optarg;
        break;
      case 'd':
        userOptions->downloadByDay = true;
        break;
      case 'f':
        userOptions->footprint = true;
        break;
      case '?':
        [[fallthrough]];
      default:
        fprintf(stderr, "Unknown return value: %c. Continuing regardless.\n\n", opt);
        break;
    }
  }

  int positionalArguments = argc - optind;

  if (positionalArguments == 0 || positionalArguments > 3) {
    fprintf(stderr, "Encountered wrong number of positional arguments\n\n");
    freeOption(userOptions);
    return NULL;
  }

  userOptions->download = strcmp("download", argv[optind]) == 0;
  userOptions->process = strcmp("process", argv[optind]) == 0;

  if (!(userOptions->download || userOptions->process)) {
    fprintf(stderr, "Unknown sub-program specified: '%s'\n", argv[optind]);
    freeOption(userOptions);
    return NULL;
  }

  positionalArguments--;
  optind++;

  if (positionalArguments == 1 && (userOptions->global && userOptions->download)) {
    userOptions->outputDirectory = argv[optind];
  } else if (positionalArguments == 2 && ((!userOptions->global && userOptions->download)
                                          || userOptions->process)) {
    userOptions->areaOfInterest = argv[optind];
    optind++;
    userOptions->outputDirectory = argv[optind];
  } else {
    fprintf(stderr, "Encountered wrong number of positional arguments\n\n");
    freeOption(userOptions);
    return NULL;
  }

  char *dupedLogFile = strdup(userOptions->logFile);
  if (dupedLogFile == NULL) {
    fprintf(stderr, "Failed to duplicate log file path\n");
    freeOption(userOptions);
    return NULL;
  }

  bool areaOfInterestExists = false;
  bool areaOfInterestReadable = false;
  bool outputDirectoryExists = fileExists(userOptions->outputDirectory);
  bool outputDirectoryWritable = fileWritable(userOptions->outputDirectory);
  bool logFileExists = fileExists(userOptions->logFile);
  bool logFileReadable = fileReadable(userOptions->logFile);
  bool logFileWritable = fileWritable(userOptions->logFile);
  // this check is needed when a log-file is to be freshly created!
  bool logFileParentDirWritable = fileWritable(dirname(dupedLogFile));

  free(dupedLogFile);

  if (userOptions->areaOfInterest) {
    areaOfInterestExists = fileExists(userOptions->areaOfInterest);
    areaOfInterestReadable = fileReadable(userOptions->areaOfInterest);
  }

  if ((!userOptions->global || userOptions->process) && !areaOfInterestExists) {
    fprintf(stderr, "AOI file '%s' does not exist\n\n", userOptions->areaOfInterest);
    freeOption(userOptions);
    return NULL;
  }

  if ((!userOptions->global || userOptions->process) && !areaOfInterestReadable) {
    fprintf(stderr, "AOI file '%s' not readable\n\n", userOptions->areaOfInterest);
    freeOption(userOptions);
    return NULL;
  }

  if (!outputDirectoryExists) {
    fprintf(stderr, "Output directory '%s' does not exist\n\n", userOptions->outputDirectory);
    freeOption(userOptions);
    return NULL;
  }

  if (!outputDirectoryWritable) {
    fprintf(stderr, "Output directory '%s' not writable\n\n", userOptions->outputDirectory);
    freeOption(userOptions);
    return NULL;
  }

  if ((!logFileExists && userOptions->process) || (!logFileParentDirWritable && userOptions->download)) {
    fprintf(stderr, "Logfile '%s' does not exist or cannot be created\n\n", userOptions->logFile);
    freeOption(userOptions);
    return NULL;
  }

  if (logFileExists && !(logFileReadable || logFileWritable)) {
    fprintf(stderr, "Logfile '%s' is not readable and writable\n\n", userOptions->logFile);
    freeOption(userOptions);
    return NULL;
  }

  forceNoTrailingSlash(userOptions);

  if (userOptions->download && getAuthentication(&userOptions->authenticationToken, NULL) == 1) {
    fprintf(stderr, "Failed to get authentication token from environment or $HOME/.cdsapirc\n\n");
    freeOption(userOptions);
    return NULL;
  }

  return userOptions;
}

int parseIntegers(int *arr, size_t capacity, size_t *elements, char *argString, const int min,
                  const int max)
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
  // well, not actually an error regarding the array,
  // but I can't compare otherwise further down below
  if (elements >= LONG_MAX) {
    return false;
  }

  if (elements == 0) {
    return false;
  }

  // bounds check
  for (size_t i = 0; i < elements; i++) {
    if (min > arr[i] || arr[i] > max) {
      return false;
    }
  }

  // as array stores numeric information, it can be sorted
  // without loss of semantic information
  qsort(arr, elements, sizeof(int), intcmp);

  // check for uniqueness of values
  for (ssize_t l = -1, c = 0, r = 1; c < (ssize_t) elements; l++, c++, r++) {
    if ((c == 0 && r < (ssize_t) elements && arr[c] == arr[r]) ||
        (c == (ssize_t) elements - 1 && arr[l] == arr[c]) ||
        (c > 0 && c < (ssize_t) elements - 1 && (arr[l] == arr[c] || arr[c] == arr[r]))) {
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
  char *environmentToken = getenv("ADSAUTH");

  if (environmentToken == NULL)
    return 1;

  *authenticationToken = strdup(environmentToken);

  return 0;
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

    cdsapirc = constructFilePath("%s/%s", home, ".cdsapirc");
    heapAlloced = true;

    if (cdsapirc == NULL) {
      fprintf(stderr, "Failed to assemble file path for default location of '.cdsapirc'\n");
      free((void *) cdsapirc);
      return 1;
    }
  }

  *authenticationToken = extractKey(cdsapirc);

  if (heapAlloced)
    free((void *) cdsapirc);

  return (*authenticationToken != NULL) ? 0 : 1;
}

[[nodiscard]] char *extractKey(const char *cdsapirc)
{
  if (!cdsapirc)
    return NULL;

  if (fileReadable(cdsapirc) == false)
    return NULL;

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
    if (*(endOfKey - 1) == '\n')
      stringLength--;

    key = strndup(startOfKey, stringLength);

    break;
  }

  free(lineptr);

  fclose(f);

  return key;
}

void forceNoTrailingSlash(const option_t *options)
{
  size_t ouputDirectoryLength = strlen(options->outputDirectory);

  if (options->outputDirectory[ouputDirectoryLength - 1] == '/') {
    options->outputDirectory[ouputDirectoryLength - 1] = '\0';
  }

  return;
}

/// TODO: needs to be updated with new fields
void printOptions(const option_t *options)
{
  printf("YEAR: ");
  for (size_t i = 0; i < options->yearsElements; i++) {
    printf("%d ", options->years[i]);
  }

  printf("\nMONTH: ");
  for (size_t i = 0; i < options->monthsElements; i++) {
    printf("%d ", options->months[i]);
  }

  printf("\nDAY: ");
  for (size_t i = 0; i < options->daysElements; i++) {
    printf("%d ", options->days[i]);
  }

  printf("\nHOUR: ");
  for (size_t i = 0; i < options->hoursElements; i++) {
    printf("%d ", options->hours[i]);
  }
  printf("\n");

  printf("Log file: '%s'\n", options->logFile);

  printf("Auth token: '%s'\n", options->authenticationToken);

  printf("aoi file: %s\n", options->areaOfInterest);

  printf("out directory: %s\n", options->outputDirectory);
}
