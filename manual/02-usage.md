# Usage {#usage}

haze offers two subprograms, one for downloading data from ECMWF's Climate Data Space and one for processing downloaded data. Both steps are coupled via a log-file, which doesn't offer deduplication of download requests at the time of writing. [See the supplied tutorial for step-by-step guide](@ref tutorial).

## Downloading of ERA-5 Data

> [!note]
> When using a custom geometry to specify the bounds of the AOI, less data may be downloaded than anticipated and the footprint of downloaded data may be smaller than the geometry suggests. This is due to how the CDS handles product requests. When in doubt, download a small file to visually check that the entire AOI is covered by water vapor cells.

haze handles the [ERA5 hourly data on single levels from 1940 to present](https://cds.climate.copernicus.eu/datasets/reanalysis-era5-single-levels?tab=overview). This data is delivered as GRIB files and refrenced horizontally to EPSG:4326. Thus, this is the reference CRS used to align data to.

To download data, the user needs an CDS account and follow the instructions to correctly set up the .cdsapirc file [detailed here](https://cds.climate.copernicus.eu/how-to-api). haze looks for this file when the download program is run. However, haze also tries to retrieve the API key from an environment variable named `ADSAUTH` before accessing above named file in the home directory of the user executing the program. If neither options succeeds, the program fails. Also note, that the API endpoint is hard-coded to `https://cds.climate.copernicus.eu/api` and cannot be changed at the moment.

### Subprogram Arguments

The download subprogram takes multiple keyword and positional arguments as detailed int the table below. Keyword arguments can be ordered arbitrarily while positional arguments cannot. A general program execution would take the form below when requesting local data, for global data requests, the `aoi` parameter must be excluded.

```
haze download <optional keyword arguments> <mandatory keyword arguments> [aoi] logfile outdir
```

| Long Argument | Short Argument | Description                                                                                                                         | Mandatory                     |
|---------------|----------------|-------------------------------------------------------------------------------------------------------------------------------------|-------------------------------|
| `--help`        | `-h`             | Print help and exit.                                                                                                                | no                            |
| `--layer`       | `-l`             | Layer to open from AOI dataset.                                                                                                     | no                            |
| `--global`      | `-g`             | Request product worldwide instead of using an AOI dataset.                                                                          | no                            |
| `--daily`       | `-d`             | Group product requests by day instead of month.                                                                                     | no                            |
| `--year`        |                | Years for which data should be downloaded.                                                                                          | yes                           |
| `--month`       |                | Months for which data should be downloaded.                                                                                         | yes                           |
| `--day`         |                | Days for which data should be downloaded.                                                                                           | yes                           |
| `--hour`        |                | Hours for which data should be downloaded (zero-based).                                                                             | yes                           |
| `aoi`           |                | File path to OGR-readble file containing one or more polygons for which to extract data. Either `layer` or the first layer is read. | if `--global` flag is not set |
| `logfile`       |                | Path to logfile storing successful downloads and processing. statuses                                                                | yes                           |
| `outdir`        |                | Directory into which output data products and CSVs are written.                                                                                                 | yes                           |

Generally, the user neeeds to supply a file containing an area of interest whose bounding box is calculated before posting a request. This contained geometries do no need to be supplied in EPGS:4326 and are reprojected on the fly, if needed. All geometry types supported by GDAL/OGR are allowed, as long as the input layer's bound geometry can be calculated. You can specify which layer to use via the `--layer` argument, if this is omitted the first layer is used by default. When downloading data globally, it's advised to not use an AOI and set the `--global` flag instead.

Without any further options, the downloaded data is grouped by month, i.e. a single file per month containing all days and hours requested. This can be changed to daily grouping best suited when updating exisitng databases.

All time fields can be given in various formats:

- single values, e.g. `--day 1`
- comma-separated list of values, e.g. `--day 1,2,3`
- range of values with inclusive end points, e.g. `--day 1:3`

An examplanatory program call to download data of global coverage for the 12th of Feburary, April and June for the years 2000 to 2020 between 0 o'clock and 23 o'clock (i.e. for every hour). The data is grouped by day and stored in the directory data-dir.

```bash
haze download --global --daily --year 2000:2020 --month 2,4,6 --day 12 --hour 0:23 data-dir/logfile data-dir/
```

## Processing of ERA-5 Data

> [!warning]
> It's a known issue that multipolygons bound to +/- 180° (i.e. split on the date line) do not intersect correctly(?) when using rasters as input that extend below or above +/- 180°!
> Example: Given the WRS-2 tile 093012, which is split at the date line in the USGS supplied WRS-2 dataset, and a raster with the extent 166.000,-50.000 : 205.000,73.000 ((xmin, ymin):(xmax, ymax)) one may expect the *western* parts of WRS-2 tile 093012 to intersect with the *eastern* part of the supplied raster because of the cyclicity of coordinates. **This, however, is currently not implemented as it's not clear how to handle this corner case correctly.** Instead, either the geometries would need to be merged or a raster dataset covering the range -180° to 180° must be supplied.

> [!note]
> Input formats GML and GMLAS are not allowed.

### Subprogram Arguments

The process subprogram takes multiple keyword and positional arguments as detailed int the table below. Keyword arguments can be ordered arbitrarily while positional arguments cannot. A general program execution would take the form below.

```
haze process <optional keyword arguments> <mandatory keyword arguments> aoi logfile outdir
```

| Long Argument | Short Argument | Description                                                                                                                         | Mandatory |
|---------------|----------------|-------------------------------------------------------------------------------------------------------------------------------------|-----------|
| `--help`        | `-h`             | Print help and exit.                                                                                                                | no        |
| `--layer`       | `-l`             | Layer to open from AOI dataset.                                                                                                     | no        |
| `--logfile`     |                | Path to logfile storing successful downloads and processing statuses                                                                | yes       |
| `--footprint`   |                | If specified, multipolygons are considered footprint geometries and those cut at the dateline are merged to a polygon to compute centroid.                                          | no        |
| `aoi`           |                | File path to OGR-readble file containing one or more polygons for which to extract data. Either `layer` or the first layer is read. | yes       |
| `logfile`       |                | Path to logfile storing successful downloads and processing. statuses                                                                | yes                           |
| `outdir`        |                | Directory to which files are saved.                                                                                                 | yes       |

Processing data is based on the supplied log file and subsequent executions do not reprocess data (unless the debug build is used). Compared to data download, there are tighter restrictions on the geometry types usable, only wkbPolygon and wkbMultiPolygon are allowed. Again, input geometries are reprojected to EPSG:4326, if needed. When processing data, an AOI file must be given.

The snipped below would process the data downloaded in the previous step for Europe:

```bash
haze process --layer europe aoi/world.gpkg data-dir/logfile data-dir/
```

### Debug Build

haze can be compiled with the debug flag set via `make debug`. The debug build does not update the processing status of datasets in the logfile and outputs a vector dataset with intersection geometries in the current working directory from which haze was launched in addition to slightly more verbose output on the command line.

## Docker Specifics

The user within the Docker image is most likely different from your account on the host machine. Thus, to correctly map the user and group ID and avoid any access restrictions you need to specifcy the `--user` or `-u` flag as follows:

```bash
docker run -u $(id -u):$(id -g) floriankaterndahl/haze:latest
```

Since haze expects the `.cdsapirc` file to be in the home directory, you also need to map the directory on your host machine to the docker container. While you can map the `.cdsapirc` file directly, this disregards the mapping between the working directory within the container to your host machine and requires another mount to be specified!

```bash
docker run -u $(id -u):$(id -g) -v $HOME:/home/ubuntu floriankaterndahl/haze:latest
# or
docker run -u $(id -u):$(id -g) -v $HOME/.cdsapirc:/home/ubuntu/.cdsapirc floriankaterndahl/haze:latest
```

It's also possible to choose a different directory path inside the container, e.g. set it equal to your home directory on the host machine. Please note, that you need to update both the working directory (`-w`) and the `$HOME` environment variable (`-e`) in this case as well.

```bash
docker run -u $(id -u):$(id -g) -v $HOME:$HOME -w $HOME -e HOME=$HOME floriankaterndahl/haze:latest
```