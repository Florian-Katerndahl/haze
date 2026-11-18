FROM ghcr.io/osgeo/gdal:ubuntu-small-3.10.2

LABEL org.opencontainers.image.title="haze"
LABEL org.opencontainers.image.author="Florian Katerndahl <florian@katerndahl.com>"
LABEL org.opencontainers.image.description="haze aims to be a drop-in replacement for water vapor processing of ERA-5 datasets to be used with FORCE."
LABEL org.opencontainers.image.documentation="https://www.katerndahl.net/haze/"
LABEL org.opencontainers.image.citation="Citation information about haze's most recent version can be found at 'https://doi.org/10.5281/zenodo.20666757'; see Zenodo for version-specific DOIs."

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y gcc libcurl4-openssl-dev libgeos-dev libproj-dev libjansson-dev make pkgconf tini && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY . /home/haze
    
WORKDIR /home/haze

# includes are ordered differently on directly built images,
# remove level of include structure
RUN sed -i -e "s#gdal/##" src/*.c src/*.h main.c && \
    mkdir build/ && \
    make -j haze && \
    make install && \
    make clean

USER ubuntu

ENV HOME=/home/ubuntu

WORKDIR /home/ubuntu

RUN chmod go+rx /home/ubuntu

ENTRYPOINT [ "tini", "--", "haze" ]

CMD [ "--help" ]
