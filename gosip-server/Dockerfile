# ── Build stage ────────────────────────────────────────────────────────────────
FROM golang:1.22-alpine AS builder

WORKDIR /src
COPY go.mod go.sum* ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -trimpath -ldflags="-s -w" -o /gosip-server .

# ── Runtime stage ───────────────────────────────────────────────────────────────
FROM scratch

COPY --from=builder /gosip-server /gosip-server
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/

EXPOSE 5060/udp
EXPOSE 5060/tcp
EXPOSE 8080/tcp

ENTRYPOINT ["/gosip-server"]
