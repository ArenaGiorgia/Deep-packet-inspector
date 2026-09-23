/*Implementa una coda thread-safe utilizzando i semafori (std::mutex e std::condition_variable) 
per sincronizzare la lettura e la scrittura. Sfrutta i puntatori intelligenti (std::unique_ptr) per
 trasferire i blocchi di memoria tra i thread senza causare memory leak o crash.*/

#include "packet_handler.h" //richiamiamo la classe codaPacchetti 


//producer: inserisce i pacchetti nella coda
void CodaPacchetti::push(std::unique_ptr<packet_inspector::NetworkPacket> pacchetto) {
    
    //Mettiamo in pausa gli altri thread finché non abbiamo finito di inserire
    std::lock_guard<std::mutex> blocco_esclusivo(mutex); 
 

   //La coda è piena perche il consumer è lento oppure è disconnesso 
   if (coda.size() >= capacita_massima) { 
    //buttiamo via il pacchetto più vecchio in testa per privelegiare i dati recenti. 
    coda.pop();      
    }

    //Spostiamo il pacchetto dentro la nostra coda 
    coda.push(std::move(pacchetto));
    
    //Svegliamo il consumer per avvisarlo che c'è un nuovo pacchetto pronto 
    condizione.notify_one();
}



//consumer: estrae i pacchetti dalla coda
std::unique_ptr<packet_inspector::NetworkPacket> CodaPacchetti::pop() {


    //Simile a lock_guard, ma più flessibile. Puo essere sbloccato quando il producer inserisce un nuovo pacchetto in coda
    //unique_lock lavora in coppia con la variabile "condizione"
    std::unique_lock<std::mutex> blocco(mutex); 

    
    //Mettiamo in pausa il thread finché la coda è vuota. 
    while (coda.empty() && !spegnimento) {  
        condizione.wait(blocco); //apre il lucchetto unique_lock e aspetta che arriva notify_one
    }

    //Se stiamo spegnendo il programma e non ci sono più pacchetti restituiamo il puntatore a nullo
    if (spegnimento && coda.empty()) {
        return nullptr;
    }

    //togliamo il pacchetto dalla coda e lo mettiamo in pacchetto estratto.
    auto pacchetto_estratto = std::move(coda.front()); //coda.front legge il primo pacchetto in cima alla fila senza rimuoverlo
    
    //rimuoviamo l elemento in cima alla fila
    coda.pop(); 

    return pacchetto_estratto;
}

//per spegnere tutto in maniera sicura
void CodaPacchetti::stop() {
    
    //blocchiamo la coda per sicurezza
    std::lock_guard<std::mutex> blocco_esclusivo(mutex);
    
    //settiamo a vero il flag dello spegnimento 
    spegnimento = true;
    
    //Svegliamo tutti i thread in ascolto non solo uno come prima con notify_one.
    condizione.notify_all();
}