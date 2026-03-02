FROM ghcr.io/osgeo/gdal:ubuntu-small-3.10.2

LABEL author="Florian Katerndahl <florian@katerndahl.com>"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y gcc libcurl4-openssl-dev libgeos-dev libjansson-dev make pkgconf && \
    apt-get clean

COPY . /home/haze
    
WORKDIR /home/haze

# includes are ordered differently on directly built images,
# remove level of include structure
RUN sed -i -e "s#gdal/##" src/*.c src/*.h main.c && \
    mkdir build/ && \
    make -j haze && \
    mv haze /usr/bin/

USER ubuntu

WORKDIR /home/ubuntu

ENTRYPOINT [ "haze" ]

CMD [ "--help" ]
