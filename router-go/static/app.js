// ==========================================
// LOGICA WEBSOCKET FRONTEND 
// ==========================================

const statusBadge = document.getElementById('status-badge');
const alertsContainer = document.getElementById('alerts-container');

// 1. Inizializziamo la connessione al nostro router Go sulla porta 8081
const ws = new WebSocket("ws://127.0.0.1:8081/ws");

// 2. Evento: Connessione stabilita con successo
ws.onopen = function () {
    statusBadge.textContent = "● LIVE SECURE CONNECTION";
    statusBadge.classList.add("status-connected");
    statusBadge.classList.remove("status-disconnected");
};

// 3. Evento: Connessione persa o chiusa
ws.onclose = function () {
    statusBadge.textContent = "● DISCONNESSO";
    statusBadge.classList.add("status-disconnected");
    statusBadge.classList.remove("status-connected");
};

// 4. Evento: Ricezione di un pacchetto dal backend Go
ws.onmessage = function (event) {
    const rawMessage = event.data;
    aggiungiAllarme(rawMessage);
};

// Funzione di utilità per creare e mostrare l'allarme nella UI
function aggiungiAllarme(testo) {
    const card = document.createElement('div');
    card.className = 'alert-card';

    const timeSpan = document.createElement('span');
    timeSpan.className = 'alert-time';
    const now = new Date();
    // Formattazione timestamp ad alta precisione
    timeSpan.textContent = now.toLocaleTimeString() + "." + now.getMilliseconds().toString().padStart(3, '0');

    const messageSpan = document.createElement('span');
    messageSpan.className = 'alert-message';
    // Protezione automatica XSS inserendo come textContent e non come innerHTML
    messageSpan.textContent = testo;

    card.appendChild(timeSpan);
    card.appendChild(messageSpan);

    // Inserisce il nuovo allarme in cima alla lista
    alertsContainer.prepend(card);

    // Ottimizzazione memoria (Garbage Collection friendly): 
    // manteniamo solo gli ultimi 50 allarmi nel browser
    if (alertsContainer.children.length > 50) {
        alertsContainer.removeChild(alertsContainer.lastChild);
    }
}