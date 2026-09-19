# STAGE 1: Builder (Compilazione Protobuf)
FROM python:3.11-slim AS builder
WORKDIR /app

# Installiamo il compilatore Protobuf
RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*

# Il context è la radice, quindi peschiamo direttamente dalla cartella api-contracts
COPY api-contracts/packet_data.proto .
RUN protoc --python_out=. packet_data.proto

# STAGE 2: Produzione
FROM python:3.11-slim
WORKDIR /app

# Peschiamo i requirements dalla cartella specifica
COPY analyzer-python/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copiamo SOLO i file sorgente Python (non tutto il progetto)
COPY analyzer-python/ .

# Ripeschiamo lo stub generato dal builder
COPY --from=builder /app/packet_data_pb2.py .

CMD ["python", "-u", "main.py"]