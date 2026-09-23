#pragma once

#include <string>
#include <thread>
#include <mutex> 
#include "packet_handler.h"

class InoltroTraffico {
private:
    
    //Lavoriamo sempre sulla stessa coda riempita dal Producer
    CodaPacchetti& coda;
 
    //L'indirizzo IP di GO  
    std::string indirizzo_ip;

    //la porta a cui spediamo i dati
    int porta;


    //un numero intero (File Descriptor) che il SO ci dà nella socket per identificare la connessione
    int socket_fd; 

    //variabile di stato per lo spegnimento in modo pulito
    bool attivo;

    //mutex per proteggere il bool
    std::mutex mutex_stato;   

    //la funzione per leggere lo stato in modo sicuro 
    bool blocco_sicuro();

    //per lavorare in backgorund e inoltrare i pacchetti 
    std::thread thread_invio;

    
    //la funzione che gira in loop dove preleviamo dalla coda, serializziamo e spediamo
    /*Qui dentro userò la funzione pop() scritta in CodaPacchetti. Prenderò il pacchetto, chiederò
     a Protobuf di convertirlo in un array di byte (serializzazione) e lo invierò sul socket*/
    void ciclo_di_invio();
    

    //Tenta di instaurare la connessione TCP con Go
    bool connetti_socket();

    //invia tutti i byte richiesti, controllando che l'invio vada a buon fine
    bool invia_tutto(const char* dati, int lunghezza);

public:
    //il Costruttore
    InoltroTraffico(CodaPacchetti& coda_condivisa, const std::string& ip_destinazione, int porta_destinazione);

    //il Distruttore
    ~InoltroTraffico();

    //le funzioni per accendere e spegnere il consumer dall'esterno
    void avvia();
    void ferma();
};