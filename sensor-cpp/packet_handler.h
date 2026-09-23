#pragma once         //impedisce al compilatore di includere file header .h due volte per errore
#include <queue>    //per gestire le code (push,pop,empty,front)
#include <mutex>   //per gestire i thread (lock_guard, unique_lock )
#include <condition_variable> //meccanismo avanzato dei thread (wait, notify_one,notify_all)
#include <memory>  //introduce lo unique pointer
#include "packet_data.pb.h" //per il protobuffer 

class CodaPacchetti {
private:
    /*dichiariamo una coda (std::queue) che contiene oggetti esclusivi (unique_ptr) di tipo 
    NetworkPacket della classe generata dal file .proto */ 
    std::queue<std::unique_ptr<packet_inspector::NetworkPacket>> coda;
    
    //metto un limite di 10 mila pacchetti per prevenire memory leak se il consumer si ferma 
    const size_t capacita_massima = 10000;

    //Semafori per evitare che due thread accedano alla coda contemporaneamente
    std::mutex mutex;
    
    /*il consumer verrà risvegliato istantaneamente solo quando il pacchetto è pronto tramite il metodo 
    push che fara poi un notify_one */ 
    std::condition_variable condizione; 
   
    
    //Quando diventa true non si aspettano pacchetti e si chiude tutto.
    bool spegnimento = false; 

public:
    //metodo del prodcucer per l'inserimento in coda 
    void push(std::unique_ptr<packet_inspector::NetworkPacket> pacchetto);

    //metodo per il consumer cje estrae il primo pacchetto disponibile e lo rimuove dalla coda 
    std::unique_ptr<packet_inspector::NetworkPacket> pop();

    //Cambia la variabile spegnimento in true e sveglia eventuali thread addormentati.
    void stop();
};

