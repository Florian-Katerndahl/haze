#include "aoi.h"
#include "fscheck.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gdal/gdal.h>
#include <gdal/cpl_conv.h>
#include <gdal/cpl_error.h>
#include <gdal/ogr_core.h>
#include <gdal/ogr_api.h>
#include <gdal/ogr_srs_api.h>
#include <time.h>

struct boundingBox *allocBoundingBox(void) {
    struct boundingBox *new = calloc(1, sizeof(struct boundingBox));
    return new;
}

void freeBoundingBox(struct boundingBox *box) {
    free(box);
    box = NULL;
}

struct boundingBox *boxFromPath(const char *filePath, const char *layerName) {
    // bigger TODO: each step could be packed into its own function which would make this one more readable
    struct boundingBox *box;

    assert(fileReadable(filePath));

    if ((box = allocBoundingBox()) == NULL) {
        perror("calloc");
        return NULL;
    }

    GDALAllRegister();
    GDALDatasetH *aoi = GDALOpenEx(filePath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    if (aoi == NULL) {
        fprintf(stderr, "Failed to open dataset %s\n", filePath);
        exit(1);
    }

    if (GDALDatasetGetLayerCount(aoi) == 0) {
        fprintf(stderr, "Provided vector file does not conain layers\n");
        CPLErr err = GDALClose(aoi);
        if (err != CE_None) {
            fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
            exit(1);
        }
        exit(1);
    }

    OGRLayerH *layer;
    if (layerName == NULL) {
        layer = GDALDatasetGetLayer(aoi, 0);
    } else {
        layer = GDALDatasetGetLayerByName(aoi, layerName);
    }
    if (layer == NULL) {
        fprintf(stderr, "Failed to get layer from dataset\n");
        CPLErr err = GDALClose(aoi);
        if (err != CE_None) {
            fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
            exit(1);
        }
        exit(1);
    }

    OGRSpatialReferenceH *layerRef = OGR_L_GetSpatialRef(layer);
    if (layerRef == NULL) {
        fprintf(stderr, "Spatial reference not available\n");
        CPLErr closeErr = GDALClose(aoi);
        if (closeErr != CE_None) {
            fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
            exit(1);
        }
        exit(1);
    }

    OGREnvelope *mbr = CPLCalloc(1, sizeof(OGREnvelope));
    OGRErr extErr = OGR_L_GetExtent(layer, mbr, 1); // force calculation of layer extent, regardless of cost
    if (extErr == OGRERR_FAILURE) {
        fprintf(stderr, "Failed to get layer extent: %s", CPLGetLastErrorMsg());
        CPLErr closeErr = GDALClose(aoi);
        if (closeErr != CE_None) {
            fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
            exit(1);
        }
    }

    char *layerWKT;
    OGRErr wktErr = OSRExportToWktEx(layerRef, &layerWKT, NULL);
    if (wktErr != OGRERR_NONE) {
        fprintf(stderr, "Failed to export layer WKT\n");
        //TODO cleanup
        exit(1);
    }

    if (EQUAL(layerWKT, SRS_WKT_WGS84_LAT_LONG)) {
        box->left   = mbr->MinX;
        box->top    = mbr->MaxY;
        box->right  = mbr->MaxX;
        box->bottom = mbr->MinY;
    } else {
        // FIXME: handle crossing of meridians
        const char *wgs84WKT = SRS_WKT_WGS84_LAT_LONG;
        OGRSpatialReferenceH *wgs84Ref = OSRNewSpatialReference(wgs84WKT);
        if (wgs84Ref == NULL) {
            fprintf(stderr, "Failed to create spatial reference object for WGS84 WKT: %s", CPLGetLastErrorMsg());
            // TODO cleanup
            exit(1);
        }
        OGRCoordinateTransformationH *transformation = OCTNewCoordinateTransformationEx(layerRef, wgs84Ref, NULL);
        if (transformation == NULL) {
            fprintf(stderr, "Failed to create transformation object: %s", CPLGetLastErrorMsg());
            // TODO cleanup
            exit(1);
        }
        if (OCTTransformBounds(
                    transformation,
                    mbr->MinX, mbr->MinY,
                    mbr->MaxX, mbr->MaxY,
                    &(box->left), &(box->bottom),
                    &(box->right), &(box->top),
                    21) == FALSE) {
            fprintf(stderr, "Failed to transform bounding box\n");
            // todo cleanup
            exit(1);
        }

        OSRDestroySpatialReference(wgs84Ref);
        OCTDestroyCoordinateTransformation(transformation);
    }

    CPLFree(layerWKT);

    CPLFree(mbr);

    CPLErr closeErr = GDALClose(aoi);
    if (closeErr != CE_None) {
        fprintf(stderr, "%s\n", CPLGetLastErrorMsg());
        exit(1);
    }

    return box;
}

void printBoundingBox(const struct boundingBox *box) {
    printf(
        "bounding box: %lf (top), %lf (right), %lf (bottom), %lf (left)\n",
        box->top, box->right, box->bottom, box->left
    );
    return;
}
