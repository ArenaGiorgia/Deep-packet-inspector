# 🛡️ Distributed Deep Packet Inspection (DPI) System

Sistema distribuito in tempo reale per l'intercettazione, il filtraggio, il riassemblaggio e l'analisi forense del traffico di rete (DPI), sviluppato come progetto per la materia APL del CdL in Ingegneria Informatica Magistrale presso l'Università degli studi di Catania.

---

## Architettura del Sistema

L'applicazione adotta un'architettura modulare distribuita a tre livelli, interamente containerizzata tramite **Docker Compose**:

1. **Modulo C++ (Packet Sniffer):** Cattura a basso livello i pacchetti di rete grezzi direttamente dall'interfaccia di rete e li inoltra via stream TCP ad alte prestazioni.
2. **Modulo Go (Router Concorrente & Dashboard):** Agisce come orchestratore centrale. Sfrutta il modello CSP (*Communicating Sequential Processes*), worker pools in parallelo e un WebSocket Hub per smistare i pacchetti e aggiornare in tempo reale la Dashboard Web.
3. **Modulo Python (Analizzatore Forense - Backend):** Riceve i flussi decodificati, gestisce il riassemblaggio avanzato delle sessioni TCP (tramite code `deque` e *Sequence Number*) ed esegue l'analisi di Livello 7 tramite Espressioni Regolari (Regex) alla ricerca di dati sensibili in chiaro.
4. **Modulo Python (Generatore di Traffico):** Un simulatore integrato per stress-testare il sistema generando un mix controllato di traffico web legittimo e attacchi simulati (HTTP/FTP).

---

## Requisiti di Sistema

Assicurati di avere installato sul tuo sistema:
* [Docker](https://www.docker.com/)
* [Docker Compose](https://docs.docker.com/compose/)

---

## Come Avviare il Progetto

La gestione dell'intera infrastruttura è centralizzata tramite Docker Compose. 

1. **Clona la repository e posizionati nella cartella principale:**
   ```bash
   cd nome-cartella-progetto
   ```

2. **Avvia l'intera pipeline (Build e Run):**
   ```bash
   docker-compose up --build
   ```
   *Nota: Il flag `--build` assicura che i container (inclusa la compilazione dei contratti Protobuf in Python e l'eseguibile statico in Go) vengano ricompilati correttamente.*

3. **Accedi alla Dashboard Web in Tempo Reale:**
   Apri il tuo browser e vai all'indirizzo:
    **`http://localhost:8081`**

4. **Arresto dei Servizi:**
   Per fermare l'esecuzione e ripulire le risorse di rete virtuali, premi `Ctrl + C` nel terminale, oppure esegui:
   ```bash
   docker-compose down
   ```

---

## Funzionalità della Dashboard
* **Split-Screen in Tempo Reale:** 
  * Colonna di Sinistra: Flusso del traffico di rete grezzo monitorato a Livello 4.
  * Colonna di Destra: Intelligence sulle minacce e alert di sicurezza bloccati a Livello 7.
* **Pannello KPI di Telemetria:** Monitoraggio costante del traffico totale analizzato, delle minacce rilevate e della latenza end-to-end calcolata dal modulo di performance in Python.

## Autori
* Giorgia Arena
* Alessio Tornabene