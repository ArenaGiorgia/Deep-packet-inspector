// ==========================================================================
// INIZIALIZZAZIONE ELEMENTI DOM E VARIABILI DI STATO
// ==========================================================================
const statusBadge = document.getElementById('status-badge');
const rawContainer = document.getElementById('raw-container');
const alertsContainer = document.getElementById('alerts-container');

// Elementi dei KPI (Contatori in alto)
const kpiTotal = document.getElementById('kpi-total');
const kpiThreats = document.getElementById('kpi-threats');
const kpiLatency = document.getElementById('kpi-latency');

// Elemento e Stato per il pulsante Freeze
const freezeBtn = document.getElementById('freeze-btn');
let isPaused = false;

// Contatori globali
let totalPackets = 0;
let totalThreats = 0;

// Configurazione WebSocket (si connette dinamicamente all'IP del server)
const wsUrl = `ws://${location.hostname}:8081/ws`;
let ws;

// ==========================================================================
// GESTIONE EVENTI (Pulsante Pausa)
// ==========================================================================
freezeBtn.addEventListener('click', () => {
    isPaused = !isPaused;

    if (isPaused) {
        freezeBtn.textContent = "▶️ RIPRENDI FLUSSO";
        freezeBtn.classList.add("active");
    } else {
        freezeBtn.textContent = "⏸️ PAUSA FLUSSO";
        freezeBtn.classList.remove("active");
    }
});

// ==========================================================================
// EFFETTI SPECIALI (Typewriter)
// ==========================================================================
function typeWriterEffect(element, text, speed = 15) {
    let i = 0;
    element.innerHTML = ""; // Svuota il contenitore
    element.classList.add("typing-cursor"); // Aggiunge il cursore CSS quadrato

    function type() {
        if (i < text.length) {
            element.innerHTML += text.charAt(i);
            i++;
            setTimeout(type, speed);
        } else {
            // Rimuove il cursore 1.5 secondi dopo aver finito di digitare
            setTimeout(() => element.classList.remove("typing-cursor"), 1500);
        }
    }
    type();
}

// ==========================================================================
// CORE LOGIC: Connessione e Ricezione Dati
// ==========================================================================
function connect() {
    ws = new WebSocket(wsUrl);

    ws.onopen = function () {
        statusBadge.textContent = "● LIVE SECURE CONNECTION";
        statusBadge.classList.add("status-connected");
        statusBadge.classList.remove("status-disconnected");
    };

    ws.onclose = function () {
        statusBadge.textContent = "● DISCONNESSO";
        statusBadge.classList.add("status-disconnected");
        statusBadge.classList.remove("status-connected");

        // Meccanismo di auto-riconnessione silenziosa ogni 3 secondi
        setTimeout(connect, 3000);
    };

    ws.onmessage = function (event) {
        processMessage(event.data);
    };
}

// ==========================================================================
// PARSING INTELLIGENTE E RENDERING
// ==========================================================================
function processMessage(testo) {
    // 1. I contatori di telemetria girano SEMPRE (dimostra che il backend è vivo)
    totalPackets++;
    kpiTotal.textContent = totalPackets;

    const isThreat = testo.includes("MINACCIA");

    // 2. BLOCCO VISIVO: Se la dashboard è in pausa, non disegniamo nulla a schermo
    if (isPaused) return;

    // Generazione Timestamp per la UI
    const now = new Date();
    const timeString = now.toLocaleTimeString() + "." + now.getMilliseconds().toString().padStart(3, '0');

    if (isThreat) {
        // --- GESTIONE MINACCE (Livello 7) ---
        totalThreats++;
        kpiThreats.textContent = totalThreats;

        // Estrazione chirurgica dei dati dalla stringa di Python tramite RegEx
        // Cattura: Gruppo 1 (IP:Porta), Gruppo 2 (Payload/Messaggio), Gruppo 3 (Latenza)
        const threatRegex = /MINACCIA DA\s+([\d\.:]+)\s*\|\s*([\s\S]*?)\s*\(Latenza:\s*([0-9.]+)\s*ms\)/i;
        const match = testo.match(threatRegex);

        let sourceIp = "Sconosciuto";
        let payloadDetails = testo.replace("🚨", "").trim(); // Fallback se la regex fallisce
        let latency = "--";

        if (match) {
            sourceIp = match[1];
            payloadDetails = match[2];
            latency = match[3];

            // Aggiorna KPI Latenza con effetto flash rosso
            kpiLatency.textContent = latency + " ms";
            kpiLatency.style.color = "#ef4444";
            setTimeout(() => kpiLatency.style.color = "", 300);
        }

        // Creazione Card Strutturata (Stile SOC)
        const card = document.createElement('div');
        card.className = 'threat-card';
        card.innerHTML = `
            <div class="threat-header">
                <div class="threat-header-left">
                    <span class="badge badge-threat">CRITICAL</span>
                    <span class="source-ip">SRC: ${sourceIp}</span>
                </div>
                <span class="alert-time">${timeString}</span>
            </div>
            <div class="threat-body">
                > <span class="typed-payload"></span>
            </div>
            <div class="threat-footer">
                Analisi completata in ${latency} ms
            </div>
        `;

        alertsContainer.prepend(card);

        // Avvia l'effetto macchina da scrivere sul payload iniettato
        const payloadElement = card.querySelector('.typed-payload');
        typeWriterEffect(payloadElement, payloadDetails);

        // Mantiene massimo 50 allarmi a video per non saturare la RAM del browser
        if (alertsContainer.children.length > 50) {
            alertsContainer.removeChild(alertsContainer.lastChild);
        }

    } else if (testo.includes("|")) {
        // --- GESTIONE TRAFFICO GREZZO TCP (Livello 4) ---
        // Ora il testo è: "SYN|192.168.1.1:80|172.22.0.4:9999"
        const parts = testo.split("|");
        const flag = parts[0];
        const sourceIp = parts[1] || "Sconosciuto";
        const destIp = parts[2] || "Sconosciuto"; // Estraiamo il destinatario!

        let badgeClass = "badge-ack";
        if (flag === "SYN") badgeClass = "badge-syn";
        else if (flag === "FIN" || flag === "RST") badgeClass = "badge-fin";
        else if (flag === "PSH") badgeClass = "badge-psh";

        // Creazione riga con IP Sorgente ➔ IP Destinazione
        const riga = document.createElement('div');
        riga.className = 'raw-card';
        riga.innerHTML = `
            <span class="alert-time">${timeString}</span>
            <span class="badge ${badgeClass}">${flag}</span>
            <div class="ip-flow">
                <span class="source-ip">${sourceIp}</span>
                <span class="arrow">➔</span>
                <span class="dest-ip">${destIp}</span>
            </div>
        `;

        rawContainer.prepend(riga);

        if (rawContainer.children.length > 100) {
            rawContainer.removeChild(rawContainer.lastChild);
        }
    }
}

// Avvio applicazione
connect();