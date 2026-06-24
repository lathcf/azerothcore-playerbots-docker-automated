package main

import (
	"testing"
	"time"
)

func TestRateLimiterAllowsUpToMax(t *testing.T) {
	rl := NewRateLimiter(3, time.Minute)
	now := time.Unix(1_700_000_000, 0)
	for i := 0; i < 3; i++ {
		if !rl.Allow("1.2.3.4", now) {
			t.Fatalf("request %d should be allowed", i+1)
		}
	}
	if rl.Allow("1.2.3.4", now) {
		t.Error("4th request in window must be blocked")
	}
}

func TestRateLimiterResetsAfterWindow(t *testing.T) {
	rl := NewRateLimiter(1, time.Minute)
	now := time.Unix(1_700_000_000, 0)
	if !rl.Allow("1.2.3.4", now) {
		t.Fatal("first request should be allowed")
	}
	if rl.Allow("1.2.3.4", now) {
		t.Fatal("second request in window should be blocked")
	}
	if !rl.Allow("1.2.3.4", now.Add(2*time.Minute)) {
		t.Error("request after window should be allowed")
	}
}

func TestRateLimiterPerKey(t *testing.T) {
	rl := NewRateLimiter(1, time.Minute)
	now := time.Unix(1_700_000_000, 0)
	if !rl.Allow("a", now) || !rl.Allow("b", now) {
		t.Error("different keys must have independent budgets")
	}
}

func TestRateLimiterEvictsExpiredKeys(t *testing.T) {
	rl := NewRateLimiter(1, time.Minute)
	now := time.Unix(1_700_000_000, 0)
	rl.Allow("old", now)
	// Advance past the window and touch a different key; the old key should be swept.
	rl.Allow("new", now.Add(2*time.Minute))
	rl.mu.Lock()
	_, stillThere := rl.hits["old"]
	rl.mu.Unlock()
	if stillThere {
		t.Error("expired key should have been evicted")
	}
}
