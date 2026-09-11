import socket
import time
import sys
import os

# Importiamo la classe Protobuf autogenerata dalla cartella analyzer-python
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "analyzer-python"))
import packet_data_pb2

UDP_IP = "127.0.0.1"
UDP_PORT = 9000

print(f"Avvio Traffic Generator (Protobuf) verso {UDP_IP}:{UDP_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    while True:
        # 1. Creiamo un pacchetto usando il contratto ufficiale Protobuf
        packet = packet_data_pb2.NetworkPacket()
        packet.source_ip = "192.168.1.50"
        packet.dest_ip = "10.0.0.5"
        packet.source_port = 55432
        packet.dest_port = 80
        packet.protocol = "TCP"
        # Inseriamo una finta anomalia (una password in chiaro) nel payload
        packet.raw_payload = b"USER admin PASS admin123"
        packet.timestamp_ms = int(time.time() * 1000)

        # 2. Serializziamo l'oggetto in un flusso binario iper-compresso
        binary_data = packet.SerializeToString()

        sock.sendto(binary_data, (UDP_IP, UDP_PORT))
        print("Pacchetto Protobuf inviato!")
        time.sleep(2)

except KeyboardInterrupt:
    print("\nGenerazione traffico interrotta.")
    sock.close()
