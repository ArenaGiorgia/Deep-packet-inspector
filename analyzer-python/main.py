import socket
import logging
from google.protobuf.message import DecodeError
import packet_data_pb2
from inspector import AnalizzatoreDPI
from timing import PerformanceMonitor
from session_reassembly import RiassemblatoreTCP


class AnalizzatoreLogico:
    """
    Modulo di analisi di Livello 7 (Backend).
    Riceve i flussi grezzi dal router Go, riassembla le sessioni TCP
    e applica l'analisi forense tramite pattern matching.
    """

    def __init__(
        self,
        host: str = "0.0.0.0",
        listen_port: int = 9001,
        alert_host: str = "router",
        alert_port: int = 9002,
    ):
        # Definiamo e accediamo agli attributi di istanza
        self.host: str = host
        self.listen_port: int = listen_port
        self.alert_address: tuple = (alert_host, alert_port)
        self.buffer_size: int = 4096

        # Dependency Injection dei sottomoduli
        self.ispettore = AnalizzatoreDPI()
        self.monitor = PerformanceMonitor()
        self.reassembler = RiassemblatoreTCP()

        # Configurazione del logging su file
        logging.basicConfig(
            filename="allarmi_dpi.log",
            level=logging.WARNING,
            format="%(asctime)s | %(message)s",
        )

    def avvia_server(self) -> None:
        """Avvia il ciclo di ascolto infinito (Acceptor Loop) UDP."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((self.host, self.listen_port))

        alert_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        print(f"Analizzatore Forense in ascolto su {self.host}:{self.listen_port}...")

        try:
            while True:
                buffer, addr = sock.recvfrom(self.buffer_size)
                packet = packet_data_pb2.NetworkPacket()
                # PARADIGMA EAFP (Easier to Ask Forgiveness than Permission)
                # Invece di controllare i tipi o la lunghezza del bytearray prima ,
                # tentiamo direttamente la deserializzazione e intercettiamo l'errore.
                try:
                    packet.ParseFromString(buffer)
                except DecodeError:
                    print(
                        f"[SECURITY WARNING] Ricevuto payload malformato da {addr}. Pacchetto scartato."
                    )
                    continue

                # TIMING (Telemetria End-to-End)
                latenza = self.monitor.record_latency(packet.timestamp_ms)

                # SESSION REASSEMBLY (Gestione TCP)
                # L'uso delle f-strings garantisce efficienza e pulizia nell'interpolazione
                session_id = f"{packet.source_ip}:{packet.source_port}-{packet.dest_ip}:{packet.dest_port}"
                payload_assemblato = self.reassembler.aggiungi_segmento(
                    session_id, packet.raw_payload, packet.seq_num, packet.flag_tcp
                )

                # Deleghiamo l'ispezione (Pattern Matching)
                minaccia_trovata, messaggio = self.ispettore.analizza_payload(
                    packet.protocol, payload_assemblato
                )

                if minaccia_trovata:
                    testo_allarme = f"🚨 MINACCIA DA {packet.source_ip}:{packet.source_port} | {messaggio} (Latenza: {latenza:.2f} ms)"
                    logging.warning(testo_allarme)
                    print(f"[{self.ispettore.rilevamenti_totali}] {testo_allarme}")
                    alert_sock.sendto(testo_allarme.encode("utf-8"), self.alert_address)

        except KeyboardInterrupt:
            print("\n[SEGNALE] Spegnimento analizzatore richiesto dall'utente.")

        finally:
            # GRACEFUL SHUTDOWN (Cleanup)
            print("[CLEANUP] Chiusura sicura dei socket completata.")
            sock.close()
            alert_sock.close()


# Blocco di protezione per evitare esecuzioni accidentali in caso di importazione
if __name__ == "__main__":
    analizzatore = AnalizzatoreLogico()
    analizzatore.avvia_server()
