FROM python:3.11-slim
WORKDIR /app

# Installa il compilatore Protobuf nel container
RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*

# MODIFICA: Ora specifichiamo la cartella analyzer-python/
COPY analyzer-python/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copiamo il contratto e il codice sorgente
COPY api-contracts/packet_data.proto /app/
COPY analyzer-python/ /app/

# Rigeneriamo lo stub pulito
RUN protoc --python_out=. packet_data.proto


CMD ["python", "-u", "main.py"]