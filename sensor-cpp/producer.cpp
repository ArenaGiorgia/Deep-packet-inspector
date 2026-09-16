/*(Il Producer): Utilizza la libreria di basso livello libpcap per mettere la scheda di rete in ascolto.
 Il suo unico compito è intercettare i byte grezzi in transito il più velocemente possibile e riversarli
  nella coda di memoria condivisa, senza perdere cicli CPU per analizzarli.*/
#include "producer.h"
#include <iostream>
#include <netinet/ip.h>    // Per leggere la struttura dati dell'header IP
#include <netinet/tcp.h>   // Per leggere la struttura dati dell'header TCP (PORTE)
#include <arpa/inet.h>     // Per convertire l'IP in formato stringa leggibile (inet_ntop)


//costruttore MIL 
CatturaTraffico::CatturaTraffico(CodaPacchetti& coda_condivisa, const std::string& nome_interfaccia)
    : coda(coda_condivisa), interfaccia(nome_interfaccia), sessione(nullptr), attivo(false) {
    // Inizializziamo le variabili. All'inizio la sessione è vuota e non siamo attivi.
}

//distruttore
CatturaTraffico::~CatturaTraffico() {
   
    //chiamiamo la funzione ferma() per sicurezza, evitando che il thread rimanga "orfano".
    ferma(); 
}


//Accensione 
void CatturaTraffico::avvia() {
    
    //Buffer fornito da pcap dove scriverà il testo di eventuali errori
    char buffer_errori[PCAP_ERRBUF_SIZE]; 

    //Apriamo la sessione di ascolto direttamente sul kernel.
    //nome scheda, c_str() Converte la stringa C++ in una stringa C-style compatibile con la libreria.
    // byte massimi da leggere (65535 "MTU" massimo, ovvero la dimensione massima in byte che un pacchetto IP può avere) 
    //modalità promiscua (1), costringe la scheda di rete a leggere tutto il traffico passante. 
    //timeout in ms (1000), buffer errori
    sessione = pcap_open_live(interfaccia.c_str(), 65535, 1, 1000, buffer_errori);

    if (sessione == nullptr) {
        // MODIFICA: Usiamo std::endl per forzare l'output immediato in Docker
        std::cerr << "Errore critico: " << buffer_errori << std::endl;
        return; // Se fallisce, interrompiamo tutto
    }

    
    //  FILTRO BPF (Il "Paraocchi" per evitare il Loop Infinito)
    struct bpf_program filtro;
    // Diciamo alla scheda di rete di NON ascoltare le porte usate dai nostri microservizi (Go e Python).
    // In questo modo ignoriamo i nostri stessi allarmi ed evitiamo che C++ li rimetta in circolo!
    std::string regola = "not port 8080 and not port 9001 and not port 9002";
    
    // Compiliamo e applichiamo la regola direttamente nel kernel
    if (pcap_compile(sessione, &filtro, regola.c_str(), 0, PCAP_NETMASK_UNKNOWN) != -1) {
        pcap_setfilter(sessione, &filtro);
    }
    // ==========================================

    //per avviare il ciclo while continuo 
    attivo = true;

    //Lanciamo il lavoratore (thread) in background, dicendogli di eseguire la funzione "cattura"
    thread_cattura = std::thread(&CatturaTraffico::cattura, this);
}


//spegnimento 
void CatturaTraffico::ferma() {
    if (!attivo) return; // Se siamo già fermi usciamo
    
    //Questo farà uscire il thread dal suo ciclo "while"
    attivo = false; 

    //Per la sincronizzazione 
    //Aspettiamo che il thread finisca l'ultimo giro e si riunisca in modo pulito al programma principale
    if (thread_cattura.joinable()) {
        thread_cattura.join();
    }

    //Chiudiamo ufficialmente la sessione col sistema operativo
    if (sessione != nullptr) {
        pcap_close(sessione);
        sessione = nullptr;
    }
}


//Producer 
void CatturaTraffico::cattura() {
    struct pcap_pkthdr* intestazione_pcap; // Metadati (esempio: quando è stato catturato e quanto è lungo)
    const u_char* byte_grezzi;             // Il contenuto fisico del pacchetto

    //fin quando non stoppiamo 
    while (attivo) {
        // Chiediamo alla scheda di rete: "C'è un pacchetto?"
        int risultato = pcap_next_ex(sessione, &intestazione_pcap, &byte_grezzi);

        if (risultato == 1) { 
            // 1 significa che abbiamo catturato un pacchetto con successo!
            
            // Creiamo un nuovo pacchetto vuoto usando i puntatori intelligenti
            auto pacchetto_nuovo = std::make_unique<packet_inspector::NetworkPacket>();
            
            // Calcoliamo il timestamp in millisecondi per misurare poi le latenze
            int64_t timestamp = (intestazione_pcap->ts.tv_sec * 1000LL) + (intestazione_pcap->ts.tv_usec / 1000);
            pacchetto_nuovo->set_timestamp_ms(timestamp);

            // ==========================================
            // PARSING DINAMICO ED ESTRAZIONE DATI
            // ==========================================
            // Assumendo un frame Ethernet (14 byte), andiamo a leggere l'header IP
            if (intestazione_pcap->caplen >= 34) { // Controlliamo che il pacchetto sia abbastanza grande
                
                // ==========================================
                // NUOVO: CONTROLLO ETHERTYPE (Ignora ARP/IPv6)
                // Il byte 12 e 13 del frame Ethernet indicano il protocollo di rete
                // ==========================================
                uint16_t ethertype = (byte_grezzi[12] << 8) | byte_grezzi[13];
                
                if (ethertype == 0x0800) { // 0x0800 è lo standard universale per IPv4
                
                    // 1. ESTRAIAMO GLI INDIRIZZI IP E LA LUNGHEZZA DELL'HEADER IP
                    // Facciamo un "salto" di 14 byte (grandezza dell'header Ethernet) 
                    // per atterrare direttamente sui dati del livello IP
                    const struct ip* ip_header = (struct ip*)(byte_grezzi + 14);
                    int ip_header_len = ip_header->ip_hl * 4;

                    // Prepariamo i contenitori per le stringhe degli IP
                    char src_ip[INET_ADDRSTRLEN];
                    char dst_ip[INET_ADDRSTRLEN];
                    
                    // Convertiamo gli IP grezzi in testo (es. "192.168.1.5")
                    inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);

                    // Assegniamo gli IP al pacchetto Protobuf
                    pacchetto_nuovo->set_source_ip(src_ip);
                    pacchetto_nuovo->set_dest_ip(dst_ip);

                    // 2. ESTRAIAMO IL PROTOCOLLO DI TRASPORTO E LE PORTE
                    // Byte 23 indica il protocollo di trasporto 
                    uint8_t protocollo_ip = byte_grezzi[23];
                    int payload_offset = 14 + ip_header_len; // Punto di partenza dopo Ethernet e IP
                    
                    if (protocollo_ip == 6) {
                        pacchetto_nuovo->set_protocol("TCP");
                        
                        // Leggiamo la struttura TCP per estrarre le porte sorgente e destinazione
                        const struct tcphdr* tcp_header = (struct tcphdr*)(byte_grezzi + payload_offset);
                        pacchetto_nuovo->set_source_port(ntohs(tcp_header->th_sport));
                        pacchetto_nuovo->set_dest_port(ntohs(tcp_header->th_dport));
                        
                        // Saltiamo anche l'header TCP per arrivare finalmente ai byte del payload puro
                        payload_offset += tcp_header->th_off * 4;

                    } else if (protocollo_ip == 17) {
                        pacchetto_nuovo->set_protocol("UDP");
                        payload_offset += 8; // L'header UDP pesa sempre 8 byte
                    } else if (protocollo_ip == 1) {
                        pacchetto_nuovo->set_protocol("ICMP");
                    } else {
                        pacchetto_nuovo->set_protocol("ALTRO");
                    }

                    // 3. ESTRATTO IL VERO E PROPRIO PAYLOAD 
                    int payload_size = intestazione_pcap->caplen - payload_offset;

                    // Salviamo i byte grezzi (che poi Python analizzerà) nel campo Protobuf "raw_payload"
                    // Ora stiamo salvando SOLO il payload pulito, senza le intestazioni di rete!
                    if (payload_size > 0) {
                        pacchetto_nuovo->set_raw_payload(byte_grezzi + payload_offset, payload_size);
                    } else {
                        pacchetto_nuovo->set_raw_payload("", 0);
                    }

                    // Stampa a video il lavoro del Sensore C++ (Log in tempo reale)
                    // MODIFICA: std::endl forza la stampa in Docker Desktop
                    std::cout << "[SENSORE C++] Sniffato pacchetto " << pacchetto_nuovo->protocol() 
                              << " da " << src_ip << ":" << pacchetto_nuovo->source_port() 
                              << " (" << payload_size << " byte di payload)" << std::endl;
                              
                } else {
                    // Non è un pacchetto IPv4 (potrebbe essere ARP o IPv6 interno di Docker)
                    // Lo ignoriamo elegantemente senza scatenare errori
                    pacchetto_nuovo->set_protocol("IGNORATO");
                    pacchetto_nuovo->set_raw_payload("", 0);
                }

            } else {
                pacchetto_nuovo->set_protocol("SCONOSCIUTO");
                
                // Salviamo i byte grezzi (che poi Python analizzerà) nel campo Protobuf "raw_payload"
                // Nel caso in cui non riusciamo a decodificare le intestazioni, copiamo tutto il pacchetto
                pacchetto_nuovo->set_raw_payload(byte_grezzi, intestazione_pcap->caplen);
            }

            //mandiamo il pacchetto nuovo alla coda 
            // MODIFICA PER SICUREZZA: Evitiamo di ingolfare Go e Python con i pacchetti "IGNORATI"
            if (pacchetto_nuovo->protocol() != "IGNORATO") {
                coda.push(std::move(pacchetto_nuovo));
            }
        }
        // Se risultato è 0 (timeout scaduto) il ciclo riparte da capo.
        // Se è negativo, c'è un errore ma il ciclo continuerà o si fermerà se modifichiamo "attivo".
    }
}