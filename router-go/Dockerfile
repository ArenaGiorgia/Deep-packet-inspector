# STAGE 1: Builder (Contiene il compilatore Go)
FROM golang:alpine AS builder
WORKDIR /app

# Copiamo TUTTO il codice subito (così Docker può leggere i file .go)
COPY . .

# MAGIC COMMAND: Legge i file .go, nota che manca 'gorilla/websocket' e lo scarica automaticamente
RUN go mod tidy

# Disabilitiamo CGO per avere un binario statico indipendente al 100%
RUN CGO_ENABLED=0 GOOS=linux go build -o router-go .

# STAGE 2: Produzione (Immagine leggerissima da ~10MB)
FROM alpine:latest
WORKDIR /root/

# Copiamo il binario dal builder
COPY --from=builder /app/router-go .
# Copiamo la cartella static per la Dashboard Web
COPY --from=builder /app/static ./static

EXPOSE 8080 8081
CMD ["./router-go"]