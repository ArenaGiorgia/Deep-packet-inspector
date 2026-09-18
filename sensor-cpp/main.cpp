/*Il punto di ingresso dell'applicazione. Inizializza il programma, intercetta i segnali del 
sistema operativo (come il Ctrl+C per lo spegnimento pulito, o graceful shutdown) e 
avvia in parallelo i thread principali di cattura e inoltro */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "packet_handler.h"
#include "producer.h"
#include "consumer.h"

// Variabile atomica globale per gestire il segnale di interruzione da tastiera (Ctrl+C)
std::atomic<bool> sistema_attivo(true);

// Funzione richiamata automaticamente dal sistema operativo se premi Ctrl+C
void gestore_segnali(int segnale) {
    std::cout << "\n Ricevuto segnale di stop (Ctrl+C o spegnimento Docker) \n";
    sistema_attivo = false;
}

int main() {
    //Registriamo il gestore del segnale SIGINT (Ctrl+C) e SIGTERM (Docker)
    std::signal(SIGINT, gestore_segnali);
    std::signal(SIGTERM, gestore_segnali);
    
    /*Quando il microservizio Go chiude la connessione (crash, restart, riavvio del container) e tu 
    continui a chiamare send() su quel socket, Linux per default invia il segnale SIGPIPE al processo, 
    che termina il programma immediatamente*/
    std::signal(SIGPIPE, SIG_IGN); //ignoriamo SIGPIPE, gestiamo noi l'errore via valore di ritorno di send()
    
    std::cout << "Avvio di del packet inspector di C++ : \n";

    //Creazione della Coda condivisa
    CodaPacchetti coda_condivisa;

    //Creazione del Producer 
    //Sostituisci "eth0" con il nome reale della tua scheda di rete (es. "wlan0" o "en0")
    std::string interfaccia_rete = "eth0"; 
    CatturaTraffico producer(coda_condivisa, interfaccia_rete);

    //Creazione del Consumer (Spedisce i pacchetti Protobuf al microservizio Go)
    // Sfruttiamo il DNS di Docker: invece di 127.0.0.1, usiamo il nome del container "router"
    std::string indirizzo_IP_Go = "router"; 
    int porta_Go = 8080;
    InoltroTraffico consumer(coda_condivisa, indirizzo_IP_Go, porta_Go);

    //Avvio dei thread in background
    producer.avvia();
    consumer.avvia();

    std::cout << " Sensore attivo e in ascolto sulla scheda: " << interfaccia_rete << " Premi Ctrl+C per fermare.\n";

    //Il thread principale si mette in pausa ricorsiva, lasciando lavorare i thread in background
    while (sistema_attivo) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    //Sequenza di spegnimento pulito 
    std::cout << " Arresto della cattura di rete.\n";
    producer.ferma();
    
    std::cout << " Sblocco dei thread in attesa sulla coda. \n";
    coda_condivisa.stop();
    
    std::cout << " Chiusura della connessione di rete verso Go.\n";
    consumer.ferma();

    std::cout << " Tutti i thread sono stati chiusi correttamente. Programma terminato.\n";
    return 0;
}