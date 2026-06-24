package main

import (
	"context"
	"errors"
	"strings"
)

var (
	ErrBadCredentials = errors.New("incorrect username or password")
)

type Service struct {
	store     Store
	botPrefix string // accounts with this username prefix are hidden from the admin list
	listLimit int    // cap on rows returned by ListAccounts
}

func NewService(store Store, botPrefix string, listLimit int) *Service {
	return &Service{store: store, botPrefix: botPrefix, listLimit: listLimit}
}

func normalize(username string) string { return strings.ToUpper(username) }

func (s *Service) Register(ctx context.Context, username, password string) error {
	if err := ValidateUsername(username); err != nil {
		return err
	}
	if err := ValidatePassword(password); err != nil {
		return err
	}
	u := normalize(username)
	salt, err := MakeSalt()
	if err != nil {
		return err
	}
	return s.store.CreateAccount(ctx, u, salt, CalculateVerifier(u, password, salt))
}

func (s *Service) Authenticate(ctx context.Context, username, password string) (bool, error) {
	salt, verifier, found, err := s.store.GetAccount(ctx, normalize(username))
	if err != nil || !found {
		return false, err
	}
	return VerifyPassword(normalize(username), password, salt, verifier), nil
}

func (s *Service) ChangePassword(ctx context.Context, username, current, newPassword string) error {
	ok, err := s.Authenticate(ctx, username, current)
	if err != nil {
		return err
	}
	if !ok {
		return ErrBadCredentials
	}
	if err := ValidatePassword(newPassword); err != nil {
		return err
	}
	return s.setPassword(ctx, normalize(username), newPassword)
}

// ResetPassword sets a new password without the current one (admin use).
func (s *Service) ResetPassword(ctx context.Context, username, newPassword string) (bool, error) {
	if err := ValidatePassword(newPassword); err != nil {
		return false, err
	}
	u := normalize(username)
	_, _, found, err := s.store.GetAccount(ctx, u)
	if err != nil || !found {
		return false, err
	}
	if err := s.setPassword(ctx, u, newPassword); err != nil {
		return false, err
	}
	return true, nil
}

func (s *Service) setPassword(ctx context.Context, normalizedUser, password string) error {
	salt, err := MakeSalt()
	if err != nil {
		return err
	}
	_, err = s.store.UpdateVerifier(ctx, normalizedUser, salt, CalculateVerifier(normalizedUser, password, salt))
	return err
}

// ListAccounts returns real (non-bot) accounts, optionally filtered by a name search.
func (s *Service) ListAccounts(ctx context.Context, search string) ([]AccountInfo, error) {
	return s.store.ListAccounts(ctx, s.botPrefix, strings.TrimSpace(search), s.listLimit)
}

// BanAccount blocks login for the account. found is false when no such account exists.
func (s *Service) BanAccount(ctx context.Context, username string) (bool, error) {
	return s.store.BanAccount(ctx, normalize(username), "webreg-admin", "Banned via admin site")
}

// UnbanAccount restores login. found is false when there was no active ban.
func (s *Service) UnbanAccount(ctx context.Context, username string) (bool, error) {
	return s.store.UnbanAccount(ctx, normalize(username))
}
