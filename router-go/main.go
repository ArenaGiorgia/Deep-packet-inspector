/*
Questo modulo funge da punto di ingresso (main) dell'intera architettura Go.
Svolge cinque compiti architetturali fondamentali:
1. Istanzia tutte le risorse hardware/software e le inietta nelle Goroutine.
2. Modello CSP (Communicating Sequential Processes): Inizializza il canale bufferizzato.
3. Accetta flussi raw in ingresso, li filtra e instrada via UDP.
4. TCP Length-Prefix Framing: Garantisce la delimitazione dei messaggi sullo stream di rete.
5. Graceful Shutdown & WaitGroup: Gestisce il ciclo di vita delle Goroutine per uno spegnimento sicuro.
*/
package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"syscall"

	"dpi/router/router"

	"google.golang.org/protobuf/proto"
)

func main() {
	fmt.Println("Avvio Router Go (Livello 4 - Multi-Protocol Gateway)...")

	// INIZIALIZZAZIONE RISORSE
	//  Istanziamo l'Hub WebSocket con il proprio stato incapsulato
	wsHub := CreaHubDashboard()

	// Apriamo il socket UDP verso l'Analizzatore Python (Sfrutta il DNS interno di Docker)
	pythonAddr, _ := net.ResolveUDPAddr("udp", "analyzer:9001")
	pythonConn, err := net.DialUDP("udp", nil, pythonAddr)
	if err != nil {
		fmt.Println("Errore apertura connessione UDP verso Python:", err)
		return
	}

	/* Creiamo il canale di comunicazione principale.
	È un canale bufferizzato (capacità 100).
	Se c'è un picco improvviso (burst) di traffico, i pacchetti vengono accodati
	in memoria senza bloccare il Producer TCP.
	*/
	packetChan := make(chan *router.NetworkPacket, 100)

	// AVVIO DEI MICROSERVIZI (Asincroni)
	// Routing HTTP per servire la Dashboard Web e il WebSocket
	http.HandleFunc("/ws", wsHub.AccettaConnessioneWeb)
	http.Handle("/", http.FileServer(http.Dir("./static")))

	go func() {
		fmt.Println("[DASHBOARD] Server Web in ascolto su http://127.0.0.1:8081")
		http.ListenAndServe(":8081", nil)
	}()

	// Inizializziamo il WaitGroup per tracciare il ciclo di vita dei Worker
	var wg sync.WaitGroup

	/*
	 [WORKER POOL]: Lanciamo 5 Goroutine per decodificare e smistare il traffico in parallelo.
	 Utilizziamo il WaitGroup per garantire che nessuna Goroutine venga interrotta bruscamente.
	*/
	for i := 0; i < 5; i++ {
		wg.Add(1) // Segnaliamo un nuovo worker attivo

		go func() {
			defer wg.Done() // Decrementa il contatore quando la Goroutine termina naturalmente

			// RICHIAMO DELLA TUA FUNZIONE IN ROUTER.GO
			AvviaSmistatorePacchetti(packetChan, wsHub, pythonConn)
		}()
	}

	//  SETUP LISTENER TCP & UDP
	//  Listener TCP per ricevere i pacchetti grezzi dal sensore C++
	tcpListener, err := net.Listen("tcp", "0.0.0.0:8080")
	if err != nil {
		fmt.Printf("Errore nell'avvio del listener TCP: %v\n", err)
		return
	}
	fmt.Println("[TCP SERVER] In attesa di stream Protobuf dal sensore C++ su 0.0.0.0:8080...")

	//  SETUP LISTENER PER IL TRAFFICO SIMULATO (Porta 9999)
	//  Permette al generator.py di completare la connessione TCP ed inviare i payload reali
	//  che verranno intercettati passivamente dal sensore C++ (libpcap).
	simListener, errSim := net.Listen("tcp", "0.0.0.0:9999")
	if errSim != nil {
		fmt.Printf("[ERRORE] Impossibile avviare il listener di simulazione sulla porta 9999: %v\n", errSim)
	} else {
		fmt.Println("[SIMULATOR SERVER] In ascolto per il traffico di test su 0.0.0.0:9999...")
		go func() {
			for {
				conn, err := simListener.Accept()
				if err != nil {
					return // Esce se il listener viene chiuso dal defer
				}
				go func(c net.Conn) {
					defer c.Close()
					buf := make([]byte, 1024)
					for {
						_, err := c.Read(buf)
						if err != nil {
							break
						}
					}
				}(conn)
			}
		}()
	}

	//  Listener UDP per ricevere gli alert (allarmi) di ritorno da Python
	alertAddr, _ := net.ResolveUDPAddr("udp", "0.0.0.0:9002")
	alertConn, errAlert := net.ListenUDP("udp", alertAddr)
	if errAlert != nil {
		fmt.Printf("Errore listener allarmi: %v\n", errAlert)
	} else {
		fmt.Println("[ALERTS] In ascolto di allarmi da Python su UDP 9002...")

		// Goroutine dedicata esclusivamente all'ascolto degli allarmi
		go func() {
			alertBuffer := make([]byte, 4096)
			for {
				n, _, err := alertConn.ReadFromUDP(alertBuffer)
				if err != nil {
					// Evitiamo di stampare l'errore se il socket è stato chiuso volontariamente dal defer
					if err.Error() != "use of closed network connection" {
						fmt.Printf("[ALERTS] Errore in lettura UDP: %v\n", err)
					}
					break
				}
				// Diffondi immediatamente l'allarme alla Dashboard
				wsHub.DiffondiAllarme(alertBuffer[:n])
			}
		}()
	}

	// Configurazione del canale per intercettare i segnali di stop del S.O. (SIGINT/SIGTERM)
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	/*
	 GRACEFUL SHUTDOWN Il blocco defer garantisce l'esecuzione LIFO.
	 L'ordine qui è VITALMENTE IMPORTANTE per evitare Panic:
	 Chiudiamo PRIMA i socket di rete (fermando i Producer) e SOLO DOPO chiudiamo il canale.
	*/
	defer func() {
		fmt.Println("\n[DEFER] Avvio Teardown delle risorse...")

		tcpListener.Close() // 1. Ferma l'accettazione di nuovi pacchetti
		if simListener != nil {
			simListener.Close() // Chiusura del listener di simulazione
		}
		pythonConn.Close() // 2. Rilascia la porta UDP
		if alertConn != nil {
			alertConn.Close()
		}

		close(packetChan) // 3. Termina in sicurezza il for-range dei Worker
	}()

	/*
	 [ACCEPTOR LOOP]: Modello Thread-per-Connection.
	 Vive in un ciclo infinito. Non appena il sensore C++ si connette,
	 delega la lettura a una Goroutine dedicata.
	*/
	go func() {
		for {
			conn, err := tcpListener.Accept()
			if err != nil {
				return // Esce silenziosamente se il listener viene chiuso dal defer
			}
			go leggiDalSensoreCPP(conn, packetChan)
		}
	}()

	//  ATTESA SINCRONA
	//  Il main thread si sospende qui finché non riceve un segnale di terminazione.

	<-sigChan
	fmt.Println("\n[SEGNALE] Spegnimento richiesto dall'utente. In attesa del completamento dei Worker...")

	// Attendiamo che i Worker finiscano di processare i pacchetti residui nel canale
	wg.Wait()

	fmt.Println("[SHUTDOWN] Tutti i worker sono stati arrestati in sicurezza. Spegnimento completato.")
}

/*
leggiDalSensoreCPP è il "Producer" della nostra architettura CSP.
Utilizza 'chan<-' per indicare un canale Send-Only (di sola scrittura),
prevenendo letture accidentali da parte del Producer.
*/
func leggiDalSensoreCPP(conn net.Conn, packetChan chan<- *router.NetworkPacket) {
	// Garantisce il rilascio del File Descriptor del socket al termine
	defer conn.Close()

	for {

		// TCP LENGTH-PREFIX FRAMING
		// Poiché TCP è uno "Stream di byte", dobbiamo leggere prima l'intestazione
		// di 4 byte inserita dal C++ per scoprire l'esatta lunghezza del payload.

		bufferLunghezza := make([]byte, 4)

		// Usiamo io.ReadFull per bloccarci finché non abbiamo esattamente 4 byte.
		if _, err := io.ReadFull(conn, bufferLunghezza); err != nil {
			if err != io.EOF {
				fmt.Printf("[ERRORE] Lettura header TCP interrotta: %v\n", err)
			}
			break
		}

		// Convertiamo i 4 byte in un intero (BigEndian è lo standard di rete)
		dimensioneMessaggio := binary.BigEndian.Uint32(bufferLunghezza)

		// Ora che sappiamo la dimensione esatta, leggiamo il payload
		bufferPayload := make([]byte, dimensioneMessaggio)
		if _, err := io.ReadFull(conn, bufferPayload); err != nil {
			fmt.Printf("[ERRORE] Lettura payload TCP interrotta: %v\n", err)
			break
		}

		// Deserializzazione sicura
		packet := &router.NetworkPacket{}
		err := proto.Unmarshal(bufferPayload, packet)
		if err != nil {
			continue // Ignoriamo pacchetti non conformi
		}

		// Inserisce il pacchetto nel canale verso i Worker
		packetChan <- packet
	}
}
