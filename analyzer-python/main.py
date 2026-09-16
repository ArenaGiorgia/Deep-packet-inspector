import socket
from google.protobuf.message import DecodeError  # Importiamo l'eccezione specifica
import packet_data_pb2
import logging
from inspector import DpiAnalyzer
from timing import PerformanceMonitor
from session_reassembly import TcpReassembler


def avvia_server():
    UDP_IP = "0.0.0.0"  # L'analizzatore ascolta su tutte le interfacce di rete
    UDP_PORT = 9001  # L'analizzatore ascolta sulla porta 9001
    BUFFER_SIZE = 4096  # Standard industriale (potenza di 2) per i buffer di rete

    # Setup di rete: Creazione del socket UDP in ingresso
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    # Canale di Ritorno per gli Allarmi verso Go
    alert_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    router_address = ("router", 9002)  # Scegliamo la porta 9002 per gli allarmi

    # [DEPENDENCY INJECTION]: Inizializziamo il motore di analisi (Livello 7) e i nuovi moduli
    ispettore = DpiAnalyzer()
    monitor = PerformanceMonitor()
    reassembler = TcpReassembler()
    # Configurazione base del salvataggio log su file
    logging.basicConfig(
        filename="allarmi_dpi.log",
        level=logging.WARNING,
        format="%(asctime)s | %(message)s",
    )
    print(f"Analizzatore Forense (Livello 7) in ascolto su {UDP_IP}:{UDP_PORT}...")

    try:
        # Acceptor Loop: ciclo infinito asincrono di elaborazione
        while True:
            # 1. Il server si mette in attesa bloccante di datagrammi UDP dal router Go
            buffer, addr = sock.recvfrom(BUFFER_SIZE)

            packet = packet_data_pb2.NetworkPacket()

            # ==========================================
            # 2. PARADIGMA EAFP (Easier to Ask for Forgiveness than Permission)
            # ==========================================
            try:
                packet.ParseFromString(buffer)
            except DecodeError:
                print(
                    f"[SECURITY WARNING] Ricevuto payload malformato da {addr}. Pacchetto scartato."
                )
                continue

            # ==========================================
            # TIMING (Telemetria End-to-End)
            # ==========================================
            latenza = monitor.record_latency(packet.timestamp_ms)

            # ==========================================
            # SESSION REASSEMBLY (Gestione TCP)
            # ==========================================
            session_id = f"{packet.source_ip}:{packet.source_port}-{packet.dest_ip}:{packet.dest_port}"
            payload_assemblato = reassembler.add_segment(session_id, packet.raw_payload)

            # 3. Deleghiamo l'ispezione (Pattern Matching) del payload all'ispettore
            minaccia_trovata, messaggio = ispettore.analyze_payload(
                packet.protocol, payload_assemblato
            )

            # 4. Se la funzione restituisce True, stampiamo l'allarme e avvisiamo Go!
            if minaccia_trovata:
                # Creiamo la stringa formattando la latenza a 2 decimali per un output pulito
                testo_allarme = f"🚨 MINACCIA DA {packet.source_ip}:{packet.source_port} | {messaggio} (Latenza: {latenza:.2f} ms)"
                # Salvataggio fisico del log come richiesto da specifiche
                logging.warning(testo_allarme)
                # Stampa sul terminale locale di Python
                print(f"[{ispettore.total_detections}] {testo_allarme}")

                # Manda il testo dell'allarme al Router Go via UDP
                alert_sock.sendto(testo_allarme.encode("utf-8"), router_address)

    except KeyboardInterrupt:
        print("\n[SEGNALE] Spegnimento analizzatore richiesto dall'utente.")

    finally:
        # ==========================================
        # GRACEFUL SHUTDOWN
        # ==========================================
        print("[CLEANUP] Chiusura sicura dei socket completata.")
        sock.close()
        alert_sock.close()


if __name__ == "__main__":
    avvia_server()
