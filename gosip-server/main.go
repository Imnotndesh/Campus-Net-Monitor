package main

import (
	"context"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/emiago/diago"
	"github.com/emiago/sipgo"
	"github.com/emiago/sipgo/sip"
	"github.com/lmittmann/tint"
)

func main() {
	// ── Structured colour logger ────────────────────────────────────────────
	slog.SetDefault(slog.New(tint.NewHandler(os.Stdout, &tint.Options{
		Level:      slog.LevelDebug,
		TimeFormat: time.TimeOnly,
	})))

	// ── Config from env (with defaults) ─────────────────────────────────────
	sipHost := envOr("SIP_HOST", "0.0.0.0")
	sipPort := envOr("SIP_PORT", "5060")
	httpPort := envOr("HTTP_PORT", "8080")

	slog.Info("🚀 gosip-server starting",
		"sip", fmt.Sprintf("%s:%s/UDP", sipHost, sipPort),
		"http", fmt.Sprintf(":%s", httpPort),
	)

	registry := NewRegistry()

	// ── Build the SIP User Agent ─────────────────────────────────────────────
	ua, err := sipgo.NewUA(sipgo.WithUserAgent("gosip-server/1.0"))
	if err != nil {
		slog.Error("Failed to create UA", "err", err)
		os.Exit(1)
	}

	// ── diago instance (handles INVITE / call bridging) ──────────────────────
	dg := diago.NewDiago(ua,
		diago.WithTransport(diago.Transport{
			Transport: "udp",
			BindHost:  sipHost,
			BindPort:  mustInt(sipPort),
		}),
	)

	// ── sipgo server handle (handles REGISTER, OPTIONS, etc.) ────────────────
	srv, err := sipgo.NewServer(ua)
	if err != nil {
		slog.Error("Failed to create SIP server", "err", err)
		os.Exit(1)
	}

	srv.OnRegister(handleRegister(registry))
	srv.OnOptions(handleOptions())

	// ── Context + graceful shutdown ──────────────────────────────────────────
	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	// ── HTTP status API ──────────────────────────────────────────────────────
	go serveHTTP(httpPort, registry)

	// ── Serve incoming INVITE dialogs via diago ──────────────────────────────
	go func() {
		slog.Info("diago Serve started")
		if err := dg.Serve(ctx, handleInvite(dg, registry)); err != nil {
			if ctx.Err() == nil { // not a shutdown
				slog.Error("diago.Serve error", "err", err)
			}
		}
	}()

	slog.Info("✅  Server ready – waiting for SIP traffic")
	<-ctx.Done()
	slog.Info("Shutting down…")
}

// ── REGISTER handler ─────────────────────────────────────────────────────────

func handleRegister(registry *Registry) sipgo.RequestHandler {
	return func(req *sip.Request, tx sip.ServerTransaction) {
		slog.Debug("← REGISTER",
			"from", req.From().Address.String(),
			"src", req.Source(),
		)

		toHdr := req.To()
		if toHdr == nil {
			tx.Respond(sip.NewResponseFromRequest(req, sip.StatusBadRequest, "Missing To", nil))
			return
		}

		aor := toHdr.Address.String()

		// Determine expires: prefer Expires header, then Contact param
		expires := 3600
		if expHdr := req.GetHeader("Expires"); expHdr != nil {
			if v, err := strconv.Atoi(strings.TrimSpace(expHdr.Value())); err == nil {
				expires = v
			}
		}

		contactHdr := req.Contact()
		if contactHdr == nil && expires > 0 {
			// No Contact but positive Expires is unusual – just 200 OK
			tx.Respond(sip.NewResponseFromRequest(req, sip.StatusOK, "OK", nil))
			return
		}

		contact := ""
		if contactHdr != nil {
			contact = contactHdr.Address.String()
			// Per-contact expires overrides header-level
			if ep := contactHdr.Params.Get("expires"); ep != "" {
				if v, err := strconv.Atoi(ep); err == nil {
					expires = v
				}
			}
		}

		registry.Store(aor, contact, req.Source(), expires)

		resp := sip.NewResponseFromRequest(req, sip.StatusOK, "OK", nil)
		if contact != "" && expires > 0 {
			resp.AppendHeader(&sip.ContactHeader{
				Address: sip.Uri{Encrypted: false},
				Params:  sip.HeaderParams{"expires": strconv.Itoa(expires)},
			})
			resp.AppendHeader(sip.NewHeader("Contact",
				fmt.Sprintf("<%s>;expires=%d", contact, expires)))
			resp.RemoveHeader("Contact") // remove the blank one we just added
			resp.AppendHeader(sip.NewHeader("Contact",
				fmt.Sprintf("<%s>;expires=%d", contact, expires)))
		}
		resp.AppendHeader(sip.NewHeader("Date", time.Now().UTC().Format(time.RFC1123)))

		if err := tx.Respond(resp); err != nil {
			slog.Error("REGISTER respond error", "err", err)
		}
	}
}

// ── OPTIONS handler (keep-alive / NAT pings) ─────────────────────────────────

func handleOptions() sipgo.RequestHandler {
	return func(req *sip.Request, tx sip.ServerTransaction) {
		slog.Debug("← OPTIONS", "src", req.Source())
		resp := sip.NewResponseFromRequest(req, sip.StatusOK, "OK", nil)
		resp.AppendHeader(sip.NewHeader("Allow", "INVITE, ACK, BYE, CANCEL, OPTIONS, REGISTER"))
		resp.AppendHeader(sip.NewHeader("Accept", "application/sdp"))
		tx.Respond(resp)
	}
}

// ── INVITE handler (call routing via diago B2BUA) ────────────────────────────

func handleInvite(dg *diago.Diago, registry *Registry) diago.ServeDialogFunc {
	return func(inDialog *diago.DialogServerSession) {
		toUser := inDialog.ToUser()
		fromUser := inDialog.FromUser()

		slog.Info("← INVITE", "from", fromUser, "to", toUser)

		// Send 100 Trying
		if err := inDialog.Trying(); err != nil {
			slog.Error("Trying failed", "err", err)
			return
		}

		// Find callee in registry
		toURI := inDialog.InviteRequest.To().Address.String()
		reg, found := registry.Lookup(toURI)
		if !found {
			// Try a plain username lookup as fallback
			reg, found = registry.Lookup("sip:" + toUser + "@127.0.0.1")
		}

		if !found {
			slog.Warn("Callee not registered – 480", "to", toUser)
			inDialog.Respond(sip.StatusTemporarilyUnavailable, "Temporarily Unavailable", nil)
			return
		}

		slog.Info("Routing call", "from", fromUser, "to", toUser, "contact", reg.Contact)

		// Send 180 Ringing
		if err := inDialog.Ringing(); err != nil {
			slog.Error("Ringing failed", "err", err)
			return
		}

		// Build destination URI
		destURI, err := sip.ParseUri(reg.Contact)
		if err != nil {
			slog.Error("Bad contact URI", "contact", reg.Contact, "err", err)
			inDialog.Respond(sip.StatusServerInternalError, "Bad contact", nil)
			return
		}

		// Bridge the two legs
		bridge := diago.NewBridge()
		if err := bridge.AddDialogSession(inDialog); err != nil {
			slog.Error("Bridge add server leg failed", "err", err)
			return
		}

		ctx := inDialog.Context()
		outDialog, err := dg.InviteBridge(ctx, destURI, bridge, diago.InviteOptions{})
		if err != nil {
			slog.Warn("Outbound INVITE failed", "err", err)
			// The error response code is already handled by diago
			return
		}
		defer outDialog.Close()

		slog.Info("✅  Call bridged", "from", fromUser, "to", toUser)

		// Hold the goroutine until one side hangs up
		select {
		case <-inDialog.Context().Done():
			slog.Info("Caller hung up", "from", fromUser)
		case <-outDialog.Context().Done():
			slog.Info("Callee hung up", "to", toUser)
		}
	}
}

// ── HTTP status API ──────────────────────────────────────────────────────────

func serveHTTP(port string, registry *Registry) {
	mux := http.NewServeMux()

	// GET /registrations – list all active registrations as JSON
	mux.HandleFunc("/registrations", func(w http.ResponseWriter, r *http.Request) {
		regs := registry.List()
		w.Header().Set("Content-Type", "application/json")
		fmt.Fprint(w, "[\n")
		for i, reg := range regs {
			ttl := time.Until(reg.ExpiresAt).Round(time.Second)
			fmt.Fprintf(w, "  {\"aor\":%q, \"contact\":%q, \"remote_ip\":%q, \"ttl\":%q}",
				reg.AOR, reg.Contact, reg.RemoteIP, ttl.String())
			if i < len(regs)-1 {
				fmt.Fprint(w, ",")
			}
			fmt.Fprint(w, "\n")
		}
		fmt.Fprint(w, "]\n")
	})

	// GET /health
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintln(w, `{"status":"ok"}`)
	})

	slog.Info("HTTP API listening", "port", port)
	if err := http.ListenAndServe(":"+port, mux); err != nil {
		slog.Error("HTTP server error", "err", err)
	}
}

// ── Helpers ──────────────────────────────────────────────────────────────────

func envOr(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func mustInt(s string) int {
	v, err := strconv.Atoi(s)
	if err != nil {
		panic(err)
	}
	return v
}
