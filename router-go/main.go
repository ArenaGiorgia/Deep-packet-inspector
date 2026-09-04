package main

import (
	"fmt"
	"net"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	fmt.Println("Avvio Router Go (Livello 4)...")

	// Creiamo un canale CSP per intercettare i segnali del sistema operativo (es. CTRL+C)
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Apriamo una porta UDP locale per ascoltare i pacchetti
	addr := net.UDPAddr{Port: 9000, IP: net.ParseIP("127.0.0.1")}
	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		fmt.Printf("Errore di connessione: %v\n", err)
		return
	}

	//  Il costrutto DEFER.
	// Questa funzione anonima viene messa in pausa e verrà eseguita 
	// automaticamente un istante prima che il programma termini.
	defer func() {
		fmt.Println("\n[DEFER] Esecuzione pulizia: Chiusura sicura del socket. Nessun dato in transito andrà perso.")
		conn.Close()
	}()

	fmt.Println("In ascolto su UDP porta 9000...")

	// Goroutine: Mettiamo l'ascolto dei pacchetti su un thread leggero parallelo
	go func() {
		buffer := make([]byte, 1024)
		for {
			n, _, err := conn.ReadFromUDP(buffer)
			if err != nil {
				return // Se il socket si chiude, la goroutine termina silenziosamente
			}
			fmt.Printf("Ricevuti %d byte di dati dal livello Edge\n", n)
		}
	}()

	// Il main thread si blocca qui, mettendosi in ascolto del canale dei segnali
	<-sigChan
	fmt.Println("\n[SEGNALE] Ricevuta interruzione (CTRL+C). Inizio procedura di Graceful Shutdown...")
}