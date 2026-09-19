/*
Questo modulo funge da punto di ingresso (main) dell'intera architettura Go.
Svolge compiti architetturali fondamentali:
1. Istanzia tutte le risorse hardware/software e le inietta nelle Goroutine.
2. Modello CSP (Communicating Sequential Processes): Inizializza il canale bufferizzato.
3. TCP Length-Prefix Framing con limiti di sicurezza sulla memoria contro OOM.
4. Graceful Shutdown Completo: Traccia sia i Producer (C++) che i Consumer (Worker) senza Deadlock.
*/
package main

import (
	"encoding/binary"
	"errors" 
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

// Costante per la sicurezza della memoria: limite massimo di un pacchetto Protobuf (es. 10 MB)
const MaxPayloadSize = 10 * 1024 * 1024

func main() {
	fmt.Println("Avvio Router Go (Livello 4 - Multi-Protocol Gateway)...")

	// INIZIALIZZAZIONE RISORSE
	// Istanziamo l'Hub WebSocket con il proprio stato incapsulato
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
	in memoria senza bloccare i Producer.
	*/
	packetChan := make(chan *router.NetworkPacket, 100)

	
	// AVVIO DEI MICROSERVIZI E WORKER POOL
	// Routing HTTP per servire la Dashboard Web e il WebSocket
	http.HandleFunc("/ws", wsHub.AccettaConnessioneWeb)
	http.Handle("/", http.FileServer(http.Dir("./static")))

	go func() {
		fmt.Println("[DASHBOARD] Server Web in ascolto su http://127.0.0.1:8081")
		// Gestione robusta dell'errore: ignoriamo ErrServerClosed che è normale durante lo spegnimento
		if err := http.ListenAndServe(":8081", nil); err != nil && !errors.Is(err, http.ErrServerClosed) {
			fmt.Printf("[ERRORE FATALE] Server Web fallito: %v\n", err)
		}
	}()

	// WaitGroup per i CONSUMER (i 5 worker che analizzano il traffico)
	var wgConsumers sync.WaitGroup

	/*
	 [WORKER POOL]: Lanciamo 5 Goroutine per decodificare e smistare il traffico in parallelo.
	 Utilizziamo il WaitGroup per garantire che nessuna Goroutine venga interrotta bruscamente.
	*/
	for i := 0; i < 5; i++ {
		wgConsumers.Add(1) // Segnaliamo un nuovo worker attivo
		go func() {
			defer wgConsumers.Done() // Decrementa il contatore quando la Goroutine termina naturalmente
			AvviaSmistatorePacchetti(packetChan, wsHub, pythonConn)
		}()
	}

	// SETUP LISTENER DI RETE
	// Listener TCP per ricevere i pacchetti grezzi dal sensore C++
	tcpListener, err := net.Listen("tcp", "0.0.0.0:8080")
	if err != nil {
		fmt.Printf("Errore nell'avvio del listener TCP: %v\n", err)
		return
	}
	fmt.Println("[TCP SERVER] In attesa di stream Protobuf dal sensore C++ su 0.0.0.0:8080...")

	// Listener per il traffico simulato (Porta 9999)
	simListener, errSim := net.Listen("tcp", "0.0.0.0:9999")
	if errSim != nil {
		fmt.Printf("[ERRORE] Impossibile avviare il listener di simulazione sulla porta 9999: %v\n", errSim)
	} else {
		fmt.Println("[SIMULATOR SERVER] In ascolto per il traffico di test su 0.0.0.0:9999...")
		go func() {
			for {
				conn, err := simListener.Accept()
				if err != nil {
					return // Esce silenziosamente durante lo shutdown
				}
				go func(c net.Conn) {
					defer c.Close()
					buf := make([]byte, 1024)
					for {
						if _, err := c.Read(buf); err != nil {
							break
						}
					}
				}(conn)
			}
		}()
	}

	// Listener UDP per ricevere gli alert (allarmi) di ritorno da Python
	alertAddr, _ := net.ResolveUDPAddr("udp", "0.0.0.0:9002")
	alertConn, errAlert := net.ListenUDP("udp", alertAddr)
	if errAlert != nil {
		fmt.Printf("Errore listener allarmi: %v\n", errAlert)
	} else {
		fmt.Println("[ALERTS] In ascolto di allarmi da Python su UDP 9002...")
		go func() {
			alertBuffer := make([]byte, 4096)
			for {
				n, _, err := alertConn.ReadFromUDP(alertBuffer)
				if err != nil {
					// Uso idiomatico di errors.Is per verificare se il socket è stato chiuso volontariamente
					if !errors.Is(err, net.ErrClosed) {
						fmt.Printf("[ALERTS] Errore in lettura UDP: %v\n", err)
					}
					break
				}
				// Diffondi immediatamente l'allarme alla Dashboard
				wsHub.DiffondiAllarme(alertBuffer[:n])
			}
		}()
	}

	
	// GESTIONE PRODUCERS E ACCEPTOR LOOP (Sicurezza contro i Panic)
	// WaitGroup e Mappa per tracciare rigorosamente le connessioni TCP attive (Producer)
	var wgProducers sync.WaitGroup
	var muConnessioni sync.Mutex
	connessioniAttive := make(map[net.Conn]bool)

	/*
	 [ACCEPTOR LOOP]: Modello Thread-per-Connection.
	 Vive in un ciclo infinito. Non appena il sensore C++ si connette,
	 delega la lettura a una Goroutine dedicata, tenendone traccia.
	*/
	go func() {
		for {
			conn, err := tcpListener.Accept()
			if err != nil {
				return // Esce silenziosamente se il listener viene chiuso durante lo shutdown
			}
			
			// Tracciamo la nuova connessione del Sensore C++
			wgProducers.Add(1)
			muConnessioni.Lock()
			connessioniAttive[conn] = true
			muConnessioni.Unlock()

			go func(c net.Conn) {
				defer wgProducers.Done() // Segnala che questo producer ha finito
				defer func() {
					// Rimuove la connessione dalla mappa quando termina
					muConnessioni.Lock()
					delete(connessioniAttive, c)
					muConnessioni.Unlock()
				}()
				
				leggiDalSensoreCPP(c, packetChan)
			}(conn)
		}
	}()

	// Configurazione del canale per intercettare i segnali di stop del S.O. (SIGINT/SIGTERM)
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)


	// SEQUENZA DI GRACEFUL SHUTDOWN (LINEARE E SENZA DEADLOCK)
	//  ATTESA SINCRONA: Il main thread si sospende qui finché non riceve un segnale.
	<-sigChan
	fmt.Println("\n[SEGNALE] Spegnimento richiesto dall'utente. Avvio Teardown delle risorse...")

	/*
	 L'ordine qui è VITALMENTE IMPORTANTE. Eseguiamo le chiusure in sequenza lineare
	 e NON in un blocco defer per evitare Deadlock e Panic su canali chiusi.
	*/

	// 1. Fermiamo i Listener (blocca l'ingresso di nuovo traffico TCP/HTTP)
	tcpListener.Close() 
	if simListener != nil {
		simListener.Close() 
	}

	// 2. FORZIAMO la chiusura dei socket attivi.
	// Questo fa fallire gli io.ReadFull bloccati nei producer, facendoli uscire dai loop.
	muConnessioni.Lock()
	for c := range connessioniAttive {
		c.Close()
	}
	muConnessioni.Unlock()

	// 3. Attendiamo che tutti i Producer siano effettivamente morti (nessuno scrive più)
	wgProducers.Wait()
	fmt.Println("[SHUTDOWN] Tutti i mittenti (Producer) sono stati arrestati.")

	// 4. ORA è sicuro chiudere il canale (garanzia matematica che nessuno tenterà di scriverci)
	close(packetChan) 

	// 5. Attendiamo che i Consumer svuotino il canale bufferizzato residuo
	wgConsumers.Wait()
	fmt.Println("[SHUTDOWN] Tutti i worker sono stati arrestati in sicurezza. Svuotamento buffer completato.")

	// 6. SOLO DOPO aver fermato i worker, chiudiamo i socket verso l'esterno.
	// Questo previene scritture accidentali UDP su socket già chiusi.
	pythonConn.Close() 
	if alertConn != nil {
		alertConn.Close()
	}

	fmt.Println("[SHUTDOWN] Risorse di rete rilasciate in sicurezza. Spegnimento completato.")
}

/*
leggiDalSensoreCPP è il "Producer" della nostra architettura CSP.
Utilizza 'chan<-' per indicare un canale Send-Only (di sola scrittura).
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

		// SICUREZZA: Prevenzione da allocazioni di memoria maligne (OOM)
		if dimensioneMessaggio > MaxPayloadSize {
			fmt.Printf("[SECURITY FATAL] Ricevuto pacchetto di %d byte (supera il limite di %d byte). Connessione abbattuta.\n", dimensioneMessaggio, MaxPayloadSize)
			break
		}

		// Ora che sappiamo la dimensione esatta (e sicura), leggiamo il payload
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