# ==========================================
# FASE 1: BUILDER (Ambiente di compilazione)
# ==========================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Installiamo i compilatori (rimosso git che era superfluo)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpcap-dev \
    libprotobuf-dev \
    protobuf-compiler

WORKDIR /app

# Copiamo i sorgenti
COPY sensor-cpp/ /app/
COPY api-contracts/packet_data.proto /app/

# Compiliamo Protobuf e il progetto C++
RUN protoc --cpp_out=. packet_data.proto
RUN mkdir -p build && cd build && cmake .. && make

# ==========================================
# FASE 2: RUNTIME (Immagine di produzione leggera)
# ==========================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Installiamo SOLO le librerie necessarie per far girare l'eseguibile, NIENTE compilatori!
RUN apt-get update && apt-get install -y \
    libpcap0.8 \
    libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiamo SOLO l'eseguibile generato dalla Fase 1
COPY --from=builder /app/build/sensor /app/sensor

# Eseguiamo il sensore
CMD ["./sensor"]