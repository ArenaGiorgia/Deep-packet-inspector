package main

import (
	"fmt"
	"net"

	"dpi/router/router"
	"google.golang.org/protobuf/proto"
)

/* 
AvviaSmistatorePacchetti è la Goroutine "Consumer".
Il simbolo <- prima di chan la trasforma in un Canale di sola lettura (Receive-only Channel). 
Garantisce a livello di compilazione che questa funzione possa solo leggere i dati,
prevenendo race condition con il Producer.

[DEPENDENCY INJECTION]: Questa funzione non crea risorse proprie. Riceve dall'esterno
il canale CSP, l'Hub WebSocket e la connessione UDP già instradata, isolando perfettamente la logica.
E' una funzione che gira in background e si occupa di smistare i pacchetti ricevuti dal sensore C++ verso l'Analizzatore Python.
*/
func AvviaSmistatorePacchetti(packetChan <-chan *router.NetworkPacket, hub *WebSocketHub, pythonConn *net.UDPConn) { 
	fmt.Println("[WORKER] Avviato e in attesa di pacchetti dal canale...")

	/* 
	Il for-range su un canale crea un ciclo bloccante ed efficiente.
	La Goroutine si mette in pausa senza consumare CPU. Il ciclo si romperà
	automaticamente e in modo pulito solo quando invocheremo close(packetChan).
	*/
	for packet := range packetChan {
		
		// Logica di Routing: filtriamo il traffico utile
		if packet.Protocol == "TCP" {
			fmt.Printf("[ROUTER] Traffico TCP da %s inoltrato a Python.\n", packet.SourceIp)
			
			// 1. Ri-serializziamo il pacchetto in binario Protobuf
			data, err := proto.Marshal(packet)
			if err == nil {
				// 2. Inoltriamo fisicamente a Python usando il socket iniettato dal main!
				pythonConn.Write(data) 
			}

			// 3. Diffondiamo una notifica in tempo reale alla Dashboard Web tramite l'Hub
			messaggioWeb := fmt.Sprintf("Analisi TCP in corso per IP: %s", packet.SourceIp)
			hub.DiffondiAllarme([]byte(messaggioWeb))

		} else {
			fmt.Printf("[ROUTER] Scartato pacchetto %s (Traffico non analizzabile)\n", packet.Protocol)
		}
	}
}