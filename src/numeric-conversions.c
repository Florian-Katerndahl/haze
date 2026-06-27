#include "numeric-conversions.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

long long convertNumberSafely(const char *timeString, bool *error)
{
    *error = false;
    char *endptr;
    errno = 0;
    long long result = strtoll(timeString, &endptr, 10);

    if (errno == EINVAL || errno == ERANGE) {
        *error = true;
        return result;
    }

    return result;
}

long convertLongSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0L;
    }

    if (temporaryResult < LONG_MIN || temporaryResult > LONG_MAX) {
        *error = true;
        return 0L;
    }

    return (long) temporaryResult;
}

int convertIntSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < INT_MIN || temporaryResult > INT_MAX) {
        *error = true;
        return 0;
    }

    return (int) temporaryResult;
}

short convertShortSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < SHRT_MIN || temporaryResult > SHRT_MAX) {
        *error = true;
        return 0;
    }

    return (short) temporaryResult;
}

unsigned int convertUnsignedIntSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < 0 || temporaryResult > UINT_MAX) {
        *error = true;
        return 0;
    }

    return (unsigned int) temporaryResult;
}

unsigned short convertUnsignedShortSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < 0 || temporaryResult > USHRT_MAX) {
        *error = true;
        return 0;
    }

    return (unsigned short) temporaryResult;
}

int convertPositiveIntegerSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < 0 || temporaryResult > INT_MAX) {
        *error = true;
        return 0;
    }

    return (int) temporaryResult;
}

short convertPostiveShortSafely(const char *timeString, bool *error)
{
    long long temporaryResult = convertNumberSafely(timeString, error);

    if (*error) {
        return 0;
    }

    if (temporaryResult < 0 || temporaryResult > SHRT_MAX) {
        *error = true;
        return 0;
    }

    return (short) temporaryResult;
}
