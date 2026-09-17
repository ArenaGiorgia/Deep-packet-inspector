import collections
from typing import Dict, Tuple


class RiassemblatoreTCP:
    """
    Gestisce la frammentazione del protocollo TCP.
    Riassembla i segmenti separati in un unico stream ordinato
    sfruttando le code ad alte prestazioni (deque) e i Sequence Number.
    """

    def __init__(self) -> None:
        # Dizionario che associa ogni ID di sessione a una coda (deque) ad alte prestazioni.
        # La deque conterrà tuple immutabili strutturate come: (Sequence_Number, Payload_Binario).
        self._buffer_sessioni: Dict[str, collections.deque] = {}

    def aggiungi_segmento(
        self, id_sessione: str, payload: bytes, seq_num: int, flag_tcp: int
    ) -> bytes:
        """
        Aggiunge un segmento alla coda di sessione, lo riordina tramite Sequence Number
        e restituisce il flusso contiguo ricostruito.
        """
        # Se la sessione è nuova, inizializziamo la doppia coda
        # Utilizziamo deque in base alle linee guida di ottimizzazione per le code performanti.
        if id_sessione not in self._buffer_sessioni:
            self._buffer_sessioni[id_sessione] = collections.deque()

        # Inseriamo il nuovo frammento all'interno della deque.
        # Le tuple sono ideali qui per garantire l'immutabilità della coppia (SeqNum, Dati).
        self._buffer_sessioni[id_sessione].append((seq_num, payload))

        # Ordinamento dinamico in base al Sequence Number (il primo elemento della tupla: x[0]).
        # Questo garantisce che i pacchetti arrivati fuori ordine vengano riallineati correttamente
        # prima di essere passati all'analizzatore Regex.
        coda_ordinata = sorted(self._buffer_sessioni[id_sessione], key=lambda x: x[0])

        # Riassemblaggio del payload completo
        payload_completo = bytearray()
        for _, dati in coda_ordinata:
            payload_completo.extend(dati)

        # GESTIONE CICLO DI VITA TCP (FIN / RST)
        # I flag TCP sono contenuti in un singolo byte.
        # Il flag FIN (Fine connessione normale) corrisponde al bit 0 (valore 1).
        # Il flag RST (Reset connessione anomalo) corrisponde al bit 2 (valore 4).
        # Usiamo l'operatore bit a bit AND (&) per mascherare ed estrarre i flag rilevanti.
        if (flag_tcp & 0x01) != 0 or (flag_tcp & 0x04) != 0:
            self.pulisci_sessione(id_sessione)

        return bytes(payload_completo)

    def pulisci_sessione(self, id_sessione: str) -> None:
        """Pulisce la memoria eliminando i dati quando la connessione TCP viene chiusa."""
        # PARADIGMA EAFP (Easier to Ask Forgiveness than Permission)
        # Invece di verificare preventivamente se la chiave esiste (LBYL), tentiamo
        # l'operazione direttamente e gestiamo l'eventuale errore a posteriori.
        try:
            del self._buffer_sessioni[id_sessione]
        except KeyError:
            pass  # La sessione era già stata rimossa o non è mai stata inizializzata
