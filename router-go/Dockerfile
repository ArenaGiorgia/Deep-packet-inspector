# STAGE 1: Builder
FROM golang:alpine AS builder
WORKDIR /app

# FIX RETE AVANZATO: Proxy alternativo e disattivazione checksum
ENV GOPROXY="https://goproxy.io,direct"
ENV GOSUMDB="off"

COPY go.mod go.sum* ./
RUN go mod download || go mod tidy

COPY . .
RUN go mod tidy

RUN CGO_ENABLED=0 GOOS=linux go build -o router-go .

# STAGE 2: Produzione
FROM alpine:latest
WORKDIR /root/
COPY --from=builder /app/router-go .
COPY --from=builder /app/static ./static

EXPOSE 8080 8081
CMD ["./router-go"]