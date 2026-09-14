FROM python:3.11-slim
WORKDIR /app

# Installiamo il compilatore protobuf nel container Python
RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*

# Copia e installa le dipendenze Python
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copiamo il codice dell'analizzatore e il file proto comune
COPY analyzer-python/ /app/
COPY api-contracts/packet_data.proto /app/

# Rigeneriamo lo stub Python internamente per evitare conflitti di versione
RUN protoc --python_out=. packet_data.proto

CMD ["python", "main.py"]