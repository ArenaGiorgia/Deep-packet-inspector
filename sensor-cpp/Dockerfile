# Usiamo un'immagine ufficiale di Ubuntu come base pulita
FROM ubuntu:22.04

# Evitiamo blocchi interattivi durante l'installazione dei pacchetti
ENV DEBIAN_FRONTEND=noninteractive

# 1. Installiamo gli strumenti di compilazione, CMake, la libreria libpcap e Protobuf
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpcap-dev \
    libprotobuf-dev \
    protobuf-compiler \
    git \
    && rm -rf /var/lib/apt/lists/*

# Creiamo la cartella di lavoro all'interno del container
WORKDIR /app

# Copiamo tutto il codice sorgente del sensore C++ dentro il container
COPY . /app

# Creiamo la directory di build ed eseguiamo CMake
RUN mkdir -p build && cd build && cmake .. && make

# Comando di default per avviare il sensore C++ compilato
CMD ["./build/sensor"]