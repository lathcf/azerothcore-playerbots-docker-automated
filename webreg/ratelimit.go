package main

import (
	"sync"
	"time"
)

// RateLimiter is a simple per-key fixed-window counter, safe for concurrent use.
type RateLimiter struct {
	max        int
	window     time.Duration
	mu         sync.Mutex
	hits       map[string]*windowCount
	lastSweep  time.Time
}

type windowCount struct {
	start time.Time
	count int
}

func NewRateLimiter(max int, window time.Duration) *RateLimiter {
	return &RateLimiter{max: max, window: window, hits: make(map[string]*windowCount)}
}

// Allow reports whether a request for key is permitted at time now.
func (rl *RateLimiter) Allow(key string, now time.Time) bool {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	// Periodically evict expired windows so the map stays bounded.
	if now.Sub(rl.lastSweep) >= rl.window {
		for k, wc := range rl.hits {
			if now.Sub(wc.start) >= rl.window {
				delete(rl.hits, k)
			}
		}
		rl.lastSweep = now
	}

	w := rl.hits[key]
	if w == nil || now.Sub(w.start) >= rl.window {
		rl.hits[key] = &windowCount{start: now, count: 1}
		return true
	}
	if w.count >= rl.max {
		return false
	}
	w.count++
	return true
}
