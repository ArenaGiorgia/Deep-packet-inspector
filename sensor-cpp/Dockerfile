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
# (Nota: ora usa percorsi specifici perché il Build Context è la cartella principale)
COPY sensor-cpp/ /app/
COPY api-contracts/packet_data.proto /app/

# Rigeneriamo i file Protobuf usando il compilatore interno di Ubuntu
# per garantire la totale compatibilità delle versioni
RUN protoc --cpp_out=. packet_data.proto

# Creiamo la directory di build ed eseguiamo CMake
RUN mkdir -p build && cd build && cmake .. && make

# Comando di default per avviare il sensore C++ compilato
CMD ["./build/sensor"]