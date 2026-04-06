package main

import (
	"log/slog"
	"sync"
	"time"
)

// Registration holds a single AOR → contact binding
type Registration struct {
	AOR       string    // e.g. sip:alice@127.0.0.1
	Contact   string    // e.g. sip:alice@192.168.1.50:5061
	ExpiresAt time.Time
	RemoteIP  string
}

// Registry is a thread-safe in-memory store for SIP registrations
type Registry struct {
	mu   sync.RWMutex
	regs map[string]*Registration // keyed by AOR
}

func NewRegistry() *Registry {
	r := &Registry{regs: make(map[string]*Registration)}
	go r.reaper() // background goroutine to clean expired registrations
	return r
}

// Store saves or refreshes a registration binding
func (r *Registry) Store(aor, contact, remoteIP string, expires int) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if expires == 0 {
		delete(r.regs, aor)
		slog.Info("Unregistered", "aor", aor)
		return
	}

	r.regs[aor] = &Registration{
		AOR:       aor,
		Contact:   contact,
		ExpiresAt: time.Now().Add(time.Duration(expires) * time.Second),
		RemoteIP:  remoteIP,
	}
	slog.Info("Registered", "aor", aor, "contact", contact, "expires", expires)
}

// Lookup finds a live (non-expired) registration for an AOR.
// Falls back to a username-only match in case the host differs.
func (r *Registry) Lookup(aor string) (*Registration, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	if reg, ok := r.regs[aor]; ok && time.Now().Before(reg.ExpiresAt) {
		return reg, true
	}

	// Fuzzy match by the user part (sip:bob@anything)
	user := aorUser(aor)
	for k, reg := range r.regs {
		if aorUser(k) == user && time.Now().Before(reg.ExpiresAt) {
			return reg, true
		}
	}
	return nil, false
}

// List returns a snapshot of all live registrations
func (r *Registry) List() []Registration {
	r.mu.RLock()
	defer r.mu.RUnlock()

	out := make([]Registration, 0, len(r.regs))
	for _, reg := range r.regs {
		if time.Now().Before(reg.ExpiresAt) {
			out = append(out, *reg)
		}
	}
	return out
}

// reaper removes expired registrations every 30 seconds
func (r *Registry) reaper() {
	t := time.NewTicker(30 * time.Second)
	defer t.Stop()
	for range t.C {
		r.mu.Lock()
		for k, reg := range r.regs {
			if time.Now().After(reg.ExpiresAt) {
				delete(r.regs, k)
				slog.Info("Registration expired (reaped)", "aor", k)
			}
		}
		r.mu.Unlock()
	}
}

// aorUser extracts the user part from a SIP URI (sip:user@host → user)
func aorUser(aor string) string {
	// Strip leading "sip:" or "sips:"
	s := aor
	for _, prefix := range []string{"sips:", "sip:"} {
		if len(s) > len(prefix) && s[:len(prefix)] == prefix {
			s = s[len(prefix):]
			break
		}
	}
	// user@host → user
	for i, c := range s {
		if c == '@' {
			return s[:i]
		}
	}
	return s
}
