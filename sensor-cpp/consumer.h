#pragma once

#include <string>
#include <thread>
#include <atomic>

// Includiamo il nostro "bancone" da cui prelevare i pacchetti
#include "packet_handler.h"

class InoltroTraffico {
private:
    
    // Usiamo sempre il reference & per lavorare sulla stessa coda riempita dal Producer
    CodaPacchetti& coda;

    //coordinate del microservizio di GO 
    //L'indirizzo IP (es. "127.0.0.1") 
    std::string indirizzo_ip;

    //la porta (es. 8080) a cui spediremo i dati
    int porta;

    //La socket come canale di comunicazione
    // È un numero intero (File Descriptor) che il sistema operativo ci dà per identificare la connessione.
    int socket_fd; 

    
    // Thread-safe, per lo spegnimento in modo pulito
    std::atomic<bool> attivo;

    //per lavorare in backgorund e inoltrare i pacchetti 
    std::thread thread_invio;

    
    // La funzione privata che gira in loop: preleva dalla coda, serializza e spedisce.
    void ciclo_di_invio();
    /*Qui dentro userò la funzione pop() che ho scritto in CodaPacchetti. Prenderò il pacchetto, chiederò
     a Protobuf di convertirlo in un array di byte (serializzazione) e lo invierò sul socket*/

    //Tenta di instaurare la connessione TCP con Go
    bool connetti_socket();

public:
    // Costruttore: chiede la coda condivisa e le coordinate di destinazione (IP e porta)
    InoltroTraffico(CodaPacchetti& coda_condivisa, const std::string& ip_destinazione, int porta_destinazione);

    // Distruttore: pulisce la memoria e chiude la connessione
    ~InoltroTraffico();

    // Metodi per accendere e spegnere il Forwarder dall'esterno
    void avvia();
    void ferma();
};