/*Creare un canale bidirezionale persistente con il browser.
 A differenza delle normali chiamate HTTP , il WebSocket rimane aperto.
In questo modo, appena il nostro sistema intercetta un pacchetto ,
lo manda istantaneamente alla pagina web senza che l'utente debba ricaricarla.
*/
package main

import (
	"fmt"
	"net/http"
	"sync"
	"github.com/gorilla/websocket"
)

// [HUB] Struttura dati per tenere traccia di tutti i browser web connessi alla dashboard
type WebSocketHub struct {
	clients map[*websocket.Conn]bool // Mappa dei client attivi
	mutex   sync.Mutex               // Mutex per garantire la Thread-Safety (previene le race condition)
	upgrader websocket.Upgrader	// Upgrader per trasformare una connessione HTTP in WebSocket
}
// Crea la dashboard grafica, la inizializza e restituisce un nuovo WebSocketHub
func CreaHubDashboard() *WebSocketHub {
	return &WebSocketHub{
		// 1. Inizializziamo la mappa tramite make
		clients: make(map[*websocket.Conn]bool),
		
		// 2. Inizializziamo la configurazione dell'Upgrader direttamente qui
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true }, // Accetta origini incrociate per lo sviluppo
		},
	}
}

// Gestisce le nuove connessioni in ingresso
func (hub *WebSocketHub) AccettaConnessioneWeb(w http.ResponseWriter, r *http.Request) {
	// Usiamo l'upgrader incapsulato nell'istanza dell'hub (hub.upgrader)
	conn, err := hub.upgrader.Upgrade(w, r, nil)
	if err != nil {
		fmt.Println("Errore nell'upgrade del WebSocket:", err)
		return
	}

	// Lock per proteggere la mappa da accessi concorrenti
	hub.mutex.Lock()
	hub.clients[conn] = true
	hub.mutex.Unlock()

	fmt.Println("[WEBSOCKET] Nuovo client connesso alla Dashboard!")

	// Goroutine di ascolto per mantenere viva la connessione
	for {
		_, _, err := conn.ReadMessage()
		if err != nil {
			hub.mutex.Lock()
			delete(hub.clients, conn)
			hub.mutex.Unlock()
			conn.Close()
			fmt.Println("[WEBSOCKET] Client disconnesso.")
			break
		}
	}
}

// DiffondiAllarme invia un alert di sicurezza a tutti i browser connessi.
func (hub *WebSocketHub) DiffondiAllarme(message []byte) {
	hub.mutex.Lock()
	defer hub.mutex.Unlock() // Garantisce lo sblocco in ogni caso d'uscita

	for client := range hub.clients {
		err := client.WriteMessage(websocket.TextMessage, message)
		if err != nil {
			client.Close()
			delete(hub.clients, client)
		}
	}
}