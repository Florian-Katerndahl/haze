#include "src/aoi.h"
#include "src/api.h"
#include "src/options.h"
#include "src/types.h"
#include "src/haze.h"
#include "src/geos-ops.h"
#include <stdio.h>
#include <stddef.h>
#include <geos_c.h>
#include <gdal/gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/ogr_core.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int exitCode = EXIT_SUCCESS;
    /* SETUP EXTERNAL LIBRARIES */
    initGEOS(geosMessagesToStderr, geosMessagesToStderr);
    GDALAllRegister();
    curl_global_init(CURL_GLOBAL_ALL);

    // see note for transformationFromWKTs and docstring for openVectorDataset
    GDALDriverH gml = GDALGetDriverByName("GML");
    GDALDriverH gmlas = GDALGetDriverByName("GMLAS");
    GDALDeregisterDriver(gml);
    GDALDeregisterDriver(gmlas);
    GDALDestroyDriver(gml);
    GDALDestroyDriver(gmlas);

    /* START OF PROGRAM */
    const OGREnvelope *aoi = NULL;

    option_t *opts = parseOptions(argc, argv);

    if (opts == NULL || opts->printHelp) {
        printHelp();
        exitCode = (opts == NULL);
        goto teardown;
    }

#ifdef DEBUG
    printOptions(opts);
#endif

    if (!opts->global && opts->download) {
        aoi = boxFromPath(opts->areaOfInterest, opts->aoiName);
        if (aoi == NULL) {
            exitCode = EXIT_FAILURE;
            goto teardown;
        }
    }

    if (opts->download) {
        if (download(opts, aoi)) {
            fprintf(stderr, "Error downloading files\n");
            exitCode = EXIT_FAILURE;
            goto teardown;
        }
    } else if (opts->process) {
        if (process(opts) == 1) {
            fprintf(stderr, "Failed to process all datasets\n");
            exitCode = EXIT_FAILURE;
            goto teardown;
        }
    }

teardown:
    /* END OF PROGRAM, FREE STACK OBJECTS */
    CPLFree((void *) aoi);
    freeOption(opts);

    /* TEARDOWN EXTERNAL LIBRARIES */
    GDALDestroy();
    finishGEOS();
    curl_global_cleanup();

    return exitCode;
}
