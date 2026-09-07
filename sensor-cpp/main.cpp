/*Il punto di ingresso dell'applicazione. Inizializza il programma, intercetta i segnali del 
sistema operativo (come il Ctrl+C per lo spegnimento pulito, o graceful shutdown) e 
avvia in parallelo i thread principali di cattura e inoltro */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

#include "packet_handler.h"
#include "capture_thread.h"
#include "forwarder.h"

// Variabile atomica globale per gestire il segnale di interruzione da tastiera (Ctrl+C)
std::atomic<bool> sistema_attivo(true);

// Funzione richiamata automaticamente dal sistema operativo se premi Ctrl+C
void gestore_segnali(int segnale) {
    std::cout << "Ricevuto segnale di stop (Ctrl+C). Avvio dello spegnimento pulito...\n";
    sistema_attivo = false;
}

int main() {
    //Registriamo il gestore del segnale SIGINT (Ctrl+C)
    std::signal(SIGINT, gestore_segnali);

    std::cout << "Avvio di PACKET INSPECTION (C++) : \n";

    //Creazione della Coda condivisa
    CodaPacchetti coda_condivisa;

    //Creazione del Producer 
    //Sostituisci "eth0" con il nome reale della tua scheda di rete (es. "wlan0" o "en0")
    std::string interfaccia_rete = "eth0"; 
    CatturaTraffico producer(coda_condivisa, interfaccia_rete);

    //Creazione del Consumer (Spedisce i pacchetti Protobuf al microservizio Go)
    std::string indirizzo_IP_Go = "127.0.0.1";
    int porta_Go = 8080;
    InoltroTraffico consumer(coda_condivisa, indirizzo_IP_Go, porta_Go);

    //Avvio dei thread in background
    producer.avvia();
    consumer.avvia();

    std::cout << "Sensore attivo e in ascolto sulla scheda " << interfaccia_rete << " Premi Ctrl+C per fermare.\n";

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