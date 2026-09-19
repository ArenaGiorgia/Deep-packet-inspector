#pragma once

#include <string> 
#include <thread>
#include <mutex>  
#include <pcap.h> // Su Linux si traduce nella libreria libpcap. La libreria standard per intercettare il traffico di rete
#include "packet_handler.h" 

class CatturaTraffico {
private:

    // Lavoreremo fisicamente sulla stessa identica coda usata dal resto del programma.
    CodaPacchetti& coda;  // usiamo il reference & per lavorare NON sulla copia della coda
    
    // Conterrà il nome della scheda da cui copiare i dati come "eth0
    std::string interfaccia;
    
    // Questo è un puntatore speciale fornito dalla libreria pcap. 
    // Rappresenta la nostra "sessione di ascolto" aperta direttamente sul kernel del PC.
    pcap_t* sessione;
    // Usiamo il puntatore * perché noi non creiamo la sessione a mano, 
    // ma chiediamo al sistema operativo di crearla per noi e ce la restiuisce per permetterci di gestirla

    // Variabile di stato e semaforo dedicato per lo spegnimento sicuro
    bool attivo;
    std::mutex mutex_stato;
    /* 
    Dato che il programma principale (che preme lo stop) e il thread producer (che gira in background) 
    lavorano in parallelo, potrebbero provare a leggere e modificare questa variabile nello stesso 
    identico millisecondo. Usiamo un lock_guard ogni singola volta che vogliamo leggere o scrivere il valore della variabile 
    tramite la funzione is_attivo(). 
   */ 

    // Funzione privata per leggere lo stato in modo sicuro (lock_guard)
    bool blocco_sicuro();

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