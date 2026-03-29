# Installation {#installtion}

haze can be installed locally and is distributed via Docker Hub.

## Native Compilation

Installing haze locally requires the installation of make as well as the development files of GDAL (at least version 3.9.0), GEOS, cURL and jansson. These can be easily installed via your system's package manager such as `apt`. Please refer to your package manager's documentation if using a system not based on Ubuntu.

```bash
sudo apt install libgdal-dev libcurl4-gnutls-dev libgeos-dev libjansson-dev make
# or
sudo apt install libgdal-dev libcurl4-openssl-dev libgeos-dev libjansson-dev make
```

After cloning the repository and changing into the newly created directory, create a folder named `build`.

```bash
git clone git@github.com:Florian-Katerndahl/haze.git
cd haze
mkdir build
```

Both a release and a debug target exist, which can't be used concurrently as they overwrite the created binary. To re-compile haze using a different target, specify the `-B` flag to re-build the project from scratch.

```bash
make haze
# or
make debug
```

## Installation via Docker

To use haze via Docker, you can pull the corresponding image from Docker Hub. At the time of writing, the published images are not tagged via semantic versioning.

To pull the debug version, run `docker pull floriankaterndahl/haze:debug-latest`. A release version is currently not distributed.

## Installation via Apptainer

To use haze via Apptainer, you can pull the corresponding image from Docker Hub. At the time of writing, the published images are not tagged via semantic versioning.

To pull the debug version, run `apptainer pull docker://floriankaterndahl/haze:debug-latest`. A release version is currently not distributed.
