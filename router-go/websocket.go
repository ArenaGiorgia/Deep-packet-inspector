/*
Creare un canale bidirezionale persistente con il browser.
A differenza delle normali chiamate HTTP, il WebSocket rimane aperto.
In questo modo, appena il nostro sistema intercetta un pacchetto,
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
	clients  map[*websocket.Conn]bool // Mappa dei client attivi
	mutex    sync.Mutex               // Mutex per garantire la Thread-Safety e prevenire Data Race
	upgrader websocket.Upgrader       // Upgrader per trasformare una connessione HTTP in WebSocket
}

// CreaHubDashboard istanzia la dashboard grafica, la inizializza e restituisce un puntatore a un nuovo WebSocketHub.
// L'uso dei puntatori evita la copia costosa dell'intera struttura e del suo Mutex interno, garantendo l'accesso condiviso.
func CreaHubDashboard() *WebSocketHub {
	return &WebSocketHub{
		// 1. Inizializziamo la mappa tramite la funzione integrata make.
		// Come specificato nelle regole di Go, make è essenziale per allocare e inizializzare strutture dati complesse come le mappe.
		clients: make(map[*websocket.Conn]bool),

		// 2. Inizializziamo la configurazione dell'Upgrader direttamente qui.
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true }, // Accetta origini incrociate per lo sviluppo
		},
	}
}

// AccettaConnessioneWeb gestisce le nuove connessioni in ingresso.
// Essendo definita con un Pointer Receiver (*WebSocketHub), ha la capacità di modificare lo stato interno dell'hub (la mappa dei client).
func (hub *WebSocketHub) AccettaConnessioneWeb(w http.ResponseWriter, r *http.Request) {
	// Usiamo l'upgrader incapsulato nell'istanza dell'hub per elevare la connessione HTTP a WebSocket.
	conn, err := hub.upgrader.Upgrade(w, r, nil)
	if err != nil {
		fmt.Println("Errore nell'upgrade del WebSocket:", err)
		return
	}

	// Acquisiamo il lock per proteggere la mappa da accessi concorrenti da parte di altre goroutine.
	hub.mutex.Lock()
	hub.clients[conn] = true
	hub.mutex.Unlock()

	fmt.Println("[WEBSOCKET] Nuovo client connesso alla Dashboard!")

	// Il blocco defer garantisce la pulizia sicura alla fine della funzione.
	// Accoda l'esecuzione in modo che la disconnessione avvenga solo un attimo prima che la funzione termini.
	defer func() {
		hub.mutex.Lock()
		delete(hub.clients, conn)
		hub.mutex.Unlock()
		conn.Close()
		fmt.Println("[WEBSOCKET] Client disconnesso e rimosso dalla mappa in sicurezza.")
	}()

	// Goroutine di ascolto per mantenere viva la connessione.
	for {
		_, _, err := conn.ReadMessage()
		if err != nil {
			// Se c'è un errore (es. browser chiuso), usciamo dal ciclo.
			// Il blocco defer soprastante scatterà automaticamente in ordine LIFO.
			break
		}
	}
}

// DiffondiAllarme invia un alert di sicurezza a tutti i browser connessi in tempo reale.
// Richiede un array di byte in ingresso.
func (hub *WebSocketHub) DiffondiAllarme(message []byte) {
	hub.mutex.Lock()
	
	// Il blocco defer assicura il rilascio del Lock in ogni caso di uscita dalla funzione,
	// prevenendo potenziali deadlock dell'intero hub.
	defer hub.mutex.Unlock()

	// Iteriamo sulla mappa dei client usando il costrutto for range.
	for client := range hub.clients {
		err := client.WriteMessage(websocket.TextMessage, message)
		if err != nil {
			// Chiudiamo il socket malfunzionante.
			client.Close()
			// Eliminiamo la chiave dalla mappa usando la funzione built-in delete.
			delete(hub.clients, client)
		}
	}
}