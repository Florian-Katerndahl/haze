# Tutorial {#tutorial}

This page walks you through an exemplanatory workflow with haze to create a new water vapor database ready to be used with [FORCE](https://force-eo.readthedocs.io/en/latest/). While the containerized version of haze is used here, the same steps apply for the native builds of haze. The choice was made to highlight possible commands that correctly pass credentials and directory paths to a container image.

## Prerequisites

Before the data can be downloaded and processed, you need to setup your access to the CDS API and create some form of area-of-interest geometry that is used for processing and potentially downloading.

### Get Access to the CDS API

-# Create an account or login to your exisitng one on [ECMWF's Climate Data Store site](https://cds.climate.copernicus.eu/)
-# Navigate to your [user profile](https://cds.climate.copernicus.eu/profile)
-# Scroll down to the "API key" section and copy the contents to a file at `~/.cdsapirc`. As detailed in the [usage section](@ref usage), this file could be named and placed arbitrarily or you could rely on environment variables to pass your account's API key.

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

docker run --rm -u $(id -u):$(id -g) -v /home/katerndf/git-repos/haze:/home/ubuntu \
  -e HOME=/home/ubuntu floriankaterndahl/haze:debug-latest \
  -e ADSAUTH="123456789-abc-def-987654321" \
  download --global --year 2023 --month 1,3 --day 1:31 --hour 0:23 \
  --logfile /home/ubuntu/data-dir/wvpdb.log /home/ubuntu/data-dir
```

## Processing Data to Water Vapor Database

We use the WRS-2 footprints downloaded and extracted above as our processing reference. Since the file only contains one layer, we can omit the `--layer` argument and let haze choose the first layer itself.

```bash
docker run --rm -u $(id -u):$(id -g) -v /home/katerndf/git-repos/haze:/home/ubuntu \
  -e HOME=/home/ubuntu floriankaterndahl/haze:debug-latest \
  process --logfile /home/ubuntu/data-dir/wvpdb.log \
  /home/ubuntu/aoi/subset.gpkg /home/ubuntu/data-dir
```

## Using Water Vapor Database with FORCE

The processed files are located in `data-dir`. Looking at the directory structure with tree, we get the following ouput (truncated for readability).

```bash
tree data-dir/*.txt
```

A single file holds four columns: x-coordinate of geometry centroid, y-coordinate of geometry centroid, the averaged water vapor value and a data source tag.

```bash
head data-dir/...txt
```

To minimize friction between FORCE and haze, the newly created txt files can be copied into their separate directory. FORCE can then be pointed to this directory via its configuration, i.e. the `DIR_WVPLUT` option within the level 2 parameter file.
