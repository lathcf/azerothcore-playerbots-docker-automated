package main

import (
	"crypto/subtle"
	"embed"
	"errors"
	"html/template"
	"log"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
)

//go:embed templates/*.html
var templateFS embed.FS

//go:embed static/*
var staticFS embed.FS

type App struct {
	svc      *Service
	sessions *SessionManager
	limiter  *RateLimiter
	cfg      Config
	tmpl     *template.Template
}

func NewApp(svc *Service, sm *SessionManager, rl *RateLimiter, cfg Config) *App {
	return &App{
		svc: svc, sessions: sm, limiter: rl, cfg: cfg,
		tmpl: template.Must(template.ParseFS(templateFS, "templates/*.html")),
	}
}

type page struct {
	Title, ZipLabel, User, Error, Notice, Query string
	Accounts                                     []AccountInfo
}

func (a *App) render(w http.ResponseWriter, name string, status int, p page) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.WriteHeader(status)
	_ = a.tmpl.ExecuteTemplate(w, name, p)
}

// isSecure reports whether the user-facing connection is HTTPS: direct TLS, or the
// X-Forwarded-Proto header set by the Cloudflare tunnel. Used for the cookie Secure
// flag so login works over plain-HTTP LAN access yet stays Secure via the tunnel.
func isSecure(r *http.Request) bool {
	return r.TLS != nil || strings.EqualFold(r.Header.Get("X-Forwarded-Proto"), "https")
}

// clientIP returns the caller's IP. CF-Connecting-IP is only trustworthy when the
// request arrives via the Cloudflare tunnel; a direct LAN client can spoof it. That is
// accepted here (LAN is trusted; remote is gated by Cloudflare Access). Falls back to
// RemoteAddr when the header is absent.
func clientIP(r *http.Request) string {
	if ip := r.Header.Get("CF-Connecting-IP"); ip != "" {
		return ip
	}
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}

// limit returns false (and writes 429) when the caller is over budget.
func (a *App) limit(w http.ResponseWriter, r *http.Request) bool {
	if a.limiter.Allow(clientIP(r), time.Now()) {
		return true
	}
	http.Error(w, "Too many requests, slow down.", http.StatusTooManyRequests)
	return false
}

// userMessage maps an error to safe user-facing text, logging anything unexpected.
func userMessage(err error) string {
	var ve ValidationError
	if errors.As(err, &ve) {
		return ve.Msg
	}
	switch {
	case errors.Is(err, ErrUserExists):
		return "That account name is already taken."
	case errors.Is(err, ErrBadCredentials):
		return "Current password is incorrect."
	default:
		log.Printf("webreg: internal error: %v", err)
		return "Something went wrong. Please try again."
	}
}

func (a *App) Routes() http.Handler {
	mux := http.NewServeMux()
	// no-cache so CSS/template style changes land immediately after a redeploy
	// (embedded files carry no modtime/ETag, so browsers would otherwise cache them).
	static := http.FileServer(http.FS(staticFS))
	mux.Handle("/static/", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-cache")
		static.ServeHTTP(w, r)
	}))
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusOK)
		_, _ = w.Write([]byte("ok"))
	})
	mux.HandleFunc("/", a.handleIndex)
	mux.HandleFunc("/register", a.handleRegister)
	mux.HandleFunc("/login", a.handleLogin)
	mux.HandleFunc("/logout", a.handleLogout)
	mux.HandleFunc("/account", a.handleAccount)
	mux.HandleFunc("/account/password", a.handleChangePassword)
	mux.HandleFunc("/download", a.handleDownload)
	mux.HandleFunc("/admin", a.basicAuth(a.handleAdmin))
	mux.HandleFunc("/admin/reset", a.basicAuth(a.handleAdminReset))
	mux.HandleFunc("/admin/ban", a.basicAuth(a.handleBan))
	mux.HandleFunc("/admin/unban", a.basicAuth(a.handleUnban))
	return mux
}

func (a *App) handleIndex(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	a.render(w, "index.html", http.StatusOK, page{Title: "Thrashernet WoW", ZipLabel: a.cfg.ClientZipLabel})
}

func (a *App) handleRegister(w http.ResponseWriter, r *http.Request) {
	if !a.limit(w, r) {
		return
	}
	if r.Method == http.MethodGet {
		a.render(w, "register.html", http.StatusOK, page{})
		return
	}
	username, password := r.FormValue("username"), r.FormValue("password")
	if err := a.svc.Register(r.Context(), username, password); err != nil {
		a.render(w, "register.html", http.StatusBadRequest, page{Error: userMessage(err)})
		return
	}
	a.sessions.SetCookie(w, normalize(username), isSecure(r))
	http.Redirect(w, r, "/account", http.StatusSeeOther)
}

func (a *App) handleLogin(w http.ResponseWriter, r *http.Request) {
	if !a.limit(w, r) {
		return
	}
	if r.Method == http.MethodGet {
		a.render(w, "login.html", http.StatusOK, page{})
		return
	}
	username, password := r.FormValue("username"), r.FormValue("password")
	ok, err := a.svc.Authenticate(r.Context(), username, password)
	if err != nil || !ok {
		a.render(w, "login.html", http.StatusUnauthorized, page{Error: "Incorrect account name or password."})
		return
	}
	a.sessions.SetCookie(w, normalize(username), isSecure(r))
	http.Redirect(w, r, "/account", http.StatusSeeOther)
}

func (a *App) handleLogout(w http.ResponseWriter, r *http.Request) {
	a.sessions.Clear(w, isSecure(r))
	http.Redirect(w, r, "/", http.StatusSeeOther)
}

func (a *App) handleAccount(w http.ResponseWriter, r *http.Request) {
	user, ok := a.sessions.User(r)
	if !ok {
		http.Redirect(w, r, "/login", http.StatusSeeOther)
		return
	}
	a.render(w, "account.html", http.StatusOK, page{User: user})
}

func (a *App) handleChangePassword(w http.ResponseWriter, r *http.Request) {
	user, ok := a.sessions.User(r)
	if !ok {
		http.Redirect(w, r, "/login", http.StatusSeeOther)
		return
	}
	if !a.limit(w, r) {
		return
	}
	err := a.svc.ChangePassword(r.Context(), user, r.FormValue("current"), r.FormValue("password"))
	if err != nil {
		a.render(w, "account.html", http.StatusBadRequest, page{User: user, Error: userMessage(err)})
		return
	}
	a.render(w, "account.html", http.StatusOK, page{User: user, Notice: "Password updated."})
}

func (a *App) handleDownload(w http.ResponseWriter, r *http.Request) {
	if a.cfg.ClientZipPath == "" {
		http.Error(w, "No client download is configured.", http.StatusNotFound)
		return
	}
	f, err := os.Open(a.cfg.ClientZipPath)
	if err != nil {
		http.Error(w, "Client download is unavailable.", http.StatusNotFound)
		return
	}
	defer f.Close()
	info, err := f.Stat()
	if err != nil {
		http.Error(w, "Client download is unavailable.", http.StatusInternalServerError)
		return
	}
	if info.IsDir() || !info.Mode().IsRegular() {
		http.Error(w, "No client download is available.", http.StatusNotFound)
		return
	}
	name := info.Name()
	w.Header().Set("Content-Disposition", "attachment; filename=\""+name+"\"")
	// ServeContent handles Range/resume and conditional requests.
	http.ServeContent(w, r, name, info.ModTime(), f)
}

func (a *App) basicAuth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		u, p, ok := r.BasicAuth()
		userOK := subtle.ConstantTimeCompare([]byte(u), []byte(a.cfg.AdminUser)) == 1
		passOK := subtle.ConstantTimeCompare([]byte(p), []byte(a.cfg.AdminPass)) == 1
		if !ok || !userOK || !passOK {
			w.Header().Set("WWW-Authenticate", `Basic realm="admin"`)
			http.Error(w, "Unauthorized", http.StatusUnauthorized)
			return
		}
		next(w, r)
	}
}

// adminQuery returns the current search term ('q'), from the form on POST or the
// query string on GET, so the list re-renders filtered the same way after an action.
func adminQuery(r *http.Request) string {
	if r.Method == http.MethodPost {
		return r.FormValue("q")
	}
	return r.URL.Query().Get("q")
}

// renderAdmin loads the (filtered) account list and renders the admin page. Any
// message in p is preserved; a list-load failure surfaces as an error.
func (a *App) renderAdmin(w http.ResponseWriter, r *http.Request, status int, p page) {
	p.Query = adminQuery(r)
	accts, err := a.svc.ListAccounts(r.Context(), p.Query)
	if err != nil {
		log.Printf("webreg: list accounts: %v", err)
		if p.Error == "" {
			p.Error = "Could not load accounts."
		}
		if status == http.StatusOK {
			status = http.StatusInternalServerError
		}
	}
	p.Accounts = accts
	a.render(w, "admin.html", status, p)
}

func (a *App) handleAdmin(w http.ResponseWriter, r *http.Request) {
	a.renderAdmin(w, r, http.StatusOK, page{})
}

func (a *App) handleAdminReset(w http.ResponseWriter, r *http.Request) {
	username, password := r.FormValue("username"), r.FormValue("password")
	found, err := a.svc.ResetPassword(r.Context(), username, password)
	switch {
	case err != nil:
		a.renderAdmin(w, r, http.StatusBadRequest, page{Error: userMessage(err)})
	case !found:
		a.renderAdmin(w, r, http.StatusNotFound, page{Error: "No such account: " + username})
	default:
		a.renderAdmin(w, r, http.StatusOK, page{Notice: "Password reset for " + strings.ToUpper(username) + "."})
	}
}

func (a *App) handleBan(w http.ResponseWriter, r *http.Request) {
	username := r.FormValue("username")
	found, err := a.svc.BanAccount(r.Context(), username)
	switch {
	case err != nil:
		a.renderAdmin(w, r, http.StatusBadRequest, page{Error: userMessage(err)})
	case !found:
		a.renderAdmin(w, r, http.StatusNotFound, page{Error: "No such account: " + username})
	default:
		a.renderAdmin(w, r, http.StatusOK, page{Notice: strings.ToUpper(username) + " is banned — login blocked."})
	}
}

func (a *App) handleUnban(w http.ResponseWriter, r *http.Request) {
	username := r.FormValue("username")
	found, err := a.svc.UnbanAccount(r.Context(), username)
	switch {
	case err != nil:
		a.renderAdmin(w, r, http.StatusBadRequest, page{Error: userMessage(err)})
	case !found:
		a.renderAdmin(w, r, http.StatusOK, page{Notice: "No active ban on " + strings.ToUpper(username) + "."})
	default:
		a.renderAdmin(w, r, http.StatusOK, page{Notice: strings.ToUpper(username) + " is unbanned."})
	}
}
