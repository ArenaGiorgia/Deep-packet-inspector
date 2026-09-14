import socket
from google.protobuf.message import DecodeError  # Importiamo l'eccezione specifica
import packet_data_pb2
from inspector import DpiAnalyzer
from timing import PerformanceMonitor
from session_reassembly import TcpReassembler


def avvia_server():
    UDP_IP = "0.0.0.0"  # L'analizzatore ascolta su tutte le interfacce di rete
    UDP_PORT = 9001  # L'analizzatore ascolta sulla porta 9001
    BUFFER_SIZE = 4096  # Standard industriale (potenza di 2) per i buffer di rete

    # Setup di rete: Creazione del socket UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))

    # [DEPENDENCY INJECTION]: Inizializziamo il motore di analisi (Livello 7) e i nuovi moduli
    ispettore = DpiAnalyzer()
    monitor = PerformanceMonitor()
    reassembler = TcpReassembler()

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
            # Invece di validare preventivamente i byte, tentiamo la decodifica.
            # Se il payload non rispetta il contratto Protobuf, catturiamo l'errore
            # e passiamo al pacchetto successivo, garantendo la resilienza (no-crash).
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
            # Calcoliamo i millisecondi trascorsi da quando il C++ ha catturato il pacchetto
            # a quando Python lo sta elaborando.
            latenza = monitor.record_latency(packet.timestamp_ms)

            # ==========================================
            # SESSION REASSEMBLY (Gestione TCP)
            # ==========================================
            # Creiamo l'ID di sessione tramite la "Tupla a 4" per riunire i frammenti
            session_id = f"{packet.source_ip}:{packet.source_port}-{packet.dest_ip}:{packet.dest_port}"

            # Passiamo il payload al riassemblatore
            payload_assemblato = reassembler.add_segment(session_id, packet.raw_payload)

            # 3. Deleghiamo l'ispezione (Pattern Matching) del payload all'ispettore
            # Usiamo il payload_assemblato invece di packet.raw_payload
            minaccia_trovata, messaggio = ispettore.analyze_payload(
                packet.protocol, payload_assemblato
            )

            # 4. Se la funzione restituisce True, stampiamo l'allarme per la Dashboard
            # Aggiungiamo anche il dato della latenza appena calcolato!
            if minaccia_trovata:
                print(
                    f"[{ispettore.total_detections}] {messaggio} (Latenza: {latenza} ms)"
                )

    except KeyboardInterrupt:
        # Intercettazione del segnale OS (SIGINT)
        print("\n[SEGNALE] Spegnimento analizzatore richiesto dall'utente.")

    finally:
        # ==========================================
        # GRACEFUL SHUTDOWN (Equivalente al 'defer' di Go)
        # ==========================================
        # Questo blocco garantisce deterministicamente il rilascio delle risorse (Teardown)
        # della porta di rete, indipendentemente da come il programma sia terminato.
        print("[CLEANUP] Chiusura sicura del socket completata.")
        sock.close()


# Entry Point Guard: Previene l'esecuzione involontaria in caso di importazione del modulo
if __name__ == "__main__":
    avvia_server()
