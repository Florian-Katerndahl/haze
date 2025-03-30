FROM ghcr.io/osgeo/gdal:ubuntu-small-3.10.2

LABEL author="Florian Katerndahl <florian@katerndahl.com>"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y gcc libcurl4-openssl-dev libgeos-dev make pkgconf && \
    apt-get clean

ADD https://github.com/akheron/jansson/releases/download/v2.14/jansson-2.14.tar.gz /home/jansson-2.14.tar.gz

WORKDIR /home/jansson-2.14

RUN tar -xzf /home/jansson-2.14.tar.gz --strip-components=1 -C . && \
    rm /home/jansson-2.14.tar.gz && \
    ./configure && \
    make -j && \
    make install
   
COPY . /home/haze
    
WORKDIR /home/haze

# includes are ordered differently on directly built images,
# remove level of include structure
RUN sed -i -e "s#gdal/##" src/*.c src/*.h main.c && \
    mkdir build/ && \
    make -j main && \
    mv haze /usr/bin/ && \
    rm -r /home/haze /home/jansson-2.14

USER ubuntu

WORKDIR /home/ubuntu

ENTRYPOINT [ "haze" ]

CMD [ "--help" ]
