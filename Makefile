# Makefile per automatizzare la generazione del codice Protobuf

PROTO_DIR = api-contracts
PROTO_FILE = $(PROTO_DIR)/packet_data.proto

.PHONY: all proto clean

all: proto

proto:
	@echo "Generazione codice Protobuf per C++ (Sensore)..."
	protoc -I=$(PROTO_DIR) --cpp_out=./sensor-cpp $(PROTO_FILE)
	
	@echo "Generazione codice Protobuf per Python (Analista)..."
	protoc -I=$(PROTO_DIR) --python_out=./analyzer-python $(PROTO_FILE)
	
	@echo "Generazione codice Protobuf per Go (Router)..."
	protoc -I=$(PROTO_DIR) --go_out=./router-go --go_opt=paths=source_relative $(PROTO_FILE)
	
	@echo "Compilazione Protobuf completata con successo in tutti e 3 i linguaggi! 🚀"

clean:
	@echo "Pulizia dei file autogenerati..."
	rm -f ./sensor-cpp/*.pb.cc ./sensor-cpp/*.pb.h
	rm -f ./analyzer-python/*_pb2.py
	rm -f ./router-go/*.pb.go