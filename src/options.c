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

  memset(userOptions->years, INITVAL, MAXYEAR * sizeof(int));
  memset(userOptions->months, INITVAL, MAXMONTH * sizeof(int));
  memset(userOptions->days, INITVAL, MAXDAY * sizeof(int));
  memset(userOptions->hours, INITVAL, MAXHOUR * sizeof(int));

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
        if (parseIntegers(userOptions->years, MAXYEAR, optarg, 1940, 2039)) {
          fprintf(stderr, "Failed to parse years or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 'm':
        if (parseIntegers(userOptions->months, MAXMONTH, optarg, 1, 12)) {
          fprintf(stderr, "Failed to parse months or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 'd':
        if (parseIntegers(userOptions->days, MAXDAY, optarg, 1, 31)) {
          fprintf(stderr, "Failed to parse days or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case 't':
        if (parseIntegers(userOptions->hours, MAXHOUR, optarg, 0, 23)) {
          fprintf(stderr, "Failed to parse hours or argument not specified\n\n");
          freeOption(userOptions);
          return NULL;
        }
        break;
      case '?': [[fallthrough]]
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
    freeOptions(userOptions);
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

int parseIntegers(int *arr, size_t n, char *argString, const int min, const int max)
{
  if (strchr(argString, ':') != NULL) {
    if (parseRange(arr, n, argString))
      return 1;
  } else if (strchr(argString, ',') != NULL) {
    if (parseList(arr, n, argString))
      return 1;
  } else {
    if (parseSingle(arr, argString))
      return 1;
  }

  if (validateArray(arr, n, min, max) == false) {
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

  if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
      || (errno != 0 && val == 0)
      || (endptr == string)
      || (val < INT_MIN || val > INT_MAX)) {
        return -1;
      }

  return (int) val;
}

int parseRange(int *arr, size_t n, const char *argString)
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

  for (size_t i = 0; i < n && val <= max; i++, val++) {
    arr[i] = val;
  }

  // range not exhausted, i.e. array cannot hold anymore values
  if (val < max) {
    return 1;
  }

  return 0;
}

int parseList(int *arr, size_t n, char *argString)
{
  if (argString == NULL) {
    return 1;
  }

  size_t count = 0;
  const char *token;

  for (size_t i = 0; i < n ; i++, argString = NULL) {
    token = strtok(argString, ",");
    if (token == NULL) {
      break;
    }

    count++;
    
    arr[i] = convertPositiveIntegerSafely(token);
    if (arr[i] == -1) {
      return 1;
    }
  }

  // no token found, immediately jumped to end of string
  if (count == 0) {
    return 1;
  }

  return 0;
}

int parseSingle(int *arr, const char *argString)
{
  arr[0] = convertPositiveIntegerSafely(argString);
  return arr[0] == -1;
}

/// TODO: the initval stuff seems like it can be handled better, e.g. by using additional variables to indicate the length and capacity!
/// TODO: shouldn't this also check for uniqueness of the values as this is also a requirement for API queries?
bool validateArray(const int *arr, const size_t n, const int min, const int max)
{
  if (arr[0] == INITVAL)
    return false;

  for (size_t i = 0; i < n; i++) {
    if ((arr[i] < min || arr[i] > max) && (i > 0 && arr[i] != INITVAL))
      return false;
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
  for (int i = 0; i < MAXYEAR && options->years[i] != INITVAL; i++) {
    printf("%d ", options->years[i]);
  }

  printf("\nMONTH: ");
  for (int i = 0; i < MAXMONTH && options->months[i] != INITVAL; i++) {
    printf("%d ", options->months[i]);
  }

  printf("\nDAY: ");
  for (int i = 0; i < MAXDAY && options->days[i] != INITVAL; i++) {
    printf("%d ", options->days[i]);
  }

  printf("\nHOUR: ");
  for (int i = 0; i < MAXHOUR && options->hours[i] != INITVAL; i++) {
    printf("%d ", options->hours[i]);
  }
  printf("\n");

  printf("Auth token: '%s'\n", options->authenticationToken);

  printf("aoi file: %s\n", options->areaOfInterest);

  printf("out directory: %s\n", options->outputDirectory);
}
