package main

import (
	"fmt"
	"net"
	"dpi/router/router" 
	"google.golang.org/protobuf/proto"
)

// StartWorker è la Goroutine "Consumer".
/*simbolo <- prima di chan.
In Go, questo trasforma il parametro in un Canale di sola lettura (Receive-only Channel). 
 Garantisce a livello di compilazione che questa funzione (il Consumer) possa solo leggere i dati dal canale, impedendole di scriverci dentro accidentalmente e prevenendo così pericolose race condition con il Producer.*/
func StartWorker(packetChan <-chan *router.NetworkPacket) {
	fmt.Println("[WORKER] Avviato e in attesa di pacchetti dal canale...")

	// Apriamo una connessione UDP in uscita verso l'Analizzatore Python (Porta 9001)
	pythonAddr := &net.UDPAddr{Port: 9001, IP: net.ParseIP("127.0.0.1")}
	/* main.go apriva una porta in ascolto (ListenUDP), 
	qui creiamo una connessione in uscita (DialUDP) verso la porta 9001, 
	dove sappiamo che il nostro Analizzatore Python ci sta aspettando.*/
	conn, err := net.DialUDP("udp", nil, pythonAddr)
	if err != nil {
		fmt.Println("Errore di connessione verso Python:", err)
		return
	}
	defer conn.Close() // Chiusura sicura anche per il socket in uscita

	// Resta in ascolto infinito sul Canale
	/*applicare range su un canale crea un ciclo bloccante ed efficiente.
La Goroutine si mette in pausa (senza consumare CPU) finché non arriva un pacchetto nel canale. 
Appena arriva, lo estrae, esegue il blocco di codice sottostante, e si rimette in pausa ad aspettare il successivo. 
Il ciclo si romperà automaticamente solo quando nel main.go invocheremo il comando close(packetChan) durante lo spegnimento.*/
	for packet := range packetChan {
		
		// Logica di Routing: filtriamo
		if packet.Protocol == "TCP" {
			fmt.Printf("[ROUTER] Traffico TCP da %s inoltrato a Python.\n", packet.SourceIp)
			
			// Ri-serializziamo in binario Protobuf
			data, err := proto.Marshal(packet)
			if err == nil {
				// Inoltriamo fisicamente a Python!
				conn.Write(data) 
			}
		} else {
			fmt.Printf("[ROUTER] Scartato pacchetto %s (Traffico non analizzabile)\n", packet.Protocol)
		}
	}
}