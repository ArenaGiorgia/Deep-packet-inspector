/*
 COMPOSITION ROOT & GATEWAY DI RETE
 Questo modulo funge da punto di ingresso (main) dell'intera architettura Go.
 Svolge quattro compiti architetturali fondamentali:
 1. Dependency Injection: Istanzia tutte le risorse hardware/software (Socket, Hub UI, Canali)
    e le inietta nelle Goroutine.
 2. Modello CSP (Communicating Sequential Processes): Inizializza il canale bufferizzato
    per la comunicazione sicura tra il thread di rete e il thread di elaborazione.
 3. Multi-Protocol Gateway: Accetta flussi TCP in ingresso (dal livello Edge C++) 
    e instrada i payload elaborati via UDP (all'analizzatore Python).
 4. Graceful Shutdown: Intercetta i segnali del Sistema Operativo per eseguire
    un teardown sicuro e deterministico delle risorse tramite il costrutto defer.
*/
package main

import (
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"

	"dpi/router/router"
	"google.golang.org/protobuf/proto"
)

func main() {
	fmt.Println("Avvio Router Go (Livello 4 - Multi-Protocol Gateway)...")
	// FASE 1: INIZIALIZZAZIONE RISORSE 
	// A. Istanziamo l'Hub WebSocket con il proprio stato incapsulato
	wsHub := CreaHubDashboard()

	// B. Apriamo il socket UDP verso l'Analizzatore Python (Sfrutta il DNS interno di Docker)
	pythonAddr, _ := net.ResolveUDPAddr("udp", "analyzer:9001")
	pythonConn, err := net.DialUDP("udp", nil, pythonAddr)
	if err != nil {
		fmt.Println("Errore apertura connessione UDP verso Python:", err)
		return
	}

	/* 
	C. [PATTERN CSP] Creiamo il canale di comunicazione principale.
	È un canale bufferizzato (capacità 100).
	Se il traffico di rete (TCP) subisce un picco improvviso (burst) e il Consumer (Python)
	è momentaneamente lento, i pacchetti vengono accodati in memoria senza bloccare 
	il Producer (evitando colli di bottiglia a livello socket).
	*/
	packetChan := make(chan *router.NetworkPacket, 100)

	// FASE 2: AVVIO DEI MICROSERVIZI (Asincroni)
	
	// Avvio del server HTTP in una Goroutine dedicata per non bloccare il main
	http.HandleFunc("/ws", wsHub.AccettaConnessioneWeb)
	//  Diciamo a Go di servire i file statici (HTML/JS/CSS) dalla cartella "static"
	http.Handle("/", http.FileServer(http.Dir("./static"))) 
	
	go func() {
		fmt.Println("[DASHBOARD] Server Web in ascolto su http://127.0.0.1:8081")
		http.ListenAndServe(":8081", nil)
	}()

	/* 
	Avviamo la Goroutine "Consumer". Passiamo
	il canale di lettura, l'Hub per la UI e il socket di uscita.
	Il Consumer vivrà in background smistando il traffico in modo totalmente disaccoppiato.
	 [WORKER POOL]: Lanciamo 5 Goroutine per decodificare e smistare il traffico in parallelo*/
	for i := 0; i < 5; i++ {
		go AvviaSmistatorePacchetti(packetChan, wsHub, pythonConn)
	}

	// FASE 3: SETUP LISTENER TCP (Da Sensore C++)
	
	listener, err := net.Listen("tcp", "0.0.0.0:8080")
	if err != nil {
		fmt.Printf("Errore nell'avvio del listener TCP: %v\n", err)
		return
	}
	fmt.Println("[TCP SERVER] In attesa di stream Protobuf dal sensore C++ su 0.0.0.0:8080...")

	// FASE 3.5: RICEZIONE ALLARMI DA PYTHON
	
	alertAddr, _ := net.ResolveUDPAddr("udp", "0.0.0.0:9002")
	alertConn, errAlert := net.ListenUDP("udp", alertAddr)
	if errAlert != nil {
		fmt.Printf("Errore listener allarmi: %v\n", errAlert)
	} else {
		fmt.Println("[ALERTS] In ascolto di allarmi da Python su UDP 9002...")
		
		// Goroutine in background che ascolta costantemente la porta 9002
		go func() {
			buf := make([]byte, 4096)
			for {
				n, _, err := alertConn.ReadFromUDP(buf)
				if err != nil { // Se c'è un errore, ferma tutto.
					fmt.Printf("[ALERTS] Errore in lettura UDP: %v\n", err)
					break
				}
				// Se non ci sono errori, diffondi l'allarme!
				wsHub.DiffondiAllarme(buf[:n]) 
			}
		}()
	}

	// Configurazione del canale per intercettare CTRL+C (SIGINT/SIGTERM)
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	/* 
	[GRACEFUL SHUTDOWN]: Il blocco defer viene accodato in memoria e garantisce 
	l'esecuzione LIFO alla terminazione del main, rilasciando le risorse hardware
	e chiudendo i canali per far spegnere dolcemente le Goroutine figlie.
	*/
	defer func() {
		fmt.Println("\n[DEFER] Avvio Teardown delle risorse...")
		close(packetChan)    // Termina il range loop del Consumer
		pythonConn.Close()   // Rilascia la porta UDP
		listener.Close()     // Rilascia la porta TCP
		if alertConn != nil {
			alertConn.Close() // Rilascia la porta UDP degli allarmi
		}
	}()

	/*
	Goroutine di Accettazione (Acceptor Loop)
	Vive in un ciclo infinito. Non appena un client si connette, la funzione Accept() 
	si sblocca. Per evitare che un client blocchi l'ascolto di un secondo client,
	il parsing dei dati viene delegato a una nuova Goroutine dedicata (modello thread-per-connection).
	*/
	go func() {
		for {
			conn, err := listener.Accept()
			if err != nil {
				return // Esce silenziosamente se il listener viene chiuso dal defer
			}
			go leggiDalSensoreCPP(conn, packetChan)
		}
	}()

	// FASE 4: ATTESA SINCRONA
	
	// Il main thread si blocca qui, in attesa di estrarre un segnale OS dal canale.
	<-sigChan
	fmt.Println("\n[SEGNALE] Spegnimento richiesto dall'utente.")
}

/*
leggiDalSensoreCPP è il "Producer" della nostra architettura CSP.
La firma utilizza 'chan<-' per indicare un canale di sola scrittura (Send-Only).
Questo garantisce a livello di compilazione che il Producer non possa accidentalmente
svuotare il canale, mantenendo un isolamento perfetto delle responsabilità.
*/
func leggiDalSensoreCPP(conn net.Conn, packetChan chan<- *router.NetworkPacket) {
	// Assicura la chiusura del socket client al termine del flusso stream
	defer conn.Close()
	
	buffer := make([]byte, 4096)

	// Lettura continua del flusso di byte TCP (SOCK_STREAM)
	for {
		n, err := conn.Read(buffer)
		if err != nil {
			// io.EOF indica che il sensore C++ ha chiuso elegantemente la connessione
			if err != io.EOF {
				fmt.Printf("Errore di lettura TCP: %v\n", err)
			}
			break 
		}

		// Decodifica (Unmarshaling) del payload binario verso la struct Go nativa
		packet := &router.NetworkPacket{}
		err = proto.Unmarshal(buffer[:n], packet)
		if err != nil {
			continue // In caso di pacchetto corrotto, lo ignora e prosegue
		}

		// Inserisce il pacchetto pulito nel canale bufferizzato verso il Consumer
		packetChan <- packet
	}
}