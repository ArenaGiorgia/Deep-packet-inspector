/*Estrae in modo asincrono i pacchetti dalla coda gestita da packet_handler.cpp. 
Legge gli header di base (IP e porte), popola la struttura Protobuf, serializza i dati in formato binario
 compresso e li spara via socket di rete verso il microservizio in Go.*/

#include "consumer.h"
#include <iostream>

// Librerie standard di Linux/Mac per gestire le connessioni di rete (Socket)
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

//costruttore 
InoltroTraffico::InoltroTraffico(CodaPacchetti& coda_condivisa, const std::string& ip_destinazione, int porta_destinazione)
    : coda(coda_condivisa), indirizzo_ip(ip_destinazione), porta(porta_destinazione), socket_fd(-1), attivo(false) {
    // MIL: socket_fd parte da -1 perché nei sistemi operativi 
    // i file descriptor validi partono da 0 in su. -1 significa "nessuna connessione".
}

//distruttore 
InoltroTraffico::~InoltroTraffico() {
    ferma(); //spegne tutto se l'oggetto viene distrutto
}


//creazione canale di comunicazione 
bool InoltroTraffico::connetti_socket() {
   
    // AF_INET = IPv4 e SOCK_STREAM = Protocollo TCP
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return false; // Creazione fallita
    }

    //Prepariamo l'indirizzo a cui "telefonare" (il microservizio Go)
    struct sockaddr_in indirizzo_server;
    indirizzo_server.sin_family = AF_INET;
    
    // htons serve a convertire il numero della porta nel formato leggibile dalla rete (Network Byte Order)
    indirizzo_server.sin_port = htons(porta);
    
    // Convertiamo l'indirizzo IP da stringa (es. "127.0.0.1") in formato binario
    inet_pton(AF_INET, indirizzo_ip.c_str(), &indirizzo_server.sin_addr);

    //Facciamo partire la "chiamata" verso Go
    int risultato_connessione = connect(socket_fd, (struct sockaddr*)&indirizzo_server, sizeof(indirizzo_server));
    if (risultato_connessione < 0) {
        close(socket_fd); // Se Go non risponde, riagganciamo
        
    //Quando il sistema operativo crea un socket valido, gli assegna un numero intero positivo 
    //(0, 1, ecc.).Inizializzarlo a -1 è la convenzione standard per indicare "nessuna connessione attiva".
        socket_fd = -1;
        return false;
    }

    return true; // Connessione stabilita
}

//avviamento 
void InoltroTraffico::avvia() {
    //assicuriamo che il client Go sia pronto a ricevere
    if (!connetti_socket()) {
        std::cerr << "Errore: Impossibile connettersi al microservizio Go.\n";
        return; 
    }

    attivo = true;
    thread_invio = std::thread(&InoltroTraffico::ciclo_di_invio, this);
}

void InoltroTraffico::ferma() {
    if (!attivo) return;

    attivo = false;

    // Sincronizziamo il thread
    if (thread_invio.joinable()) {
        thread_invio.join();
    }

    // Chiudiamo fisicamente la connessione di rete, rilasciando la risorsa di rete
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}


void InoltroTraffico::ciclo_di_invio() {
    
    while (attivo) {
        
        //preleviamo il pacchetto, se la coda è vuota, il thread si mette a dormire da solo non consumando  CPU.
        auto pacchetto_ricevuto = coda.pop();

        // Se pop() restituisce nullptr, significa che il programma si sta spegnendo
        if (pacchetto_ricevuto == nullptr) {
            break; // Usciamo dal ciclo while
        }

        //Trasformiamo l'oggetto Protobuf in una stringa di byte (serializzazione)
        std::string dati_serializzati;
        pacchetto_ricevuto->SerializeToString(&dati_serializzati);

        //Inviamo i byte attraverso la socket verso Go
        //passiamo la socket, i dati da inviare, la grandezza dei dati, 0 (nessun flag speciale)
        send(socket_fd, dati_serializzati.c_str(), dati_serializzati.size(), 0);
    }
}
 