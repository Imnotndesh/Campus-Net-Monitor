# gosip-server

A minimal SIP registrar + call-routing server written in Go using [diago](https://github.com/emiago/diago) and [sipgo](https://github.com/emiago/sipgo).

---

## What it does

| SIP Method | Behaviour |
|------------|-----------|
| `REGISTER` | Stores the `AOR → Contact` binding in memory with TTL. Expired bindings are auto-reaped every 30 s. |
| `INVITE`   | Looks up the callee in the registry and bridges both legs via a diago B2BUA. Returns `480 Temporarily Unavailable` if callee is not registered. |
| `OPTIONS`  | Returns `200 OK` – used by softphones as a keep-alive / NAT punch. |

Plus a small **HTTP API** for monitoring (port `8080`).

---

## Quick Start

### Option A – Docker Compose (recommended for testing)

```bash
docker compose up --build
```

### Option B – Run directly (requires Go 1.22+)

```bash
go mod tidy
go run .
```

---

## Softphone Setup

Point any softphone at `127.0.0.1:5060` (UDP).

| Setting    | Value          |
|------------|----------------|
| SIP server | `127.0.0.1`    |
| Port       | `5060`         |
| Transport  | UDP            |
| Username   | e.g. `alice`   |
| Password   | *(leave blank)*|
| Domain     | `127.0.0.1`    |

Register **two** accounts (e.g. `alice` and `bob`), then call `sip:bob@127.0.0.1` from Alice's softphone.

### Recommended softphones
- **Linphone** – free, cross-platform, easy
- **MicroSIP** – lightweight Windows app
- **Zoiper** – good codec support
- **Bria Solo** – professional feel, free tier

---

## HTTP API

```bash
# List all active registrations
curl http://localhost:8080/registrations

# Health check
curl http://localhost:8080/health
```

Example response:
```json
[
  {"aor":"sip:alice@127.0.0.1", "contact":"sip:alice@192.168.1.50:5061", "remote_ip":"192.168.1.50", "ttl":"52m30s"},
  {"aor":"sip:bob@127.0.0.1",   "contact":"sip:bob@192.168.1.51:5061",   "remote_ip":"192.168.1.51", "ttl":"58m10s"}
]
```

---

## Configuration

Set via environment variables:

| Variable    | Default   | Description               |
|-------------|-----------|---------------------------|
| `SIP_HOST`  | `0.0.0.0` | Interface to listen on    |
| `SIP_PORT`  | `5060`    | SIP UDP/TCP port          |
| `HTTP_PORT` | `8080`    | HTTP API port             |

---

## Project Structure

```
gosip-server/
├── main.go          # Entry point: UA setup, REGISTER/OPTIONS/INVITE handlers, HTTP API
├── registry.go      # Thread-safe in-memory AOR registration store
├── Dockerfile       # Multi-stage scratch image (~8 MB)
├── docker-compose.yml
└── go.mod
```

---

## Debugging

Enable detailed SIP + RTP tracing by setting these env vars before running:

```bash
SIP_DEBUG=true RTP_DEBUG=true go run .
```

Or in code:
```go
sip.SIPDebug = true
media.RTPDebug = true
```

---

## Connecting Physical VoIP Phones (later)

Replace `127.0.0.1` with your machine's LAN IP (e.g. `192.168.1.100`). Make sure port `5060/UDP` is not blocked by your firewall. Physical phones register and call exactly the same way.

---

## Next Steps

- Add **digest authentication** (`sipgo.DigestAuth`)
- Add **TCP / TLS / WebSocket** transports (diago `WithTransport`)
- Persist registrations to **SQLite or Redis**
- Add a **dial-plan** (extensions → actions)
- Add **voicemail** (record to `.wav` with diago's `AudioRecording`)
