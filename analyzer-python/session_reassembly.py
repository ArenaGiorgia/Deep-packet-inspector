class TcpReassembler:
    """
    Gestisce la frammentazione del protocollo TCP.
    In un ambiente di produzione, riassembla i segmenti separati in un unico stream
    basandosi su IP Sorgente, IP Destinazione e Porte prima dell'ispezione DPI.
    """

    def __init__(self):
        # Un dizionario per memorizzare i frammenti temporanei in base alla sessione
        self._session_buffers = {}

    def add_segment(self, session_id: str, payload: bytes) -> bytes:
        """
        Aggiunge un segmento al buffer di sessione.
        (Versione Stub per dimostrazione architetturale)
        """
        if session_id not in self._session_buffers:
            self._session_buffers[session_id] = bytearray()

        self._session_buffers[session_id].extend(payload)

        # Per ora restituiamo direttamente il payload.
        # In futuro, qui ci sarà la logica basata sui Sequence Number del TCP.
        return bytes(self._session_buffers[session_id])

    def clear_session(self, session_id: str) -> None:
        """Pulisce la memoria quando la connessione TCP (FIN/RST) viene chiusa."""
        if session_id in self._session_buffers:
            del self._session_buffers[session_id]
