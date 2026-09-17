// LOGICA WEBSOCKET FRONTEND 

const statusBadge = document.getElementById('status-badge');
const rawContainer = document.getElementById('raw-container');
const alertsContainer = document.getElementById('alerts-container');

// Elementi dei contatori (KPI)
const kpiTotal = document.getElementById('kpi-total');
const kpiThreats = document.getElementById('kpi-threats');
const kpiLatency = document.getElementById('kpi-latency');

// Variabili di stato interne
let totalPackets = 0;
let totalThreats = 0;

// URL dinamico: si connette sempre all'IP giusto su cui navighi
const wsUrl = `ws://${location.hostname}:8081/ws`;
let ws;

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
        // Tenta di riconnettersi in automatico dopo 3 secondi
        setTimeout(connect, 3000);
    };

    ws.onmessage = function (event) {
        const rawMessage = event.data;
        processMessage(rawMessage);
    };
}

function processMessage(testo) {
    //  Aggiorniamo sempre il traffico totale analizzato
    totalPackets++;
    kpiTotal.textContent = totalPackets;

    //  Capiamo di che tipo di pacchetto si tratta
    const isThreat = testo.includes("🚨") || testo.includes("MINACCIA");

    const card = document.createElement('div');
    card.className = 'alert-card';

    if (isThreat) {
        // Aggiorna contatore minacce
        totalThreats++;
        kpiThreats.textContent = totalThreats;
        card.classList.add('threat-card');

        //  Estraiamo il numero esatto della latenza dal testo usando una RegEx!
        const latencyMatch = testo.match(/Latenza:\s*([0-9.]+)\s*ms/);
        if (latencyMatch && latencyMatch[1]) {
            kpiLatency.textContent = latencyMatch[1] + " ms";
            // Effetto flash sul testo per far notare l'aggiornamento
            kpiLatency.style.color = "#ef4444";
            setTimeout(() => kpiLatency.style.color = "", 300);
        }
    }

    const timeSpan = document.createElement('span');
    timeSpan.className = 'alert-time';
    const now = new Date();
    timeSpan.textContent = now.toLocaleTimeString() + "." + now.getMilliseconds().toString().padStart(3, '0');

    const messageSpan = document.createElement('span');
    messageSpan.className = 'alert-message';
    messageSpan.textContent = testo;

    card.appendChild(timeSpan);
    card.appendChild(messageSpan);

    //  SMISTAMENTO SPLIT-SCREEN
    if (isThreat) {
        // Le minacce vanno nella colonna di Destra (Ne teniamo massimo 50 a video)
        alertsContainer.prepend(card);
        if (alertsContainer.children.length > 50) {
            alertsContainer.removeChild(alertsContainer.lastChild);
        }
    } else {
        // Il traffico grezzo va nella colonna di Sinistra (Ne teniamo massimo 100 a video)
        rawContainer.prepend(card);
        if (rawContainer.children.length > 100) {
            rawContainer.removeChild(rawContainer.lastChild);
        }
    }
}

// Avvio della connessione all'apertura della pagina
connect();