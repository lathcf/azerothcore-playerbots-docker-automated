package main

import (
	"testing"
	"time"
)

func TestSessionRoundTrip(t *testing.T) {
	m := NewSessionManager([]byte("test-secret"), time.Hour)
	now := time.Unix(1_700_000_000, 0)
	tok := m.Encode("Bob", now)
	user, ok := m.Decode(tok, now.Add(time.Minute))
	if !ok || user != "Bob" {
		t.Fatalf("Decode = (%q,%v), want (Bob,true)", user, ok)
	}
}

func TestSessionExpired(t *testing.T) {
	m := NewSessionManager([]byte("test-secret"), time.Hour)
	now := time.Unix(1_700_000_000, 0)
	tok := m.Encode("Bob", now)
	if _, ok := m.Decode(tok, now.Add(2*time.Hour)); ok {
		t.Error("expired token must not decode")
	}
}

func TestSessionTampered(t *testing.T) {
	m := NewSessionManager([]byte("test-secret"), time.Hour)
	now := time.Unix(1_700_000_000, 0)
	tok := m.Encode("Bob", now)
	if _, ok := m.Decode(tok+"x", now); ok {
		t.Error("tampered token must not decode")
	}
	other := NewSessionManager([]byte("different-secret"), time.Hour)
	if _, ok := other.Decode(tok, now); ok {
		t.Error("token signed with another secret must not decode")
	}
}
