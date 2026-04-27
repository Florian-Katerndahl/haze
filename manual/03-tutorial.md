# Tutorial {#tutorial}

This page walks you through an exemplanatory workflow with haze to create a new water vapor database ready to be used with [FORCE](https://force-eo.readthedocs.io/en/latest/). While the containerized version of haze is used here, the same steps apply for the native builds of haze. The choice was made to highlight possible commands that correctly pass credentials and directory paths to a container image.

## Prerequisites

Before the data can be downloaded and processed, you need to setup your access to the CDS API and create some form of area-of-interest geometry that is used for processing and potentially downloading.

### Get Access to the CDS API

-# Create an account or login to your exisitng one on [ECMWF's Climate Data Store site](https://cds.climate.copernicus.eu/)
-# Navigate to your [user profile](https://cds.climate.copernicus.eu/profile)
-# Scroll down to the "API key" section and copy the contents to a file at `~/.cdsapirc`. As detailed in the [usage section](@ref usage), this file could be named and placed arbitrarily or you could rely on environment variables to pass your account's API key.
-# From your user profile, navigate to the "Licenses" section at the top of the page and accept the "CC-BY licence". Without this, you won't be able to access the ERA-5 dataset!

### Geometries for Processing

To compile a water vapor database that can be used to process Landsat imagery, the WRS-2 footprints are downloaded and extracted into an aptly named directory. Note though, that any other geometry will work as well.

```bash
wget https://d9-wret.s3.us-west-2.amazonaws.com/assets/palladium/production/s3fs-public/atoms/files/WRS2_descending_0.zip
unzip -d aoi WRS2_descending_0.zip
```

To cut down on processing time, we restrict our AOI to a small part of eastern Russia that crosses the dateline boundary.

```bash
cd aoi/
ogr2ogr -spat -179 63 -181 68 -nlt MULTIPOLYGON subset.gpkg WRS2_descending.shp WRS2_descending
```

## Downloading Raw Data

> [!note]
> The example below passes the API key via an environment variable instead of the `.cdsapirc` file!

While the concrete water vapor database is only generated for a small subset to illustrate functionality, data is still downloaded with a global footprint, thus no AOI needs to be specified. Instead, we set the `--global` flag to request world wide coverage.

```bash
mkdir data-dir

docker run --rm -u $(id -u):$(id -g) -v $PWD:/home/ubuntu \
  -e ADSAUTH="123456789-abc-def-987654321" \
  floriankaterndahl/haze:0.0.8 \
  download --global --year 2023 --month 1 --day 1:31 --hour 0:23 \
  /home/ubuntu/data-dir/wvpdb.log /home/ubuntu/data-dir
```

The program call above groups the data by month, thus only a single file is downloaded.

```bash
tree data-dir/

# data-dir/
# ├── 2023-01.grib
# └── wvpdb.log
# 
# 0 directories, 2 files
```

## Processing Data to Water Vapor Database

We use the WRS-2 footprints downloaded and extracted above as our processing reference. Since the file only contains one layer, we can omit the `--layer` argument and let haze choose the first layer itself. The tables are stored in a different directory compared to the raw data. Also note the use of the `--footprint` flag which indicates, that the supplied geometries represent footprints (of satellite scenes, in this case). This is important to correcly compute the center coordinate for those geometries, that cross the antimeridian and are split around it.

```bash
mkdir table-dir

docker run --rm -u $(id -u):$(id -g) -v $PWD:/home/ubuntu \
  floriankaterndahl/haze:0.0.8 \
  process --footprint /home/ubuntu/aoi/subset.gpkg \
  /home/ubuntu/data-dir/wvpdb.log /home/ubuntu/table-dir
```

## Using Water Vapor Database with FORCE

The processed files are located in `table-dir`. Looking at the directory structure with tree, we see that one table is saved per day even if the raw data is downloaded as a monthly "composite".

```bash
tree table-dir/

# table-dir/
# ├── WVP_2023-01-01.txt
# ├── WVP_2023-01-02.txt
# ├── WVP_2023-01-03.txt
# ├── WVP_2023-01-04.txt
# ├── WVP_2023-01-05.txt
# ├── WVP_2023-01-06.txt
# ├── WVP_2023-01-07.txt
# ├── WVP_2023-01-08.txt
# ├── WVP_2023-01-09.txt
# ├── WVP_2023-01-10.txt
# ├── WVP_2023-01-11.txt
# ├── WVP_2023-01-12.txt
# ├── WVP_2023-01-13.txt
# ├── WVP_2023-01-14.txt
# ├── WVP_2023-01-15.txt
# ├── WVP_2023-01-16.txt
# ├── WVP_2023-01-17.txt
# ├── WVP_2023-01-18.txt
# ├── WVP_2023-01-19.txt
# ├── WVP_2023-01-20.txt
# ├── WVP_2023-01-21.txt
# ├── WVP_2023-01-22.txt
# ├── WVP_2023-01-23.txt
# ├── WVP_2023-01-24.txt
# ├── WVP_2023-01-25.txt
# ├── WVP_2023-01-26.txt
# ├── WVP_2023-01-27.txt
# ├── WVP_2023-01-28.txt
# ├── WVP_2023-01-29.txt
# ├── WVP_2023-01-30.txt
# └── WVP_2023-01-31.txt

# 0 directories, 31 files
```

A single file holds four columns: x-coordinate of geometry centroid, y-coordinate of geometry centroid, the averaged water vapor value and a data source tag.

```bash
head table-dir/WVP_2023-01-15.txt

# 76.1686 68.2751 0.004772 ERA
# 173.6034 66.9354 0.005535 ERA
# -166.4294 68.2751 0.006537 ERA
# -62.9503 66.9355 0.006347 ERA
# 63.4749 65.5841 0.008463 ERA
# 166.6289 64.2228 0.009889 ERA
# -177.2346 65.5841 0.008855 ERA
# -117.1205 64.2229 0.010667 ERA
# -4.9287 62.8532 0.011991 ERA
# -57.1318 68.2751 0.005682 ERA
```
