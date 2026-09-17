import time
from functools import wraps


# DECORATORE: Misura il tempo di esecuzione di una funzione Python
def misura_tempo_esecuzione(func):
    """
    Decoratore per il profiling delle prestazioni.
    Sfrutta time.perf_counter() per la massima precisione temporale
    e functools.wraps per preservare la docstring e il nome della funzione originale.
    """

    @wraps(func)
    def wrapper(*args, **kwargs):
        # time.perf_counter misura il tempo di clock a precisione elevata
        tempo_inizio = time.perf_counter()
        risultato = func(*args, **kwargs)
        tempo_fine = time.perf_counter()

        tempo_ms = (tempo_fine - tempo_inizio) * 1000
        print(f"[PROFILING] Funzione '{func.__name__}' eseguita in {tempo_ms:.4f} ms")

        return risultato

    return wrapper


class PerformanceMonitor:
    """
    Modulo di telemetria per misurare la latenza end-to-end dell'architettura.
    Calcola il tempo trascorso dalla cattura hardware (C++) all'analisi software (Python).
    """

    def __init__(self) -> None:
        self.latenza_massima_ms: float = 0.0
        self.pacchetti_totali: int = 0
        self.latenza_cumulativa: float = 0.0

    def record_latency(self, timestamp_cplusplus_ms: int) -> float:
        """Calcola la latenza di un singolo pacchetto in millisecondi."""
        if timestamp_cplusplus_ms == 0:
            return 0.0  # Ignora se il timestamp non è valido

        # Tempo attuale in millisecondi basato sull'orologio di sistema
        tempo_attuale_ms = int(time.time() * 1000)

        # Calcolo del delta temporale end-to-end
        latenza = tempo_attuale_ms - timestamp_cplusplus_ms

        # Aggiornamento delle metriche statistiche
        self.pacchetti_totali += 1
        self.latenza_cumulativa += latenza
        if latenza > self.latenza_massima_ms:
            self.latenza_massima_ms = latenza

        return float(latenza)

    def get_average_latency(self) -> float:
        """Restituisce la latenza media dell'ecosistema."""
        if self.pacchetti_totali == 0:
            return 0.0
        return self.latenza_cumulativa / self.pacchetti_totali
