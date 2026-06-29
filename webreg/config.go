package main

import (
	"errors"
	"fmt"
)

type Config struct {
	Listen         string
	SiteName       string
	AdminUser      string
	AdminPass      string
	SessionSecret  []byte
	ClientZipPath  string
	ClientZipLabel string
	AddonsZipPath  string
	AddonsZipLabel string
	BotPrefix      string
	DBDSN          string
}

func or(v, def string) string {
	if v == "" {
		return def
	}
	return v
}

// LoadConfig reads configuration from getenv (os.Getenv in production).
func LoadConfig(getenv func(string) string) (Config, error) {
	cfg := Config{
		Listen:         or(getenv("WEBREG_LISTEN"), "0.0.0.0:8090"),
		SiteName:       or(getenv("WEBREG_SITE_NAME"), "WoW Server"),
		AdminUser:      or(getenv("WEBREG_ADMIN_USER"), "admin"),
		AdminPass:      getenv("WEBREG_ADMIN_PASS"),
		SessionSecret:  []byte(getenv("WEBREG_SESSION_SECRET")),
		ClientZipPath:  getenv("WEBREG_CLIENT_ZIP_PATH"),
		ClientZipLabel: or(getenv("WEBREG_CLIENT_ZIP_LABEL"), "Download client"),
		AddonsZipPath:  getenv("WEBREG_ADDONS_ZIP_PATH"),
		AddonsZipLabel: or(getenv("WEBREG_ADDONS_ZIP_LABEL"), "Download bot addons"),
		BotPrefix:      or(getenv("WEBREG_BOT_PREFIX"), "rndbot"),
	}
	if cfg.AdminPass == "" {
		return Config{}, errors.New("WEBREG_ADMIN_PASS is required")
	}
	if len(cfg.SessionSecret) < 16 {
		return Config{}, errors.New("WEBREG_SESSION_SECRET must be at least 16 bytes")
	}
	// parseTime=true so DATE/TIMESTAMP columns (joindate, last_login) scan into time.Time.
	cfg.DBDSN = fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?timeout=5s&parseTime=true",
		or(getenv("WEBREG_DB_USER"), "webreg"),
		getenv("WEBREG_DB_PASS"),
		or(getenv("WEBREG_DB_HOST"), "ac-database"),
		or(getenv("WEBREG_DB_PORT"), "3306"),
		or(getenv("WEBREG_DB_NAME"), "acore_auth"),
	)
	return cfg, nil
}
