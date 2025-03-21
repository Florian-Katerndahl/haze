#define _POSIX_C_SOURCE 200809L

#include "options.h"
#include "types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <getopt.h>

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
  printf("\toutdir:  Directory into which output CSV's are written.\n");
}

[[nodiscard]] option_t *parseOptions(int argc, char *argv[])
{
  option_t *userOptions = calloc(1, sizeof(option_t));
  if (userOptions == NULL) {
    perror("calloc");
    exit(1); // exiting here should be ok
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
        printHelp();
        exit(0);
      case 'y':
        if (parseIntegers(userOptions->years, MAXYEAR, optarg, 1940, 2039)) {
          fprintf(stderr, "Failed to parse years or argument not specified\n");
          exit(1);
        }
        break;
      case 'm':
        if (parseIntegers(userOptions->months, MAXMONTH, optarg, 1, 12)) {
          fprintf(stderr, "Failed to parse months or argument not specified\n");
          exit(1);
        }
        break;
      case 'd':
        if (parseIntegers(userOptions->days, MAXDAY, optarg, 1, 31)) {
          fprintf(stderr, "Failed to parse days or argument not specified\n");
          exit(1);
        }
        break;
      case 't':
        if (parseIntegers(userOptions->hours, MAXHOUR, optarg, 0, 23)) {
          fprintf(stderr, "Failed to parse hours or argument not specified\n");
          exit(1);
        }
        break;
      case '?': // erroneous option specified
        break;
      default:
        fprintf(stderr, "Unknown return value: %c. Continuing regardless.\n", opt);
        break;
    }
  }

  if (argc - optind != 2) {
    fprintf(stderr, "Missing positional arguments\n");
    printHelp();
    exit(1);
  }

  userOptions->areaOfInterest = argv[optind];
  optind++;
  userOptions->outputDirectory = argv[optind];

  if (getAuthentication(&userOptions->authenticationToken, NULL, &userOptions->withAllocation) == 1) {
    fprintf(stderr, "Failed to get authentication token from enironment or $HOME/.cdsapirc\n");
    printHelp();
    exit(1);
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

int parseRange(int *arr, size_t n, const char *argString)
{
  const char *sep = strchr(argString, ':');
  sep++;
  int min = atoi(argString);
  int max = atoi(sep);
  if (max < min)
    return 1;
  int val = min;
  for (size_t i = 0; i < n && val <= max; i++, val++) {
    arr[i] = val;
  }

  return 0;
}

int parseList(int *arr, size_t n, char *argString)
{
  char *str;
  const char *token;
  size_t i;

  for (i = 0, str = argString; i < n ; i++, str = NULL) {
    token = strtok(str, ",");
    if (token == NULL)
      break;
    arr[i] = atoi(token);
  }

  return 0;
}

int parseSingle(int *arr, const char *argString)
{
  arr[0] = atoi(argString);
  return 0;
}

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

int getAuthentication(char **authenticationToken, const char *filePath, bool *neededAllocation)
{
  if (getAuthenticationFromEnvironment(authenticationToken) == 0) {
    *neededAllocation = false;
    return 0;
  }

  if (getAuthenticationFromFile(authenticationToken, filePath) == 0) {
    *neededAllocation = true;
    return 0;
  }

  return 1;
}

int getAuthenticationFromEnvironment(char **authenticationToken)
{
  *authenticationToken = getenv("ADSAUTH");

  if (*authenticationToken == NULL) {
    return 1;
  }

  return 0;
}

int getAuthenticationFromFile(char **authenticationToken, const char *filePath)
{
  FILE *f;

  if (filePath == NULL) {
    const char *home = getenv("HOME");
    if (home == NULL) {
      fprintf(stderr, "Failed to get home directory path\n");
      return 1;
    }

    char path[1024];

    int charsWritten = snprintf(path, sizeof(path), "%s/%s", home, ".cdsapirc");
    if ((size_t) charsWritten >= sizeof(path) || charsWritten < 0) {
      fprintf(stderr, "Failed to assemble file path for default location of '.cdsapirc'\n");
      return 1;
    }

    f = fopen(path, "rt");
  } else {
    f = fopen(filePath, "rt");
  }

  if (f == NULL) {
    perror("fopen");
    return 1;
  }

  // skip first line by reading chars sequentially
  while (fgetc(f) != '\n');
  if (feof(f)) {
    fprintf(stderr, "Authentication ended unexpectedly\n");
    fclose(f);
    return 1;
  }

  char *key = NULL;
  size_t readBytes = 0;
  ssize_t charsRead = 0;
  if ((charsRead = getline(&key, &readBytes, f)) == -1) {
    perror("getline");
    free(key);
    fclose(f);
    return 1;
  }

  if (key[charsRead - 1] == '\n')
    key[charsRead - 1] = '\0';

  char *token = strchr(key, ' ');
  if (token == NULL) {
    fprintf(stderr, "'.cdsapirc' is malformed\n");
    free(key);
    fclose(f);
    return 1;
  }

  token++;
  if (token == NULL) {
    fprintf(stderr, "'.cdsapirc' is malformed\n");
    free(key);
    fclose(f);
    return 1;
  }

  *authenticationToken = calloc(strlen(token) + 1, sizeof(char));
  if (*authenticationToken == NULL) {
    perror("calloc");
    free(key);
    fclose(f);
    return 1;
  }

  strcpy(*authenticationToken, token);

  free(key);
  fclose(f);

  return 0;
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