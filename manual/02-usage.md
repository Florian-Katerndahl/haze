# Usage {#usage}

haze offers two subprograms, one for downloading data from ECMWF's Climate Data Space and one for processing downloaded data. Both steps are coupled via a log-file, which doesn't offer deduplication of download requests at the time of writing. [See the supplied tutorial for step-by-step guide](@ref tutorial).

## Downloading of ERA-5 Data

haze handles the [ERA5 hourly data on single levels from 1940 to present](https://cds.climate.copernicus.eu/datasets/reanalysis-era5-single-levels?tab=overview). This data is delivered as GRIB files and refrenced horizontally to EPSG:4326. Thus, this is the reference CRS used to align data to.

To download data, the user needs an CDS account and follow the instructions to correctly set up the .cdsapirc file [detailed here](https://cds.climate.copernicus.eu/how-to-api). haze looks for this file when the download program is run. However, haze also tries to retrieve the API key from an environment variable named `ADSAUTH` before accessing above named file in the home directory of the user executing the program. If neither options succeeds, the program fails. Also note, that the API endpoint is hard-coded to `https://cds.climate.copernicus.eu/api` and cannot be changed at the moment.

Generally, the user neeeds to supply a file containing an area of interest whose bounding box is calculated before posting a request. This contained geometries do no need to be supplied in EPGS:4326 and are reprojected on the fly, if needed. All geometry types supported by GDAL/OGR are allowed, as long as the input layer's bound geometry can be calculated. You can specify which layer to use via the `--layer` argument, if this is omitted the first layer is used by default. When downloading data globally, it's advised to not use an AOI and set the `--global` flag instead.

Without any further options, the downloaded data is grouped by month, i.e. a single file per month containing all days and hours requested. This can be changed to daily grouping best suited when updating exisitng databases.

All time fields can be given in various formats:

- single values, e.g. `--days 1`
- comma-separated list of values, e.g. `--days 1,2,3`
- range of values with inclusive end points, e.g. `--days 1:3`

An examplanatory program call to download data of global coverage for the 12th of Feburary, April and June for the years 2000 to 2020 between 0 o'clock and 23 o'clock (i.e. for every hour). The data is grouped by day and stored in the directory data-dir.

```bash
haze download --global --daily --year 2000:2020 --month 2,4,6 --day 12 --hour 0:23 --logfile data-dir/logfile data-dir/
```

## Processing of ERA-5 Data

Processing data is based on the supplied log file and subsequent executions do not reprocess data (unless the debug build is used). Compared to data download, there are tighter restrictions on the geometry types usable because of the underlying GEOS implementation to prepare geometries for intersection testing (wkbPolygon, wkbMultiPolygon, wkbMultiSurface, wkbSurface, wkbPolyhedralSurface, wkbTIN, wkbTriangle). Again, input geometries are reprojected to EPSG:4326, if needed. When processing data, an AOI file must be given.

The snipped below would process the data downloaded in the previous step for Europe:

```bash
haze process --logfile data-dir/logfile --layer europe aoi/world.gpkg data-dir/
```

### Debug Build

haze can be compiled with the debug flag set via `make debug`. The debug build does not update the processing status of datasets in the logfile and outputs a vector dataset with intersection geometries in the current working directory from which haze was launched in addition to slightly more verbose output on the command line.
