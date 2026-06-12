# Overview {#mainpage}

[![DOI](https://zenodo.org/badge/942918936.svg)](https://doi.org/10.5281/zenodo.20666757)

> [!note]
> If you come across issues or want to suggest improvements, please [report them on Github!](https://github.com/Florian-Katerndahl/haze/issues)

haze aims to be a drop-in replacement for water vapor processing of ERA-5 datasets to be used with FORCE. haze offers capabilities to download and process those datasets in different steps by accepting arbitrary vector geometries (to a certain degree) and calculating the area-weighted mean of intersecting raster cells. Thus, the feature set is broader compared to the original FORCE implementation. This is expected to allow greater flexibility and easier usage.

This online documentation contains more detailed information about haze's installation, usage and documents all functions/routines. See the sidebar for the corresponding links. The actual implementation is grouped by *topics*.

## Quickstart

haze can be either installed locally by first installing all necessary dependencies or using a container runtime such as Docker or Singularity. See [the installation page for more details](#installation). For more detailed descriptions of the program arguments, a step-by-step tutorial on how to use haze and some thoughts about parallel processing with haze, see the respective pages on the left side bar!

haze offers two subprograms for downloading and processing water vapor data. Downloading data can be done either with an global extent or restricted by a bounding box which is computed on the fly by giving a vector dataset and possibly the name of the layer to use. In case the geometries are not in EPSG:4326, the data is reprojected but the original file is left untouched. haze keeps track of downloaded (and processed) files in a logfile which allows to build water vapor databases incrementally without re-processing data needlessly.

```bash
haze download --global --daily --year 2023 --month 1:12 --day 1,2,3,4,5,6 --hour 11,12 wvpdb/logfile wvpdb/raw
```

Processing data downloaded previously requires the usage of AOI geometries in order to compute area-weighted mean values. When the input geometries represent footprints of satellite scenes, e.g. WRS-2 tiles, it may be necessary to specify the flag `--wrap-on-edge` to correctly compute centroid coordinates. Also note, that input geometries and the underlying raster dataset must overlay completely, otherwise the computation of water vapor values is erroneous!

```bash
haze process --wrap-on-edge wvpdb/aoi/WRS2_descending.shp wvpdb/logfile wvpdb/tables
```

## Citation

**Please refrain from using the CFF file directly or the citation export offered by GitHub!** Instead, use the DOI from the related publication on Zenodo, linking to a specific version tag that was used in your work.

## Acknowledgement

haze was funded in part by the Deutsche Forschungsgemeinschaft (DFG, German Research Foundation) – Project-ID 414984028 – SFB 1404 FONDA. 

Additionally, haze heavily relies on the libraries listed below, without which it could not exist in this form. Consider supporting them, if you can!

- [cURL](https://curl.se/)
- [GDAL/OGR](https://gdal.org/)
- [PROJ](https://proj.org/en/stable/)
- [GEOS computational geometry library](https://libgeos.org/)
- [Jansson](https://github.com/akheron/jansson)
