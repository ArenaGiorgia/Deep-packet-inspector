import socket
import time
import random
from typing import List


class GeneratoreTraffico:
    """
    Simulatore di traffico di rete per stress-testare il sistema DPI.
    Genera un mix di traffico legittimo e malevolo
    per verificare l'assenza di falsi positivi e l'accuratezza del pattern matching.
    """

    def __init__(self, target_ip: str = "127.0.0.1", target_port: int = 9999):
        self.target_ip = target_ip
        self.target_port = target_port

        # Dizionari per la simulazione di attacchi e traffico legittimo
        self._password: List[str] = [
            "admin123",
            "password",
            "root",
            "qwerty",
            "123456",
            "letmein",
            "P@ssw0rd",
            "admin",
            "test",
        ]
        self._percorsi_legittimi: List[str] = [
            "/index.html",
            "/api/status",
            "/assets/logo.png",
            "/contact",
            "/about.html",
            "/css/style.css",
            "/js/app.js",
        ]

    def _invia_payload_tcp(self, payload: str, tipo_attacco: str) -> None:
        """Apre un socket effimero, inietta il payload e lo richiude."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(1.0)
                sock.connect((self.target_ip, self.target_port))
                sock.sendall(payload.encode("utf-8"))
                print(f"  -> {tipo_attacco} inviato con successo.")
        except ConnectionRefusedError:
            print(
                f"  -> {tipo_attacco} creato (Rifiutato dal target, ma catturato dal C++)"
            )
        except Exception as e:
            print(f"[ERRORE] Impossibile generare traffico: {e}")

    def genera_traffico_http_legittimo(self) -> None:
        """Genera traffico web legittimo (Nessun allarme atteso)"""
        percorso = random.choice(self._percorsi_legittimi)
        payload = (
            f"GET {percorso} HTTP/1.1\r\n"
            f"Host: {self.target_ip}\r\n"
            f"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
            f"Accept: */*\r\n\r\n"
        )
        self._invia_payload_tcp(payload, f"Traffico Legittimo ({percorso})")

    def genera_attacco_http(self) -> None:
        """Simula un attacco HTTP POST con credenziali in chiaro"""
        pwd = random.choice(self._password)
        payload = (
            f"POST /api/login HTTP/1.1\r\n"
            f"Host: {self.target_ip}\r\n"
            f"Content-Type: application/json\r\n"
            f"\r\n"
            f'{{"username": "admin", "password": "{pwd}"}}'
        )
        self._invia_payload_tcp(payload, f"Attacco HTTP (Credenziali: {pwd})")

    def genera_attacco_ftp(self) -> None:
        """Simula un tentativo di login FTP in chiaro"""
        pwd = random.choice(self._password)
        payload = f"USER admin\r\nPASS {pwd}\r\n"
        self._invia_payload_tcp(payload, f"Attacco FTP (Credenziali: {pwd})")

    def avvia_simulazione(self, iterazioni: int = 20) -> None:
        print(f" Avvio Simulazione (Target: {self.target_ip}:{self.target_port})...")
        print("Mischio traffico benigno (60%) e malevolo (40%).\n")

        for i in range(iterazioni):
            print(f"[GENERATE] Progresso: Richiesta {i+1}/{iterazioni}")

            scelta = random.choices(
                population=["legittimo", "attacco_http", "attacco_ftp"],
                weights=[0.6, 0.20, 0.20],
                k=1,
            )[0]

            if scelta == "legittimo":
                self.genera_traffico_http_legittimo()
            elif scelta == "attacco_http":
                self.genera_attacco_http()
            else:
                self.genera_attacco_ftp()

            #  La Pausa di mezzo secondo (0.5).
            # Garantisce una fluidità perfetta sulla Dashboard Web, permettendoti
            # di parlare mentre il traffico scorre in tempo reale ("Effetto Matrix").
            time.sleep(0.5)

        print("\n Simulazione completata.")


if __name__ == "__main__":
    generatore = GeneratoreTraffico(target_ip="router", target_port=9999)
    # Aumentato a 200 iterazioni per garantire circa 100 secondi di simulazione continua
    generatore.avvia_simulazione(iterazioni=200)
