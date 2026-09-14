import re


class DpiAnalyzer:
    """
    Componente di Deep Packet Inspection (DPI).
    Analizza i payload di rete decodificati alla ricerca di dati sensibili in chiaro.
    """

    def __init__(self) -> None:
        # [INCAPSULAMENTO]: L'underscore indica che queste variabili sono 'private'
        # e non devono essere manipolate direttamente dall'esterno dell'oggetto.
        self._detection_count: int = 0

        # [OTTIMIZZAZIONE]: L'automa a stati finiti (FSM) dell'espressione regolare
        # viene pre-compilato a tempo di inizializzazione per massimizzare il throughput.
        self._credential_pattern = re.compile(r"(?i)(user|pass|password|login)")

    def analyze_payload(self, protocol: str, raw_payload: bytes) -> tuple[bool, str]:
        """
        Ispeziona il payload binario in base al protocollo di trasporto.
        """
        # ==========================================
        # PARADIGMA EAFP (Easier to Ask Forgiveness than Permission)
        # ==========================================
        # Un payload crittografato o puramente binario genererà un'eccezione
        # durante la decodifica UTF-8. La catturiamo per garantire la continuità del servizio.
        try:
            decoded_text = raw_payload.decode("utf-8")
        except UnicodeDecodeError:
            return False, "Traffico binario o crittografato (ignorato)."

        # [STRUCTURAL PATTERN MATCHING]: Costrutto Python 3.10+
        match protocol.upper():
            case "TCP":
                # Esecuzione del pattern matching sul payload decodificato
                if self._credential_pattern.search(decoded_text):
                    self._detection_count += 1

                    # Pulizia dei caratteri di escape (\r\n) per un logging pulito
                    clean_payload = decoded_text.strip()
                    return (
                        True,
                        f"[ALERT TCP] Dati sensibili in chiaro rilevati | Payload: {clean_payload}",
                    )
                return False, "Traffico TCP ispezionato (Nessuna anomalia)."

            case "UDP":
                # Policy corrente: l'ispezione delle credenziali non è applicata ai datagrammi UDP
                return False, "Policy DPI non applicata al traffico UDP."

            case _:
                # Caso di fallback (Gestione anomalie strutturali del pacchetto)
                return False, f"Protocollo di trasporto non supportato: {protocol}"

    @property
    def total_detections(self) -> int:
        """
        Proprietà (Getter) per accedere in sola lettura al contatore degli allarmi.
        """
        return self._detection_count
