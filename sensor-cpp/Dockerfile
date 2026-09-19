FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

#Installiamo i compilatori
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpcap-dev \
    libprotobuf-dev \
    protobuf-compiler

WORKDIR /app

#copiamo i sorgenti
COPY sensor-cpp/ /app/
COPY api-contracts/packet_data.proto /app/

#compiliamo Protobuf e il progetto C++
RUN protoc --cpp_out=. packet_data.proto
RUN mkdir -p build && cd build && cmake .. && make

#scarichiamo le immagini
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

#installiamo solo le librerie necessarie per far girare l'eseguibile
RUN apt-get update && apt-get install -y \
    libpcap0.8 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

#copiamo solo l'eseguibile
COPY --from=builder /app/build/sensor /app/sensor

#eseguiamo il sensore
CMD ["./sensor"]