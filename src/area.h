#ifndef AREA_H
#define AREA_H

/**
 * @file area.h
 * @author Florian Katerndahl <florian@katerndahl.com>
 * @brief This header file describes function signatures for fast geodesic area calculation.
 * @version 0.1
 * @date 2026-04-29
 *
 * @copyright Copyright (c) 2026
 *
 * @defgroup fast-geodarea Fast Geodesic Area Calculation
 * @{
 */

#include <gdal/ogr_api.h>
#include <geodesic.h>

/**
 * @brief Fast Computation of Geodesic Area for Linear Rings
 *
 * @details Perform geodesic area calculation in case `geometry` is a
 *          linear ring. This geometry type is not a standard WKB/WKT type
 *          but represents the individual exterior/interior rings that
 *          comprise polygons, multipolygons, etc.
 *          The same restrictions as for fastGeodesicArea(),
 *          fastMultipolygonialGeodesicArea() and fastPolygonialGeodesicArea()
 *          apply, i.e. no checks for the validity and/or adequacy of
 *          the geometrie's CRS is performed. However, in contrast to those
 *          functions, the notion of a "valid geometry" does not hold and
 *          the caller is responsible for making sure the input is sensible, e.g.
 *          extracted from an already exisitng polygon.
 *
 * @warning This function will not warn about incorrect CRS's. The only cases
 *          in which projection errors may be caught are: latitudinal
 *          values exceed +/- 90°.
 *
 * @ref fastGeodesicArea(), fastMultipolygonialGeodesicArea(), fastPolygonialGeodesicArea()
 *
 * @param geometry Reference to linear ring geometry.
 * @param g Reference to an initialized `struct geod_geodesic`. Needed by PROJ for geodesic
 *        operations.
 * @return double Area of `geometry`, -1.0 on error.
 */
double fastLinearRingGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g);

/**
 * @brief Fast Computation of Geodesic Area for Polygons
 *
 * @details Perform geodesic area calculation in case `geometry` is a
 *          polygon. The same restrictions as for fastGeodesicArea(),
 *          fastMultipolygonialGeodesicArea() and fastLinearRingGeodesicArea()
 *          apply, i.e. no checks for the validity and/or adequacy of
 *          the geometrie's CRS is performed.
 *
 * @warning This function will not warn about incorrect CRS's. The only cases
 *          in which projection errors may be caught are: latitudinal
 *          values exceed +/- 90°.
 *
 * @ref fastGeodesicArea(), fastMultipolygonialGeodesicArea(), fastLinearRingGeodesicArea()
 *
 * @param geometry Reference to a valid polygonal geometry.
 * @param g Reference to an initialized `struct geod_geodesic`. Needed by PROJ for geodesic
 *        operations.
 * @return double Area of `geometry`, -1.0 on error.
 */
double fastPolygonialGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g);

/**
 * @brief Fast Computation of Geodesic Area for Multipolygons
 *
 * @details Perform geodesic area calculation in case `geometry` is a multi-
 *          polygon. The same restrictions as for fastGeodesicArea(), fastPolygonialGeodesicArea()
 *          and fastLinearRingGeodesicArea() apply, i.e. no checks for the validity and/or
 *          adequacy of the geometrie's CRS is performed.
 *
 * @warning This function will not warn about incorrect CRS's. The only cases
 *          in which projection errors may be caught are: latitudinal
 *          values exceed +/- 90°.
 *
 * @ref fastGeodesicArea(), fastPolygonialGeodesicArea(), fastLinearRingGeodesicArea()
 *
 * @param geometry Reference to a valid multipolygonal geometry.
 * @param g Reference to an initialized `struct geod_geodesic`. Needed by PROJ for geodesic
 *        operations.
 * @return double Area of `geometry`, -1.0 on error.
 */
double fastMultipolygonialGeodesicArea(const OGRGeometryH geometry, const struct geod_geodesic *g);

/**
 * @brief Fast Computation of Geodesic Area for Polygons and Multipolygons
 *
 * @details This function implements geodesic area calculation for polygons
 *          and multipolygons. In contrast to GDAL's/OGR's methods, this not
 *          not only supports fewer geometry types but also assumes that input
 *          geometries are in a CRS suitable for geodesic calculations. GDAL/OGR
 *          performs reprojection on the fly which is the main source of runtime
 *          overhead.
 *          Both implementations rely on PROJ's `geod_polygonarea`.
 *
 * @note This function takes direct insipiration from GDAL's/OGR's
 *       `OGR_G_GeodesicArea` and the implementation specifics of the
 *       respective C++ methods -- one could call this a copy-cat version
 *       to a certain extent. Equality of results was tested against a
 *       complete WRS-2 dataset and the entirety of the GADM v4.1 database.
 *
 * @warning This function will not warn about incorrect CRS's. The only cases
 *          in which projection errors may be caught are: a previous reprojection
 *          resulted in invalid geometries, the supplied spatial reference object
 *          does not have it's semi-major or inverse-flattening set, latitudinal
 *          values exceed +/- 90°.
 *
 * @ref fastMultipolygonialGeodesicArea(), fastPolygonialGeodesicArea(), fastLinearRingGeodesicArea()
 *
 * @param geometry Reference to a valid geometry object whose area should be calculated.
 *        The object is not required to have a CRS assigned to it.
 * @param spatialReference Reference to spatial reference object which describes
 *        the CRS of `geometry`.
 * @return double Area of `geometry`, -1.0 on error.
 */
double fastGeodesicArea(const OGRGeometryH geometry, const OGRSpatialReferenceH spatialReference);

/** @} */ // end of group
#endif // AREA_H