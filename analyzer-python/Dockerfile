
# STAGE 1: Builder (Compilazione Protobuf)
FROM python:3.11-slim AS builder
WORKDIR /app

# Installiamo il compilatore SOLO nel builder
RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*

# Copiamo il contratto e generiamo il file Python (packet_data_pb2.py)
COPY api-contracts/packet_data.proto .
RUN protoc --python_out=. packet_data.proto

# STAGE 2: Produzione (Runtime Pulito)
FROM python:3.11-slim
WORKDIR /app

# Installiamo solo le dipendenze Python (es. la libreria protobuf)
COPY analyzer-python/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copiamo il codice sorgente
COPY analyzer-python/ .

# Copiamo DAL BUILDER esclusivamente il file Python generato, lasciando indietro il compilatore!
COPY --from=builder /app/packet_data_pb2.py .

CMD ["python", "-u", "main.py"]