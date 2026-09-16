/*AvviaSmistatorePacchetti è la Goroutine "Worker".
Nel main.go ne lanceremo 5 in parallelo creando un vero "Worker Pool" per scalare su più core della CPU.
Il simbolo <- chan indica un Canale di sola lettura (Receive-only Channel), garantendo
che questa funzione prelevi i dati in sicurezza senza poterli alterare accidentalmente prima del tempo.
Invece di appesantire il C++ con il parsing, qui applichiamo il pattern "Thin Sensor".
Il C++ ci inietta i byte grezzi catturati dalla scheda di rete.
Sarà Go, a spacchettare il livello Datalink (Ethernet),
Network (IPv4) e Transport (TCP) estraendo solo ciò che è importante per l'analisi forense in Python.*/
package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"dpi/router/router"
	"google.golang.org/protobuf/proto"
)

func AvviaSmistatorePacchetti(packetChannel <-chan *router.NetworkPacket, hub *WebSocketHub, connessionePython *net.UDPConn) {
	fmt.Println("[WORKER] Avviato e in attesa di frame di rete grezzi dal C++...")

	// Il for-range sul canale si blocca (senza consumare CPU) finché non arrivano nuovi pacchetti
	for rawPacket := range packetChannel {
		
		frameRete := rawPacket.RawPayload

		// Un pacchetto minimo Ethernet (14 byte) + IPv4 (20 byte) è di 34 byte.
		// Se è più piccolo, è un frammento corrotto. Lo scartiamo.
		if len(frameRete) < 34 {
			continue
		}

		 /*DECODIFICA ETHERNET (Livello 2 - Datalink)
		 Leggiamo l'EtherType ai byte 12 e 13. Il valore 0x0800 indica che il protocollo successivo è IPv4.*/
		typeEthernet := binary.BigEndian.Uint16(frameRete[12:14])
		if typeEthernet != 0x0800 {
			continue // Ignoriamo traffico IPv6, ARP, ecc. per concentrarci sulle minacce IPv4
		}

		// DECODIFICA IPv4 (Livello 3 - Network)
		// L'header IPv4 inizia al byte 14.
		// I primi 4 bit del byte 14 indicano la versione, i secondi 4 bit indicano la lunghezza dell'header in word da 32 bit.
		// Usiamo l'operatore bit a bit AND (& 0x0F) per mascherare i primi 4 bit ed estrarre la lunghezza.
		lunghezzaHeaderIP := int(frameRete[14] & 0x0F) * 4
		
		// Il byte 9 dell'header IP (14 + 9 = 23) ci dice quale protocollo di trasporto c'è dentro. (6 = TCP, 17 = UDP)
		protocolloIP := frameRete[23]
		
		// Indirizzi IP sorgente e destinazione (ciascuno di 4 byte)
		ipSorgente := net.IPv4(frameRete[26], frameRete[27], frameRete[28], frameRete[29]).String()
		ipDestinazione := net.IPv4(frameRete[30], frameRete[31], frameRete[32], frameRete[33]).String()

		// DECODIFICA TCP (Livello 4 - Transport)
		
		if protocolloIP == 6 { // È un pacchetto TCP
			
			inizioTCP := 14 + lunghezzaHeaderIP
			
			// Verifica di sicurezza per evitare Panic: l'header TCP minimo è di 20 byte
			if len(frameRete) < inizioTCP+20 {
				continue 
			}

			// Estraiamo le porte leggendo blocchi di 2 byte (Uint16)
			portaSorgente := binary.BigEndian.Uint16(frameRete[inizioTCP : inizioTCP+2])
			portaDestinazione := binary.BigEndian.Uint16(frameRete[inizioTCP+2 : inizioTCP+4])
			
			// Estraiamo i Sequence Number (Blocchi di 4 byte - Uint32) vitali per il riassemblaggio in Python
			numeroSequenza := binary.BigEndian.Uint32(frameRete[inizioTCP+4 : inizioTCP+8])
			
			// Estraiamo i Flag TCP (FIN, SYN, RST, PSH, ACK, URG) localizzati al byte 13 del TCP
			flagTCP := frameRete[inizioTCP+13]

			// Calcoliamo dove finisce l'header TCP usando uno shift bit a bit (>> 4) per estrarre il Data Offset
			lunghezzaHeaderTCP := int(frameRete[inizioTCP+12] >> 4) * 4
			inizioDatiTCP := inizioTCP + lunghezzaHeaderTCP

			// Estraiamo finalmente il vero payload (testo/dati) scartando tutti gli header precedenti
			var datiTCP []byte
			if len(frameRete) > inizioDatiTCP {
				datiTCP = frameRete[inizioDatiTCP:]
			}

			/* POPOLAMENTO PROTOBUF E INOLTRO:
			 Riempiamo l'oggetto Protobuf con i dati puliti e ordinati*/
			rawPacket.SourceIp = ipSorgente
			rawPacket.DestIp = ipDestinazione
			rawPacket.SourcePort = int32(portaSorgente)
			rawPacket.DestPort = int32(portaDestinazione)
			rawPacket.Protocol = "TCP"
			rawPacket.SeqNum = numeroSequenza
			rawPacket.TcpFlags = uint32(flagTCP)
			rawPacket.RawPayload = datiTCP 

			// Serializziamo in formato binario ultraleggero per Python
			datiSerializzati, err := proto.Marshal(rawPacket)
			if err == nil {
				connessionePython.Write(datiSerializzati)
			}

			// Invia la notifica alla Dashboard (inviamo solo i metadati, non il payload pesante!)
			messaggioWeb := fmt.Sprintf("Analisi TCP decodificata: %s:%d", ipSorgente, portaSorgente)
			hub.DiffondiAllarme([]byte(messaggioWeb))
		}
	}
}