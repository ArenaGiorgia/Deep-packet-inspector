import socket
import time
import random
from typing import List


class TrafficInjector:
    """
    Simulatore di traffico di rete per stress-testare il sistema DPI.
    Genera un mix di traffico legittimo (Benign) e malevolo (Malicious)
    per verificare l'assenza di falsi positivi e l'accuratezza del pattern matching.
    """

    def __init__(self, target_ip: str = "127.0.0.1", target_port: int = 8080):
        self.target_ip = target_ip
        self.target_port = target_port

        # Dizionari per la simulazione
        self._passwords: List[str] = ["admin123", "password", "root", "qwerty"]
        self._benign_paths: List[str] = [
            "/index.html",
            "/api/status",
            "/assets/logo.png",
        ]

    def _send_tcp_payload(self, payload: str, attack_type: str) -> None:
        """Apre un socket effimero, inietta il payload e lo richiude."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(1.0)
                # Nota: In un test reale, la connect va verso un server dummy.
                # Se il sensore C++ è in ascolto promiscuo (libpcap), intercetterà tutto.
                sock.connect((self.target_ip, self.target_port))
                sock.sendall(payload.encode("utf-8"))
                print(f"[INJECT] {attack_type} inviato con successo.")
        except ConnectionRefusedError:
            # Se la porta è chiusa, stampiamo a video per la demo, ma il pacchetto
            # transita comunque sulla scheda di rete e viene sniffato dal sensore C++
            print(
                f"[INJECT] {attack_type} generato (Rifiutato dal target, ma visibile al Sensore)"
            )
        except Exception as e:
            print(f"[ERRORE] Impossibile iniettare traffico: {e}")

    def generate_benign_http(self) -> None:
        """Genera traffico web legittimo (Nessun allarme atteso)"""
        path = random.choice(self._benign_paths)
        payload = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {self.target_ip}\r\n"
            f"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
            f"Accept: */*\r\n\r\n"
        )
        self._send_tcp_payload(payload, f"Traffico Legittimo ({path})")

    def generate_malicious_http(self) -> None:
        """Simula un attacco HTTP POST con credenziali in chiaro"""
        pwd = random.choice(self._passwords)
        payload = (
            f"POST /api/login HTTP/1.1\r\n"
            f"Host: {self.target_ip}\r\n"
            f"Content-Type: application/json\r\n"
            f"\r\n"
            f'{{"username": "admin", "password": "{pwd}"}}'
        )
        self._send_tcp_payload(payload, f"Attacco HTTP (Credenziali: {pwd})")

    def generate_malicious_ftp(self) -> None:
        """Simula un tentativo di login FTP in chiaro"""
        pwd = random.choice(self._passwords)
        payload = f"USER admin\r\nPASS {pwd}\r\n"
        self._send_tcp_payload(payload, f"Attacco FTP (Credenziali: {pwd})")

    def start_simulation(self, iterations: int = 20) -> None:
        print(f" Avvio Simulazione (Target: {self.target_ip}:{self.target_port})...")
        print("Mischio traffico benigno (70%) e malevolo (30%).\n")

        for i in range(iterations):
            # Scelta pesata: 70% di probabilità traffico normale, 30% attacco
            scelta = random.choices(
                population=["benign", "malicious_http", "malicious_ftp"],
                weights=[0.7, 0.15, 0.15],
                k=1,
            )[0]

            if scelta == "benign":
                self.generate_benign_http()
            elif scelta == "malicious_http":
                self.generate_malicious_http()
            else:
                self.generate_malicious_ftp()

            # Attesa randomica per simulare la rete reale
            time.sleep(random.uniform(0.3, 1.5))

        print("\n Simulazione completata.")


if __name__ == "__main__":
    injector = TrafficInjector(target_ip="127.0.0.1", target_port=8080)
    injector.start_simulation(iterations=15)
