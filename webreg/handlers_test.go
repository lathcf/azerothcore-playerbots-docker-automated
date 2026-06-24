package main

import (
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
	"time"
)

func testApp() *App {
	cfg := Config{
		AdminUser: "admin", AdminPass: "adminpw",
		ClientZipLabel: "Download client",
	}
	return NewApp(newSvc(newFakeStore()),
		NewSessionManager([]byte("0123456789abcdef"), time.Hour),
		NewRateLimiter(100, time.Minute), cfg)
}

func TestHealthz(t *testing.T) {
	rec := httptest.NewRecorder()
	testApp().Routes().ServeHTTP(rec, httptest.NewRequest("GET", "/healthz", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("healthz = %d, want 200", rec.Code)
	}
}

func TestRegisterThenLoginFlow(t *testing.T) {
	app := testApp()
	form := url.Values{"username": {"Bob"}, "password": {"hunter2"}}
	req := httptest.NewRequest("POST", "/register", strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	rec := httptest.NewRecorder()
	app.Routes().ServeHTTP(rec, req)
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("register = %d, want 303 redirect", rec.Code)
	}
	if len(rec.Result().Cookies()) == 0 {
		t.Fatal("register should set a session cookie")
	}
}

func TestAccountRequiresSession(t *testing.T) {
	rec := httptest.NewRecorder()
	testApp().Routes().ServeHTTP(rec, httptest.NewRequest("GET", "/account", nil))
	if rec.Code != http.StatusSeeOther {
		t.Fatalf("unauthenticated /account = %d, want 303 to /login", rec.Code)
	}
}

func TestAdminRequiresBasicAuth(t *testing.T) {
	app := testApp()
	rec := httptest.NewRecorder()
	app.Routes().ServeHTTP(rec, httptest.NewRequest("GET", "/admin", nil))
	if rec.Code != http.StatusUnauthorized {
		t.Fatalf("/admin without auth = %d, want 401", rec.Code)
	}
	req := httptest.NewRequest("GET", "/admin", nil)
	req.SetBasicAuth("admin", "adminpw")
	rec = httptest.NewRecorder()
	app.Routes().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("/admin with auth = %d, want 200", rec.Code)
	}
}

func TestSessionCookieSecureFollowsScheme(t *testing.T) {
	app := testApp()
	// Register an account in this app so we can log in against it.
	regForm := url.Values{"username": {"Bob"}, "password": {"hunter2"}}.Encode()
	rr := httptest.NewRequest("POST", "/register", strings.NewReader(regForm))
	rr.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	app.Routes().ServeHTTP(httptest.NewRecorder(), rr)

	loginCookie := func(httpsProto bool) *http.Cookie {
		f := url.Values{"username": {"Bob"}, "password": {"hunter2"}}.Encode()
		req := httptest.NewRequest("POST", "/login", strings.NewReader(f))
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		if httpsProto {
			req.Header.Set("X-Forwarded-Proto", "https")
		}
		rec := httptest.NewRecorder()
		app.Routes().ServeHTTP(rec, req)
		for _, c := range rec.Result().Cookies() {
			if c.Name == sessionCookieName {
				return c
			}
		}
		t.Fatal("login set no session cookie")
		return nil
	}

	if loginCookie(false).Secure {
		t.Error("session cookie must NOT be Secure over plain HTTP (LAN access would break)")
	}
	if !loginCookie(true).Secure {
		t.Error("session cookie must be Secure when X-Forwarded-Proto=https (tunnel)")
	}
}

func TestRateLimitRegister(t *testing.T) {
	app := NewApp(newSvc(newFakeStore()),
		NewSessionManager([]byte("0123456789abcdef"), time.Hour),
		NewRateLimiter(1, time.Minute), Config{AdminUser: "a", AdminPass: "b"})
	do := func() int {
		req := httptest.NewRequest("GET", "/register", nil)
		req.Header.Set("CF-Connecting-IP", "9.9.9.9")
		rec := httptest.NewRecorder()
		app.Routes().ServeHTTP(rec, req)
		return rec.Code
	}
	if do() != http.StatusOK {
		t.Fatal("first request should pass")
	}
	if do() != http.StatusTooManyRequests {
		t.Error("second request from same IP should be rate limited")
	}
}

// registerVia creates an account through the public register endpoint.
func registerVia(app *App, username string) {
	f := url.Values{"username": {username}, "password": {"hunter2"}}.Encode()
	req := httptest.NewRequest("POST", "/register", strings.NewReader(f))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	app.Routes().ServeHTTP(httptest.NewRecorder(), req)
}

// adminPost posts a form to an /admin* endpoint with Basic Auth.
func adminPost(app *App, path string, form url.Values) *httptest.ResponseRecorder {
	req := httptest.NewRequest("POST", path, strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	req.SetBasicAuth("admin", "adminpw")
	rec := httptest.NewRecorder()
	app.Routes().ServeHTTP(rec, req)
	return rec
}

func TestAdminListsRealAccounts(t *testing.T) {
	app := testApp()
	registerVia(app, "Alice")

	req := httptest.NewRequest("GET", "/admin", nil)
	req.SetBasicAuth("admin", "adminpw")
	rec := httptest.NewRecorder()
	app.Routes().ServeHTTP(rec, req)
	if rec.Code != http.StatusOK {
		t.Fatalf("/admin = %d, want 200", rec.Code)
	}
	if !strings.Contains(rec.Body.String(), "ALICE") {
		t.Error("admin page should list the registered account ALICE")
	}
}

func TestAdminBanThenUnban(t *testing.T) {
	app := testApp()
	registerVia(app, "Bob")

	rec := adminPost(app, "/admin/ban", url.Values{"username": {"Bob"}})
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "Banned") {
		t.Fatalf("ban: code=%d, expected re-rendered list showing Banned", rec.Code)
	}
	rec = adminPost(app, "/admin/unban", url.Values{"username": {"Bob"}})
	if rec.Code != http.StatusOK || !strings.Contains(rec.Body.String(), "Active") {
		t.Fatalf("unban: code=%d, expected re-rendered list showing Active", rec.Code)
	}
}

func TestAdminBanRoutesRequireAuth(t *testing.T) {
	app := testApp()
	for _, path := range []string{"/admin/ban", "/admin/unban"} {
		req := httptest.NewRequest("POST", path, strings.NewReader("username=Bob"))
		req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
		rec := httptest.NewRecorder()
		app.Routes().ServeHTTP(rec, req)
		if rec.Code != http.StatusUnauthorized {
			t.Errorf("%s without auth = %d, want 401", path, rec.Code)
		}
	}
}
