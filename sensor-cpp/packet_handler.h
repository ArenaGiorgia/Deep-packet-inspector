//definisco nel file .h la classe con i metodi che andrò a richiamare nel file packet_handler.cpp
#pragma once  //direttiva moderna che impedisce al compilatore di includere questo file due volte per errore
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

// Includiamo il contratto generato da Protobuf
#include "packet_data.pb.h"

class PacketQueue {
private:
    // La struttura dati reale che conterrà i pacchetti in fila
    std::queue<std::unique_ptr<packet_inspector::NetworkPacket>> queue_;
    
    // Semafori per evitare che due thread tocchino la coda contemporaneamente
    std::mutex mutex_;
    std::condition_variable cond_var_; 
    /*Invece di far girare il Forwarder a vuoto chiedendo alla coda "Ci sono pacchetti?
     Ci sono pacchetti?" (sprecando il 100% della CPU), questa variabile addormenta il Forwarder.
      Verrà risvegliato istantaneamente solo quando il cuoco grida "Piatto pronto!" 
      (tramite il metodo push).
    */
    
    // Bandierina per lo spegnimento sicuro (Graceful Shutdown)
    bool stop_flag_ = false;

public:
    // Metodo per il PRODUCER (il cuoco): Inserisce un nuovo pacchetto nella coda
    void push(std::unique_ptr<packet_inspector::NetworkPacket> packet);

    // Metodo per il CONSUMER (il cameriere): Preleva il pacchetto più vecchio
    // Restituisce un puntatore nullo se la coda è in fase di spegnimento
    std::unique_ptr<packet_inspector::NetworkPacket> pop();

    // Metodo per sbloccare tutti e chiudere il programma pulitamente
    void stop();
};