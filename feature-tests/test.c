#include "../src/strtree.h"
#include "../src/haze.h"
#include <gdal/cpl_conv.h>
#include <gdal/cpl_port.h>
#include <gdal/cpl_string.h>
#include <gdal/cpl_error.h>
#include <gdal/gdal.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <stdio.h>
#include <time.h>
#include <geos_c.h>

static void
geos_msg_handler(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf (fmt, ap);
    va_end(ap);
}

int main(void) {
    initGEOS(NULL, NULL);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALAllRegister();

    GDALDatasetH *ds = GDALOpenEx("../data/example.grib", GDAL_OF_RASTER | GDAL_OF_READONLY, NULL, NULL, NULL);
    const char *rasterWkt = extractCRSAsWKT(ds, NULL);
    
    vectorGeometryList *areasOfInterest = NULL;
    buildGEOSGeometriesFromFile("../data/grid.kml", NULL, &areasOfInterest, rasterWkt);

    struct rawData *data = NULL;
    readRasterDataset(ds, &data);
    struct geoTransform transform = {0};
    getRasterMetadata(ds, &transform);
    closeGDALDataset(ds);

    struct averagedData *average = NULL;
    averageRawDataWithSizeOffset(data, &average, 0, 0);

    cellGeometryList *rasterCellsAsGEOS = NULL; // todo: document that this should be freed!

    // todo: optionally implement function to crop raster beforehand

    GEOSSTRtree *rasterTree = buildSTRTreefromRaster(average, &transform, &rasterCellsAsGEOS);

    // a function to query the tree constructed by buildSTRTreefromRaster which somehow gets me for each polygon in areasOfInterest
    // the intersecting polygons of the tree so I can calculate the area-weighted average
    intersection_t *intersections = querySTRTree(areasOfInterest, rasterTree); // todo: free intersections!
    if (intersections == NULL) {
        fprintf(stderr, "No intersections found\n");
        // todo: cleanup
        return 0;
    }

    // a functions that (should be split up into smaller pieces)
    // 1. converts GEOSGeometry back to OGRGeometry (via WKBExport)
    // 2. given two OGRGeometries (Polygons) computes the intersection
    // 3. a) depending on the CRS being geodesic or not, calculating the appropriate area
    // 3. b) query a WKT/dataset for property
    // 4. calculate area-weighted average
    // 5. get centroid of polygon
    mean_t *weightedMeans = calculateAreaWeightedMean(intersections, rasterWkt); // todo: free this!
    if (weightedMeans == NULL) {
     fprintf(stderr, "Failed to calculate weighted means\n");
     // todo: cleanup
     return 1;
    }

    // 6. write tuple (centroid coordinates, average value, ERA5) to a file
    writeWeightedMeans(weightedMeans, "file.csv");
    
    freeVectorGeometryList(areasOfInterest);
    freeCellGeometryList(rasterCellsAsGEOS);
    freeIntersections(intersections);
    freeWeightedMeans(weightedMeans);
    

    GEOSSTRtree_destroy(rasterTree);

    CPLFree((void* ) rasterWkt);
    freeAverageData(average);
    freeRawData(data);
    finishGEOS();

    return 0;
}