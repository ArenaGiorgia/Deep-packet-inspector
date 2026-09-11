/*
 Fa esattamente quattro cose:
    Gestione Segnali: Crea un canale per intercettare i segnali del sistema operativo .
    Setup Rete (Ascolto): Apre la porta UDP 9000 mettendosi in attesa dei dati provenienti dal livello Edge .
    Core CSP (Communicating Sequential Processes): Crea il packetChan, un Canale bufferizzato (in grado di tenere in memoria 100 pacchetti) che farà da canale di comunicazione sicuro tra i thread.
    Graceful Shutdown: Usa il costrutto defer per programmare la chiusura pulita del socket e del canale un attimo prima che il programma finisca.
*/
package main

import (
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"syscall"

	"dpi/router/router"
	"google.golang.org/protobuf/proto"
)
func main() {
	fmt.Println("Avvio Router Go (Livello 4 - TCP Server per Sensore C++)...")

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Ascolto TCP sulla porta 8080 concordata con il sensore C++[cite: 7, 8]
	listener, err := net.Listen("tcp", "127.0.0.1:8080")
	if err != nil {
		fmt.Printf("Errore nell'avvio del listener TCP: %v\n", err)
		return
	}
	// [PATTERN CSP]: Creiamo un canale bufferizzato capace di contenere 100 pacchetti in coda
	packetChan := make(chan *router.NetworkPacket, 100) //Il numero 100 significa che è un Canale Bufferizzato: può immagazzinare fino a 100 pacchetti in coda prima di bloccarsi. Se per un attimo arrivano tantissimi pacchetti dalla rete e il Worker è occupato, i pacchetti non vanno persi ma si mettono in coda nel buffer.

	// Avviamo la Goroutine Worker (Consumer) passandogli il canale
	/*Il suo unico scopo è stare in ascolto dall'altra parte del canale (packetChan),
	 prelevare i pacchetti smistati dal Producer, filtrarli e inviarli a Python.*/
	go StartWorker(packetChan)

	defer func() {
		fmt.Println("\n[DEFER] Chiusura sicura del listener e dei canali in corso...")
		close(packetChan) // Chiudiamo il canale per spegnere elegantemente il Worker
		listener.Close()  // Chiudiamo il socket di ascolto TCP principale
	}()

	// Goroutine di Ricezione (Producer)
	/*Questa Goroutine agisce da Produttore: vive in un ciclo infinito, estrae i byte dalla porta 9000, li decodifica usando la funzione di Protobuf (proto.Unmarshal) e, tramite l'operatore freccia <-, infila il pacchetto pulito all'interno del canale, tornando immediatamente in ascolto.*/
	go func() {
		for {
			conn, err := listener.Accept()
			if err != nil {
				return 
			}

			// Gestisce la connessione del client in una goroutine dedicata
			go handleClient(conn, packetChan)
		}
	}()

	<-sigChan
	/*L'operatore freccia senza nulla a sinistra significa "aspetta fermo qui finché non esce qualcosa dal canale sigChan".
Le due Goroutine (Producer e Consumer) continueranno a lavorare in background all'infinito. Il programma si sbloccherà e si spegnerà (attivando il defer) solo quando premerai CTRL+C e il sistema operativo invierà il segnale fatidico in quel canale.*/
	fmt.Println("\n[SEGNALE] Inizio procedura di Graceful Shutdown...")
}

// Funzione per leggere lo stream di byte TCP inviato dal C++
func handleClient(conn net.Conn, packetChan chan<- *router.NetworkPacket) {
	defer conn.Close()
	buffer := make([]byte, 4096)

	for {
		n, err := conn.Read(buffer)
		if err != nil {
			if err != io.EOF {
				fmt.Printf("Errore di lettura TCP: %v\n", err)
			}
			break
		}

		packet := &router.NetworkPacket{}
		err = proto.Unmarshal(buffer[:n], packet)
		if err != nil {
			continue
		}

		// Inserisce il pacchetto nel canale CSP sicuro
		packetChan <- packet
	}
}