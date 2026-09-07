#pragma once

#include <string>
#include <thread>
#include <atomic>

// La libreria standard internazionale per intercettare il traffico di rete
#include <pcap.h> 

// Includiamo il nostro "bancone" dove appoggeremo i pacchetti catturati
#include "packet_handler.h" 

class CatturaTraffico {
private:

    // Lavoreremo fisicamente sulla stessa identica coda usata dal resto del programma.
    CodaPacchetti& coda;  // usiamo il reference & per lavorare NON sulla copia della coda
    
    // Conterrà il nome della scheda da cui copiare i dati (es. "eth0" o "wlan0")
    std::string interfaccia;
    
    // Questo è un puntatore speciale fornito dalla libreria pcap. 
    // Rappresenta la nostra "sessione di ascolto" aperta direttamente sul kernel del PC.
    pcap_t* sessione_pcap;
   // Usiamo il puntatore * perché noi non creiamo la sessione a mano, 
   //ma chiediamo al sistema operativo di crearla per noi e ce la restiuisce per permetterci di gestirla

    // std::atomic garantisce che leggere e scrivere questa variabile sia sicuro al 100% 
    //anche se due thread lo fanno nello stesso millisecondo, senza bisogno di usare un mutex.
    std::atomic<bool> attivo;
    /*con un classico bool andresti incontro a un grosso problema. Dato che il programma principale 
    (che preme lo stop) e il thread di cattura (che gira in background) lavorano in parallelo, 
    potrebbero provare a leggere e modificare quel bool nello stesso identico millisecondo. 
    Questo in C++ si chiama Data Race e fa crashare l'applicazione. Per usare un bool normale in 
    sicurezza, saresti costretta a creare un nuovo std::mutex (un altro semaforo) e usare un lock_guard 
    ogni singola volta che vuoi anche solo leggere il valore della variabile.
    std::atomic<bool> serve proprio a evitarti tutto questo lavoro extra.
    È semplicemente un booleano che dice al processore di gestire le letture e le scritture in modo 
    totalmente sicuro e istantaneo, senza bisogno di  scrivere il codice per i semafori.
    */ 


    // L'oggetto che staccherà il lavoro dal programma principale per farlo girare in background.
    std::thread thread_cattura;

    // La funzione privata che girerà all'infinito dentro il thread per intercettare i byte.
    void cattura();

public:
    // il costruttore quando creiamo l'oggetto, dobbiamo dirgli quale coda usare e quale scheda di rete ascoltare
    CatturaTraffico(CodaPacchetti& coda_condivisa, const std::string& nome_interfaccia);

    // il distruttore
    ~CatturaTraffico();

    // Metodi pubblici per controllare il nostro Producer dall'esterno
    void avvia();
    void ferma();
};