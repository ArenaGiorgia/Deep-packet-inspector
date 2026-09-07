import socket
import time

UDP_IP = "127.0.0.1"
UDP_PORT = 9000

print(f"Avvio Traffic Generator verso {UDP_IP}:{UDP_PORT}")
# Creiamo un socket UDP
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    while True:
        # Inviamo un flusso di byte grezzi (più avanti useremo Protobuf)
        fake_payload = b"FAKE_TCP_PACKET_WITH_DATA"
        sock.sendto(fake_payload, (UDP_IP, UDP_PORT))
        print("Pacchetto sintetico inviato!")
        time.sleep(1.5)  # Invia un pacchetto ogni secondo e mezzo
except KeyboardInterrupt:
    print("\nGenerazione traffico interrotta.")
    sock.close()
