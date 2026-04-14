FROM ghcr.io/osgeo/gdal:ubuntu-small-3.10.2

LABEL author="Florian Katerndahl <florian@katerndahl.com>"
LABEL description="haze aims to be a drop-in replacement for water vapor processing of ERA-5 datasets to be used with FORCE."

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y gcc libcurl4-openssl-dev libgeos-dev libjansson-dev make pkgconf && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY . /home/haze
    
WORKDIR /home/haze

# includes are ordered differently on directly built images,
# remove level of include structure
RUN sed -i -e "s#gdal/##" src/*.c src/*.h main.c && \
    mkdir build/ && \
    make -j debug && \
    mv haze /usr/bin/ && \
    make clean

USER ubuntu

ENV HOME=/home/ubuntu

WORKDIR /home/ubuntu

ENTRYPOINT [ "haze" ]

CMD [ "--help" ]
