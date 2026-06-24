package main

import (
	"log"
	"net/http"
	"os"
	"time"

	_ "github.com/go-sql-driver/mysql"
)

func main() {
	cfg, err := LoadConfig(os.Getenv)
	if err != nil {
		log.Fatalf("config: %v", err)
	}
	store, err := NewMySQLStore(cfg.DBDSN)
	if err != nil {
		log.Fatalf("database: %v", err)
	}
	app := NewApp(
		NewService(store, cfg.BotPrefix, 500),
		NewSessionManager(cfg.SessionSecret, 7*24*time.Hour),
		NewRateLimiter(20, time.Minute),
		cfg,
	)
	srv := &http.Server{
		Addr:              cfg.Listen,
		Handler:           app.Routes(),
		ReadHeaderTimeout: 10 * time.Second,
		// no WriteTimeout: large client downloads can take a long time.
	}
	log.Printf("ac-webreg listening on %s", cfg.Listen)
	if err := srv.ListenAndServe(); err != nil {
		log.Fatal(err)
	}
}
