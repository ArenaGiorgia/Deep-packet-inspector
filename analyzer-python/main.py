import socket
import packet_data_pb2
from inspector import NetworkInspector


def avvia_server():
    UDP_IP = "127.0.0.1"
    UDP_PORT = 9001  # L'analizzatore ascolta sulla porta 9001

    # Creazione del socket UDP per ricevere i dati
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    # Istanziamo l'oggetto ispettore
    ispettore = NetworkInspector()

    print(f"Analizzatore Forense (Livello 7) in ascolto su {UDP_IP}:{UDP_PORT}...")

    try:
        while True:
            # 1. Il server si mette in attesa di ricevere byte dal router Go
            buffer, addr = sock.recvfrom(2048)

            # 2. Ricostruisce l'oggetto Protobuf a partire dai byte grezzi
            packet = packet_data_pb2.NetworkPacket()
            packet.ParseFromString(buffer)

            # 3. Passa i dati spacchettati all'ispettore per l'analisi
            minaccia_trovata, messaggio = ispettore.analyze_packet(
                packet.protocol, packet.raw_payload
            )

            # 4. Se la funzione restituisce True, stampa l'allarme
            if minaccia_trovata:
                print(f"[{ispettore.alert_count}] {messaggio}")

    except KeyboardInterrupt:
        # Cattura l'uscita manuale tramite CTRL+C
        print("\nSpegnimento analizzatore richiesto dall'utente.")

    finally:
        # Il blocco finally viene eseguito sempre e in ogni circostanza
        # È il luogo ideale per il cleanup e per rilasciare le risorse di rete[cite: 2]
        print("Chiusura sicura del socket completata.")
        sock.close()


# Questo blocco previene l'esecuzione accidentale se il file viene importato altrove[cite: 2]
if __name__ == "__main__":
    avvia_server()
