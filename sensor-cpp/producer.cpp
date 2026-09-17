/*(Il Producer): Utilizza la libreria di basso livello libpcap per mettere la scheda di rete in ascolto.
 Il suo unico compito è intercettare i byte grezzi in transito il più velocemente possibile e riversarli
 nella coda di memoria condivisa, senza perdere cicli CPU per analizzarli.*/
#include "producer.h"
#include <iostream>

// costruttore MIL 
CatturaTraffico::CatturaTraffico(CodaPacchetti& coda_condivisa, const std::string& nome_interfaccia)
    : coda(coda_condivisa), interfaccia(nome_interfaccia), sessione(nullptr), attivo(false) {
    // Inizializziamo le variabili. All'inizio la sessione è vuota e non siamo attivi.
}

// distruttore
CatturaTraffico::~CatturaTraffico() {
    // chiamiamo la funzione ferma() per sicurezza, evitando che il thread rimanga "orfano".
    ferma(); 
}

bool CatturaTraffico::blocco_sicuro() {
    // Il lock_guard blocca automaticamente il mutex e lo sblocca quando la funzione finisce (RAII)
    std::lock_guard<std::mutex> lock(mutex_stato);
    return attivo;
}

// Accensione 
void CatturaTraffico::avvia() {
    
    // Buffer fornito da pcap dove scriverà il testo di eventuali errori
    char buffer_errori[PCAP_ERRBUF_SIZE]; 

    // Apriamo la sessione di ascolto direttamente sul kernel.
    sessione = pcap_open_live(interfaccia.c_str(), 65535, 1, 1000, buffer_errori);

    if (sessione == nullptr) {
        std::cerr << "Errore critico: " << buffer_errori << "\n";
        return; // Se fallisce, interrompiamo tutto
    }

    
    // FIX FONDAMENTALE: FILTRO BPF (BERKELEY PACKET FILTER)
    // Escludiamo il traffico interno (porte dei container Go e Python) per evitare "l'effetto eco".
    // Catturiamo tutto il resto, così il generatore Python (che ora punta alla porta 9999)
    // verrà intercettato correttamente!
    struct bpf_program filtro;
    std::string regola_filtro = "not port 8080 and not port 8081 and not port 9001 and not port 9002";
    if (pcap_compile(sessione, &filtro, regola_filtro.c_str(), 0, PCAP_NETMASK_UNKNOWN) != -1) {
        pcap_setfilter(sessione, &filtro);
    } else {
        std::cerr << "Impossibile compilare il filtro BPF.\n";
    }
    

    // per avviare il ciclo while continuo (usiamo il lock per sicurezza)
    {
        std::lock_guard<std::mutex> lock(mutex_stato);
        attivo = true;
    }

    // Lanciamo il lavoratore (thread) in background, dicendogli di eseguire la funzione "cattura"
    thread_cattura = std::thread(&CatturaTraffico::cattura, this);
}

// spegnimento 
void CatturaTraffico::ferma() {
    if (!blocco_sicuro()) return; // Se siamo già fermi usciamo
    
    // Questo farà uscire il thread dal suo ciclo "while"
    {
        std::lock_guard<std::mutex> lock(mutex_stato);
        attivo = false; 
    }

    // Per la sincronizzazione 
    // Aspettiamo che il thread finisca l'ultimo giro e si riunisca in modo pulito al programma principale
    if (thread_cattura.joinable()) {
        thread_cattura.join();
    }

    // Chiudiamo ufficialmente la sessione col sistema operativo
    if (sessione != nullptr) {
        pcap_close(sessione);
        sessione = nullptr;
    }
}


// Producer (Ora è un vero "Thin Sensor")
void CatturaTraffico::cattura() {
    struct pcap_pkthdr* intestazione_pcap; 
    const u_char* byte_grezzi;             

    // Usiamo la funzione sicura col Mutex per evitare Data Race!
    while (blocco_sicuro()) {
        int risultato = pcap_next_ex(sessione, &intestazione_pcap, &byte_grezzi);

        if (risultato == 1) { 
            
            // Creiamo un nuovo pacchetto vuoto (FIX: aggiunto il tipo per il template!)
            auto pacchetto_nuovo = std::make_unique<packet_inspector::NetworkPacket>();
            
            // Inseriamo il Timestamp Hardware
            int64_t timestamp = (intestazione_pcap->ts.tv_sec * 1000LL) + (intestazione_pcap->ts.tv_usec / 1000);
            pacchetto_nuovo->set_timestamp_ms(timestamp);

    
            // THIN SENSOR - NESSUNA DECODIFICA IP/TCP IN C++
            // Copiamo TUTTO il pacchetto Ethernet grezzo direttamente nel raw_payload.
            // Sarà il Worker Pool in Go a fare la fatica di decodificare gli header IP e TCP!
            if (intestazione_pcap->caplen > 0) {
                pacchetto_nuovo->set_raw_payload(byte_grezzi, intestazione_pcap->caplen);
            }

            // Mettiamo il pacchetto nella coda condivisa
            coda.push(std::move(pacchetto_nuovo));
        }
    }
}