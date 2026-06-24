package main

import "testing"

func env(m map[string]string) func(string) string {
	return func(k string) string { return m[k] }
}

func TestLoadConfigDefaults(t *testing.T) {
	cfg, err := LoadConfig(env(map[string]string{
		"WEBREG_ADMIN_PASS":     "secret",
		"WEBREG_SESSION_SECRET": "abcdefghijklmnopqrstuvwxyz012345",
		"WEBREG_DB_PASS":        "dbpw",
	}))
	if err != nil {
		t.Fatal(err)
	}
	if cfg.Listen != "0.0.0.0:8090" {
		t.Errorf("Listen = %q, want default 0.0.0.0:8090", cfg.Listen)
	}
	if cfg.AdminUser != "admin" {
		t.Errorf("AdminUser = %q, want default admin", cfg.AdminUser)
	}
	if cfg.BotPrefix != "rndbot" {
		t.Errorf("BotPrefix = %q, want default rndbot", cfg.BotPrefix)
	}
	wantDSN := "webreg:dbpw@tcp(ac-database:3306)/acore_auth?timeout=5s&parseTime=true"
	if cfg.DBDSN != wantDSN {
		t.Errorf("DBDSN = %q, want %q", cfg.DBDSN, wantDSN)
	}
}

func TestLoadConfigRequiresAdminPass(t *testing.T) {
	_, err := LoadConfig(env(map[string]string{
		"WEBREG_SESSION_SECRET": "abcdefghijklmnopqrstuvwxyz012345",
		"WEBREG_DB_PASS":        "dbpw",
	}))
	if err == nil {
		t.Error("missing WEBREG_ADMIN_PASS must be an error")
	}
}

func TestLoadConfigRequiresSessionSecret(t *testing.T) {
	_, err := LoadConfig(env(map[string]string{
		"WEBREG_ADMIN_PASS": "secret",
		"WEBREG_DB_PASS":    "dbpw",
	}))
	if err == nil {
		t.Error("missing/short WEBREG_SESSION_SECRET must be an error")
	}
}
