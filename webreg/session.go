package main

import (
	"crypto/hmac"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"net/http"
	"strconv"
	"strings"
	"time"
)

const sessionCookieName = "webreg_session"

type SessionManager struct {
	secret []byte
	ttl    time.Duration
}

func NewSessionManager(secret []byte, ttl time.Duration) *SessionManager {
	return &SessionManager{secret: secret, ttl: ttl}
}

func (m *SessionManager) sign(payload string) string {
	mac := hmac.New(sha256.New, m.secret)
	mac.Write([]byte(payload))
	return base64.RawURLEncoding.EncodeToString(mac.Sum(nil))
}

// Encode builds "<b64(user|expiryUnix)>.<b64(hmac)>".
func (m *SessionManager) Encode(username string, now time.Time) string {
	exp := now.Add(m.ttl).Unix()
	payload := base64.RawURLEncoding.EncodeToString(
		[]byte(username + "|" + strconv.FormatInt(exp, 10)))
	return payload + "." + m.sign(payload)
}

// Decode returns the username if the signature is valid and not expired.
func (m *SessionManager) Decode(token string, now time.Time) (string, bool) {
	parts := strings.SplitN(token, ".", 2)
	if len(parts) != 2 {
		return "", false
	}
	if subtle.ConstantTimeCompare([]byte(parts[1]), []byte(m.sign(parts[0]))) != 1 {
		return "", false
	}
	raw, err := base64.RawURLEncoding.DecodeString(parts[0])
	if err != nil {
		return "", false
	}
	fields := strings.SplitN(string(raw), "|", 2)
	if len(fields) != 2 {
		return "", false
	}
	exp, err := strconv.ParseInt(fields[1], 10, 64)
	if err != nil || now.Unix() >= exp {
		return "", false
	}
	return fields[0], true
}

// SetCookie writes the session cookie. secure should be true only when the
// user-facing connection is HTTPS — a Secure cookie is dropped by browsers over
// plain HTTP, which breaks login on direct LAN access.
func (m *SessionManager) SetCookie(w http.ResponseWriter, username string, secure bool) {
	http.SetCookie(w, &http.Cookie{
		Name:     sessionCookieName,
		Value:    m.Encode(username, time.Now()),
		Path:     "/",
		HttpOnly: true,
		Secure:   secure,
		SameSite: http.SameSiteLaxMode,
		MaxAge:   int(m.ttl.Seconds()),
	})
}

// Clear expires the session cookie. secure must match how it was set so the
// deletion cookie isn't itself dropped over plain HTTP.
func (m *SessionManager) Clear(w http.ResponseWriter, secure bool) {
	http.SetCookie(w, &http.Cookie{
		Name: sessionCookieName, Value: "", Path: "/",
		HttpOnly: true, Secure: secure, SameSite: http.SameSiteLaxMode, MaxAge: -1,
	})
}

// User returns the logged-in username from the request cookie, if valid.
func (m *SessionManager) User(r *http.Request) (string, bool) {
	c, err := r.Cookie(sessionCookieName)
	if err != nil {
		return "", false
	}
	return m.Decode(c.Value, time.Now())
}
