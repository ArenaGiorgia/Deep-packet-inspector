/*Il consumer estrae in modo asincrono i pacchetti dalla coda gestita da packet_handler.cpp. Legge gli 
header di base (IP e porte), popola la struttura Protobuf, serializza i dati in formato binario
compresso e li spara via socket di rete verso il microservizio in Go.*/

#include "consumer.h"
#include <iostream> 
#include <sys/socket.h>  //librerie standard di Linux per gestire le connessioni di rete della socket
#include <arpa/inet.h>   //tradurre indirizzo IP e porta dal formato leggibile dal computer locale a quello standard richiesto dalla rete.
#include <unistd.h>
#include <netdb.h>      //fornisce gethostbyname per risolvere nomi di dominio (come "router") in IP
#include <cstring>      //per gestire la memoria (memset, memcpy)
#include <chrono>       //per misurare i tempi di attesa (sleep)
#include <thread>       //per mettere in pausa il thread durante i tentativi di connessione
#include <csignal>      //per la gestione del segnale SIGPIPE

//costruttore 
InoltroTraffico::InoltroTraffico(CodaPacchetti& coda_condivisa, const std::string& ip_destinazione, int porta_destinazione)
    : coda(coda_condivisa), indirizzo_ip(ip_destinazione), porta(porta_destinazione), socket_fd(-1), attivo(false) {
    //i file descriptor validi partono da 0 in su. -1 nessuna connessione
}

//distruttore 
InoltroTraffico::~InoltroTraffico() {
    ferma(); //spegne tutto se l'oggetto viene distrutto
}

//funzione per leggere lo stato in modo sicuro 
bool InoltroTraffico::blocco_sicuro() {
    std::lock_guard<std::mutex> blocco(mutex_stato);
    return attivo;
}

//creazione canale di comunicazione 
bool InoltroTraffico::connetti_socket() {
   
    // AF_INET: IPv4 e SOCK_STREAM : connessione affidabile e 0 di default come combinazione allora TCP 
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return false; // Creazione fallita
    }

    //Prepariamo l'indirizzo a cui mandare a Go
    struct sockaddr_in indirizzo_server;
    memset(&indirizzo_server, 0, sizeof(indirizzo_server)); //Puliamo la memoria da cose che non servono
    indirizzo_server.sin_family = AF_INET;
    
    //htons serve a convertire il numero della porta nel formato leggibile dalla rete Big Endian
    indirizzo_server.sin_port = htons(porta);

    //Se non è un IP numerico (es. "127.0.0.1"), proviamo a risolverlo come nome di container
    if (inet_pton(AF_INET, indirizzo_ip.c_str(), &indirizzo_server.sin_addr) <= 0) {   
        struct hostent* host = gethostbyname(indirizzo_ip.c_str());
        if (host == nullptr) {
            close(socket_fd);
            socket_fd = -1;
            return false; // Impossibile trovare l'host 
        }

        //copiamo l'indirizzo IP binario tradotto
        memcpy(&indirizzo_server.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
    }
    

    //Facciamo partire la chiamata verso Go (handshake a tre vie)
    int risultato_connessione = connect(socket_fd, (struct sockaddr*)&indirizzo_server, sizeof(indirizzo_server));
    if (risultato_connessione < 0) {
        close(socket_fd); // Se Go non risponde chiuduiamo
        socket_fd = -1;  //per indicare "nessuna connessione attiva".
        return false;
    }

    return true; //connessione stabilita
}

//avviamento 
void InoltroTraffico::avvia() {
    
    int tentativi_rimasti = 5;
    while (tentativi_rimasti > 0 && !connetti_socket()) {
        std::cerr << " Consumer di C++: in attesa che Go sia pronto... (" << tentativi_rimasti << " tentativi rimasti)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Aspetta 2 secondi prima di riprovare
        tentativi_rimasti--;
    }

    //assicuriamo che il client Go sia pronto a ricevere
    if (socket_fd == -1) {
        std::cerr << "Impossibile connettersi al microservizio Go." << std::endl;
        return; 
    }
    
    std::cout << " Consumer di C++: connessione verso Go (" << indirizzo_ip << ":" << porta << ") stabilita" << std::endl;

    {
        //acquisiamo il lucchetto prima di accendere il motore
        std::lock_guard<std::mutex> blocco(mutex_stato);
        attivo = true;
    }

    thread_invio = std::thread(&InoltroTraffico::ciclo_di_invio, this);
}

void InoltroTraffico::ferma() {
    {
        //acquisiamo il lucchetto prima di spegnere
        std::lock_guard<std::mutex> blocco(mutex_stato);
        attivo = false;
    }

    //sincronizziamo il thread
    if (thread_invio.joinable()) {
        thread_invio.join();
    }

    //chiudiamo fisicamente la connessione di rete
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}

bool InoltroTraffico::invia_tutto(const char* dati, int lunghezza) {
    
    int byte_inviati = 0; // quanti byte abbiamo già spedito

    // Continuiamo finché non abbiamo inviato tutti i byte richiesti
    while (byte_inviati < lunghezza) {

        //Inviamo il pezzo di dati che manca
        int risultato = send(socket_fd, dati + byte_inviati, lunghezza - byte_inviati, MSG_NOSIGNAL);
        //Linux per default invia il segnale SIGPIPE al processo, che termina il programma immediatamente
        //usiamo MSG_NOSIGNAL per gestire il segnale SIGPIPE e ignorarlo

        if (risultato <= 0) {
            std::cerr << "Errore: invio fallito, connessione con Go probabilmente caduta.\n";
            return false;  
        }

        //aggiorniamo il conteggio di quanto abbiamo inviato finora
        byte_inviati = byte_inviati + risultato;
    }

    return true;
}


void InoltroTraffico::ciclo_di_invio() {
    
    while (blocco_sicuro()) {
        
        //preleviamo il pacchetto, se la coda è vuota, il thread si mette a dormire da solo non consumando CPU.
        auto pacchetto_ricevuto = coda.pop();

        //Se pop() restituisce nullptr, significa che il programma si sta spegnendo
        if (pacchetto_ricevuto == nullptr) {
            break; // Usciamo dal ciclo while
        }

        //trasformiamo l'oggetto Protobuf in una stringa di byte (serializzazione)
        std::string dati_serializzati;
        pacchetto_ricevuto->SerializeToString(&dati_serializzati);


        // Diciamo a Go la dimensione esatta del pacchetto prima di inviarlo.
        // htonl() converte l'intero a 32 bit nel Network Byte Order (Big Endian)
        uint32_t dimensione_pacchetto = htonl(dati_serializzati.size());
        
        //prima inviamo la dimensione del pacchetto (4 byte)
        bool primo_check = invia_tutto((const char*) &dimensione_pacchetto, sizeof(dimensione_pacchetto));

        //se il primo invio è andato bene, inviamo il pacchetto vero e proprio
        bool secondo_check = false;
        if (primo_check) {
            secondo_check = invia_tutto(dati_serializzati.c_str(), dati_serializzati.size());
        }

        //se uno dei due invii è fallito, ci fermiamo
        if (!primo_check || !secondo_check) {
            std::lock_guard<std::mutex> blocco(mutex_stato);
            attivo = false;
            break; //usciamo dal while 
        }
       
    }
}