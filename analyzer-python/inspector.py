# Il suo compito è ricevere i pacchetti nudi e crudi (che  gli verranno passati dal router Go) e decidere matematicamente se contengono una minaccia.
class NetworkInspector:
    def __init__(self):
        self.alert_count = (
            0  # contatore terrà memoria di quanti allarmi sono scattati nel tempo.
        )

    def analyze_packet(self, protocol, raw_payload):
        """Analizza il payload alla ricerca di password in chiaro."""

        # Approccio EAFP (Easier to Ask Forgiveness than Permission)
        # Invece di fare controlli preventivi sui tipi, tentiamo la decodifica e catturiamo l'errore a posteriori
        try:
            testo_decodificato = raw_payload.decode("utf-8")
        except UnicodeDecodeError:
            # Se la decodifica fallisce, catturiamo l'eccezione e ignoriamo il traffico[cite: 2]
            return False, "Traffico binario ignorato."

        # Utilizziamo il 'match-case', invece che if-else, per la leggibilità del codice.
        match protocol:
            case "TCP":
                #  verificare la presenza della stringa
                if "PASS" in testo_decodificato or "USER" in testo_decodificato:
                    self.alert_count += 1
                    return (
                        True,
                        f"[ALLARME TCP] Credenziali in chiaro rilevate! ({testo_decodificato})",
                    )
                return False, "TCP sicuro."

            case "UDP":
                return False, "Traffico UDP non analizzato per credenziali."

            case _:
                # Caso di default per i protocolli sconosciuti
                return False, f"Protocollo {protocol} non supportato."
