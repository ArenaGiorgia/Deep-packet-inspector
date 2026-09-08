//definisco nel file .h la classe con i metodi che andrò a richiamare nel file packet_handler.cpp
#pragma once  //direttiva moderna che impedisce al compilatore di includere file header .h due volte per errore
#include <queue> //per gestire le code (push,pop,empty,front)
#include <mutex>  //per gestire i thread (lock_guard, unique_lock )
#include <condition_variable> //meccanismo avanzato dei thread (wait, notify_one,notify_all)
#include <memory>  //introduce lo unique pointer
#include "packet_data.pb.h" //per il protobuffer 

class CodaPacchetti {
private:
    /*Stiamo dichiarando una coda (std::queue) che contiene oggetti esclusivi (unique_ptr) 
    di tipo NetworkPacket (la classe generata dal tuo file .proto) */
    std::queue<std::unique_ptr<packet_inspector::NetworkPacket>> coda;
    
    //Semafori per evitare che due thread accedano alla coda contemporaneamente
    std::mutex mutex;
    
    std::condition_variable condizione; 
    /*Invece di far girare il forwarder a vuoto chiedendo alla coda "Ci sono pacchetti?
     Ci sono pacchetti?"questa variabile addormenta il Forwarder(consumer).
      Verrà risvegliato istantaneamente solo quando il pacchetto è pronto tramite il metodo push 
      che fara poi un notify_one .
    */
    
    //Quando diventerà true, dirà a chi sta aspettando pacchetti di smettere di aspettare e chiudere tutto.
    bool spegnimento = false; 

public:
    //metodo per il capture_thread (producer):  Inserisce un nuovo pacchetto nella coda
    void push(std::unique_ptr<packet_inspector::NetworkPacket> pacchetto);

    //metodo per il forwarder_thread (consumer) : estrae il primo pacchetto disponibile, lo rimuove dalla coda 
    std::unique_ptr<packet_inspector::NetworkPacket> pop();
     //Restituisce un puntatore nullo se la coda è in fase di spegnimento

    //Cambia la variabile spegnimento in true e sveglia eventuali thread addormentati.
    void stop();
};

