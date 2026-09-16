import time
from functools import wraps


# DECORATORE CUSTOM misura il tempo di esecuzione CPU della singola funzione Python
def misura_tempo_esecuzione(func):

    @wraps(func)
    def wrapper(*args, **kwargs):
        start_time = time.perf_counter()
        risultato = func(*args, **kwargs)
        end_time = time.perf_counter()

        tempo_ms = (end_time - start_time) * 1000
        print(f"[PROFILING] Funzione '{func.__name__}' elaborata in {tempo_ms:.4f} ms")

        return risultato

    return wrapper


# CLASSE DI TELEMETRIA
class PerformanceMonitor:
    """
    Modulo di telemetria per misurare la latenza end-to-end dell'architettura.
    Calcola il tempo trascorso dalla cattura hardware (C++) all'analisi software (Python).
    """

    def __init__(self):
        self.max_latency_ms = 0.0
        self.total_packets = 0
        self.cumulative_latency = 0.0

    def record_latency(self, cplusplus_timestamp_ms: int) -> float:
        """Calcola la latenza di un singolo pacchetto in millisecondi."""
        if cplusplus_timestamp_ms == 0:
            return 0.0  # Ignora se il timestamp non è valido

        # Tempo attuale in millisecondi
        current_time_ms = int(time.time() * 1000)

        # Calcolo del delta
        latency = current_time_ms - cplusplus_timestamp_ms

        # Aggiornamento delle statistiche
        self.total_packets += 1
        self.cumulative_latency += latency
        if latency > self.max_latency_ms:
            self.max_latency_ms = latency

        return latency

    def get_average_latency(self) -> float:
        """Restituisce la latenza media dell'ecosistema."""
        if self.total_packets == 0:
            return 0.0
        return self.cumulative_latency / self.total_packets
