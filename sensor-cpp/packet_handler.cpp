/*(La Coda): L'area di transito sicura. Implementa una coda thread-safe utilizzando 
i semafori (std::mutex e std::condition_variable) per sincronizzare la lettura e la scrittura. 
Sfrutta i puntatori intelligenti (std::unique_ptr) per trasferire i blocchi di memoria tra i thread 
senza causare memory leak o crash.*/

#include "packet_handler.h" //richiamiamo la classe codaPacchetti 


// PRODUCER: Inserisce i pacchetti nella coda
void CodaPacchetti::push(std::unique_ptr<packet_inspector::NetworkPacket> packet) {
    
    //Mettiamo in pausa gli altri thread finché non abbiamo finito di inserire
    std::lock_guard<std::mutex> blocco_esclusivo(mutex); 
   // un blocco intelligente basato su una regola chiamata RAII (Resource Acquisition Is Initialization).
   // Appena questa riga viene eseguita, il mutex viene bloccato. 
   //Il blocco si sblocca da solo non appena la funzione termina.
    
    //Spostiamo il pacchetto dentro la nostra coda (usiamo move per non copiare dati pesanti)
    coda.push(std::move(packet));
    
    //Svegliamo il consumer per avvisarlo che c'è un nuovo pacchetto pronto
    condizione.notify_one();
}



//CONSUMER: Preleva i pacchetti dalla coda
std::unique_ptr<packet_inspector::NetworkPacket> CodaPacchetti::pop() {


    //Simile a lock_guard, ma più flessibile. Può essere bloccato e sbloccato manualmente a piacimento
    //(unique_lock infatti lavora in coppia con la variabile "condizione")
    std::unique_lock<std::mutex> blocco(mutex); 

    
    //Mettiamo in pausa il thread finché la coda è vuota. 
    //Si sveglia da solo se arriva un pacchetto o se premiamo il tasto di spegnimento.
    while (coda.empty() && !spegnimento) {
        condizione.wait(blocco);
    }

    //Se stiamo spegnendo il programma e non ci sono più pacchetti, restituiamo nulla.
    if (spegnimento && coda.empty()) {
        return nullptr;
    }

    //Se siamo qui, significa che c'è un pacchetto. Lo prendiamo, lo togliamo alla coda
    //e lo mettiamo in pacchetto estratto.
    auto pacchetto_estratto = std::move(coda.front()); 
    //coda.front legge il primo pacchetto in cima alla fila senza rimuoverlo.
    
    coda.pop(); //Distrugge l'elemento in cima alla fila. 
    //Ecco perché prima dobbiamo salvare il pacchetto in pacchetto_estratto tramite move

    return pacchetto_estratto;
}

//Tasto di emergenza: spegne tutto in maniera pulita
void CodaPacchetti::stop() {
    
    //Blocchiamo la coda per sicurezza
    std::lock_guard<std::mutex> blocco_esclusivo(mutex);
    
    //settiamo a vero il flag dello spegnimento 
    spegnimento = true;
    
    //Svegliamo gli eventuali thread rimasti addormentati in attesa
    //A differenza di notify_one (che sveglia un thread a caso), questo sveglia tutti i thread in ascolto.
    // Costringe tutti i consumatori a svegliarsi,
    condizione.notify_all();
}