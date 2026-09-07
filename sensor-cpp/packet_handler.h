//definisco nel file .h la classe con i metodi che andrò a richiamare nel file packet_handler.cpp
#pragma once  //direttiva moderna che impedisce al compilatore di includere questo file due volte per errore
#include <queue> //per gestire le code
#include <mutex>  //per gestire i thread
#include <condition_variable> //meccanismo avanzato dei thread
#include <memory>  //introduce lo unique pointer
#include "packet_data.pb.h" 

class CodaPacchetti {
private:
    //La struttura dati reale che conterrà i pacchetti in fila
    std::queue<std::unique_ptr<packet_inspector::NetworkPacket>> coda;
    
    //Semafori per evitare che due thread accedano alla coda contemporaneamente
    std::mutex mutex;
    
    std::condition_variable condizione; 
    /*Invece di far girare il Forwarder a vuoto chiedendo alla coda "Ci sono pacchetti?
     Ci sono pacchetti?" (sprecando il 100% della CPU), questa variabile addormenta il Forwarder.
      Verrà risvegliato istantaneamente solo quando il pacchetto è pronto (tramite il metodo push).
    */
    
    //lo settiamo in vero se dobbiamo spegnere il programma 
    bool spegnimento = false; //normale funzionamento essendo in false 

public:
    // Metodo per il PRODUCER: Inserisce un nuovo pacchetto nella coda
    void push(std::unique_ptr<packet_inspector::NetworkPacket> packet);

    // Metodo per il CONSUMER: Preleva il pacchetto più vecchio
    std::unique_ptr<packet_inspector::NetworkPacket> pop();
     //Restituisce un puntatore nullo se la coda è in fase di spegnimento

    // Metodo per sbloccare tutti e chiudere il programma pulitamente
    void stop();
};