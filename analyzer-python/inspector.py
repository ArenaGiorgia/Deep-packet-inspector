import re


class AnalizzatoreDPI:
    """
    Componente di Deep Packet Inspection (DPI).
    Analizza i payload di rete decodificati alla ricerca di dati sensibili in chiaro
    tramite l'uso di Espressioni Regolari (Regex).
    """

    def __init__(self) -> None:
        # INCAPSULAMENTO: L'underscore singolo indica per convenzione che la variabile
        # è ad uso interno (protetta) e non deve essere modificata dall'esterno.
        self._conteggio_rilevamenti: int = 0

        # [OTTIMIZZAZIONE E PRECISIONE FORENSE]: L'automa a stati finiti (FSM) dell'espressione
        # regolare viene pre-compilato a tempo di inizializzazione per massimizzare il throughput.
        # Utilizziamo re.MULTILINE per far sì che '^' corrisponda all'inizio di ogni singola riga
        # (fondamentale per gli header HTTP e i comandi FTP separati da \r\n).
        # - '^user\s' e '^pass\s': Intercettano i comandi FTP esatti, ignorando falsi positivi
        #   come l'header legittimo "User-Agent: Mozilla/5.0" (che ha il trattino e non lo spazio).
        # - '\bpassword\b': Utilizza i Word Boundaries (\b) per intercettare le chiavi JSON
        #   esatte (es. "password": "123") ignorando parole che la contengono (es. "passenger").
        self._pattern_credenziali = re.compile(
            r"(?i)(^user\s|^pass\s|\bpassword\b)", re.MULTILINE
        )

    def analizza_payload(
        self, protocollo: str, payload_grezzo: bytes
    ) -> tuple[bool, str]:
        """
        Ispeziona il payload binario in base al protocollo di trasporto.
        Restituisce una tupla (Minaccia_Trovata, Messaggio_Di_Log).
        """
        # PARADIGMA EAFP (Easier to Ask Forgiveness than Permission)
        # Invece di controllare byte per byte se il payload è testuale (LBYL),
        # tentiamo direttamente la decodifica. Se è traffico crittografato o binario puro,
        # solleverà un'eccezione UnicodeDecodeError che gestiamo con l'except.
        try:
            testo_decodificato = payload_grezzo.decode("utf-8")
        except UnicodeDecodeError:
            return False, "Traffico binario o crittografato (ignorato)."

        # STRUCTURAL PATTERN MATCHING
        # Sostituisce le vecchie catene di if-elif o i dizionari, garantendo
        # una logica di diramazione estremamente pulita e performante.
        match protocollo.upper():
            case "TCP":
                # Esecuzione del pattern matching sul payload testuale
                if self._pattern_credenziali.search(testo_decodificato):
                    self._conteggio_rilevamenti += 1

                    # Pulizia dei caratteri di formattazione (es. \r\n) usando .strip()
                    payload_pulito = testo_decodificato.strip()
                    return (
                        True,
                        f"[ALERT TCP] Dati sensibili in chiaro rilevati | Payload: {payload_pulito}",
                    )
                return False, "Traffico TCP ispezionato (Nessuna anomalia)."

            case "UDP":
                #  L'ispezione delle credenziali non è applicata ai datagrammi UDP
                return False, "Policy DPI non applicata al traffico UDP."

            case _:
                # Caso di default (fallback) per gestire protocolli inattesi
                return False, f"Protocollo di trasporto non supportato: {protocollo}"

    # DECORATORE @property
    # Trasforma il metodo in un attributo a sola lettura.
    # Espone in modo sicuro il contatore nascondendo la logica interna.
    @property
    def rilevamenti_totali(self) -> int:
        """
        Proprietà (Getter) per accedere in sola lettura al contatore degli allarmi.
        """
        return self._conteggio_rilevamenti
