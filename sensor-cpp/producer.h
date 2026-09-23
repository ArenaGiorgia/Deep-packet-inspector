#pragma once
#include <string> 
#include <thread>
#include <mutex>  
#include <pcap.h> //libreria standard per intercettare il traffico di rete
#include "packet_handler.h" 

class CatturaTraffico {
private:

    //lavoriamo su un unica coda usata dal resto del programma.
    CodaPacchetti& coda;  
    
    //contiene il nome della scheda da cui copiare i dati come "eth0
    std::string interfaccia;
    
    /* Usiamo il puntatore della libreria pcap perché noi non creiamo la sessione a mano,ma chiediamo
    al sistema operativo di crearla per noi e ce la restiuisce per permetterci di gestirla */
    pcap_t* sessione;
   
    //la variabile di stato per lo spegnimento sicuro
    bool attivo;

    //sempre per la gestione dei thread
    std::mutex mutex_stato;
   
    //la funzione per leggere lo stato in modo sicuro (lock_guard)
    bool blocco_sicuro();

    //l'oggetto che staccherà il lavoro dal programma principale per farlo girare in background.
    std::thread thread_cattura;

    //la funzione che gira all'infinito dentro il thread per intercettare i byte.
    void cattura();

public:
    // il costruttore dove passiamo quale coda usare e quale scheda di rete ascoltare
    CatturaTraffico(CodaPacchetti& coda_condivisa, const std::string& nome_interfaccia);

    //il distruttore
    ~CatturaTraffico();

    //funzioni per controllare il nostro Producer dall'esterno
    void avvia();
    void ferma();
};