/*(Il Producer): Utilizza la libreria di basso livello libpcap per mettere la scheda di rete in ascolto.
 Il suo unico compito è intercettare i byte grezzi in transito il più velocemente possibile e riversarli
  nella coda di memoria condivisa, senza perdere cicli CPU per analizzarli.*/
#include "producer.h"
#include <iostream>


//costruttore MIL 
CatturaTraffico::CatturaTraffico(CodaPacchetti& coda_condivisa, const std::string& nome_interfaccia)
    : coda(coda_condivisa), interfaccia(nome_interfaccia), sessione(nullptr), attivo(false) {
    // Inizializziamo le variabili. All'inizio la sessione è vuota e non siamo attivi.
}

//distruttore
CatturaTraffico::~CatturaTraffico() {
   
    //chiamiamo la funzione ferma() per sicurezza, evitando che il thread rimanga "orfano".
    ferma(); 
}

// Funzione privata per leggere lo stato in modo sicuro 
bool CatturaTraffico::blocco_sicuro() {
    // Il lock_guard chiude il mutex. Nessuno può scrivere mentre leggiamo.
    std::lock_guard<std::mutex> blocco(mutex_stato);
    return attivo;
} // Alla fine di questa parentesi, il blocco si distrugge e il mutex si riapre da solo.


//Accensione 
void CatturaTraffico::avvia() {
    
    //Buffer fornito da pcap dove scriverà il testo di eventuali errori
    char buffer_errori[PCAP_ERRBUF_SIZE]; 

    //Apriamo la sessione di ascolto direttamente sul kernel.
    //nome scheda, c_str() Converte la stringa C++ in una stringa C-style compatibile con la libreria.
    // byte massimi da leggere (65535 "MTU" massimo, ovvero la dimensione massima in byte che un pacchetto IP può avere) 
    //modalità promiscua (1), costringe la scheda di rete a leggere tutto il traffico passante. 
    //timeout in ms (1000), buffer errori
    sessione = pcap_open_live(interfaccia.c_str(), 65535, 1, 1000, buffer_errori);

    if (sessione == nullptr) {
        std::cerr << "Errore critico: " << buffer_errori << "\n";
        return; // Se fallisce, interrompiamo tutto
    }

    {
        // Semaforo rosso: acquisiamo il lucchetto solo per il tempo necessario a modificare la variabile
        std::lock_guard<std::mutex> blocco(mutex_stato);
        //per avviare il ciclo while continuo 
        attivo = true;
    } // Semaforo verde: il lucchetto si rilascia qui

    //Lanciamo il lavoratore (thread) in background, dicendogli di eseguire la funzione "cattura"
    thread_cattura = std::thread(&CatturaTraffico::cattura, this);
}


//spegnimento 
void CatturaTraffico::ferma() {
    {
        // Prendiamo il lucchetto per controllare e modificare lo stato in modo sicuro
        std::lock_guard<std::mutex> blocco(mutex_stato);
        if (!attivo) return; //Se il Producer è già spento, la funzione esce immediatamente senza fare danni
        
        //Questo farà uscire il thread dal suo ciclo "while (blocco_sicuro())"
        attivo = false; 
    } // Il mutex si sblocca qui, PRIMA di aspettare la fine del thread con la join()

    //Per la sincronizzazione 
    //Aspettiamo che il thread del producer finisca l'ultimo giro e si riunisca in modo sicuro al programma principale
    if (thread_cattura.joinable()) {
        thread_cattura.join();
    }

    //Chiudiamo ufficialmente la sessione col sistema operativo
    if (sessione != nullptr) {
        pcap_close(sessione);
        sessione = nullptr;
    }
}


//Producer 
void CatturaTraffico::cattura() {
    
    //questa struct è l header dalla cattura usata da libcap 
    struct pcap_pkthdr* intestazione_pcap; //Metadati (esempio: quando è stato catturato e quanto è lungo)
   
    const u_char* byte_grezzi;
    /* Il contenuto fisico del pacchetto, È un array di byte grezzi non strutturati. Contiene tutto: 
    il frame Ethernet, l'intestazione IP, l'intestazione TCP/UDP e i dati dell'applicazione.*/

    //fin quando non stoppiamo, chiamiamo la funzione sicura blocco_sicuro() 
    while (blocco_sicuro()) {

        // Chiediamo alla scheda di rete: "C'è un pacchetto?"
        int risultato = pcap_next_ex(sessione, &intestazione_pcap, &byte_grezzi);

        if (risultato == 1) { 
            // 1 significa che abbiamo catturato un pacchetto con successo!
            
            // Creiamo un nuovo pacchetto vuoto usando i puntatori intelligenti
            auto pacchetto_nuovo = std::make_unique<packet_inspector::NetworkPacket>();

            // Salviamo i byte grezzi (che poi Python analizzerà) nel campo Protobuf "raw_payload"
            pacchetto_nuovo->set_raw_payload(byte_grezzi, intestazione_pcap->caplen);
            
            // Calcoliamo il timestamp in millisecondi per misurare poi le latenze
            int64_t timestamp = (intestazione_pcap->ts.tv_sec * 1000LL) + (intestazione_pcap->ts.tv_usec / 1000);
            pacchetto_nuovo->set_timestamp_ms(timestamp);

            //mandiamo il pacchetto nuovo alla coda 
            coda.push(std::move(pacchetto_nuovo));
        }
        
    }
}