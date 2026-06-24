package main

import (
	"context"
	"errors"
	"strings"
	"testing"
)

type fakeStore struct {
	rows   map[string][2][]byte // username -> {salt, verifier}
	banned map[string]bool      // username -> active ban
	order  []string             // insertion order, for stable listing
}

func newFakeStore() *fakeStore {
	return &fakeStore{rows: map[string][2][]byte{}, banned: map[string]bool{}}
}

// newSvc builds a Service over the given store with the production bot prefix + cap.
func newSvc(s Store) *Service { return NewService(s, "rndbot", 500) }

func (f *fakeStore) CreateAccount(_ context.Context, u string, salt, v []byte) error {
	if _, ok := f.rows[u]; ok {
		return ErrUserExists
	}
	f.rows[u] = [2][]byte{salt, v}
	f.order = append(f.order, u)
	return nil
}
func (f *fakeStore) GetAccount(_ context.Context, u string) ([]byte, []byte, bool, error) {
	r, ok := f.rows[u]
	if !ok {
		return nil, nil, false, nil
	}
	return r[0], r[1], true, nil
}
func (f *fakeStore) UpdateVerifier(_ context.Context, u string, salt, v []byte) (bool, error) {
	if _, ok := f.rows[u]; !ok {
		return false, nil
	}
	f.rows[u] = [2][]byte{salt, v}
	return true, nil
}
func (f *fakeStore) ListAccounts(_ context.Context, botPrefix, search string, limit int) ([]AccountInfo, error) {
	var out []AccountInfo
	for _, u := range f.order {
		if botPrefix != "" && strings.HasPrefix(strings.ToUpper(u), strings.ToUpper(botPrefix)) {
			continue
		}
		if search != "" && !strings.Contains(strings.ToUpper(u), strings.ToUpper(search)) {
			continue
		}
		out = append(out, AccountInfo{Username: u, Banned: f.banned[u]})
		if limit > 0 && len(out) >= limit {
			break
		}
	}
	return out, nil
}
func (f *fakeStore) BanAccount(_ context.Context, username, _, _ string) (bool, error) {
	if _, ok := f.rows[username]; !ok {
		return false, nil
	}
	f.banned[username] = true
	return true, nil
}
func (f *fakeStore) UnbanAccount(_ context.Context, username string) (bool, error) {
	if !f.banned[username] {
		return false, nil
	}
	f.banned[username] = false
	return true, nil
}

func TestRegisterStoresUppercaseAndVerifies(t *testing.T) {
	fs := newFakeStore()
	svc := newSvc(fs)
	ctx := context.Background()
	if err := svc.Register(ctx, "Bob", "hunter2"); err != nil {
		t.Fatal(err)
	}
	if _, ok := fs.rows["BOB"]; !ok {
		t.Fatal("username should be stored uppercase")
	}
	ok, err := svc.Authenticate(ctx, "bob", "hunter2")
	if err != nil || !ok {
		t.Errorf("Authenticate = (%v,%v), want (true,nil)", ok, err)
	}
	ok, _ = svc.Authenticate(ctx, "bob", "wrong")
	if ok {
		t.Error("wrong password must not authenticate")
	}
}

func TestRegisterRejectsBadInput(t *testing.T) {
	svc := newSvc(newFakeStore())
	if err := svc.Register(context.Background(), "x", "hunter2"); err == nil {
		t.Error("short username must be rejected")
	}
	if err := svc.Register(context.Background(), "Bob", "abc"); err == nil {
		t.Error("short password must be rejected")
	}
}

func TestRegisterDuplicate(t *testing.T) {
	svc := newSvc(newFakeStore())
	ctx := context.Background()
	_ = svc.Register(ctx, "Bob", "hunter2")
	if err := svc.Register(ctx, "bob", "hunter2"); !errors.Is(err, ErrUserExists) {
		t.Errorf("duplicate register = %v, want ErrUserExists", err)
	}
}

func TestChangePassword(t *testing.T) {
	svc := newSvc(newFakeStore())
	ctx := context.Background()
	_ = svc.Register(ctx, "Bob", "hunter2")
	if err := svc.ChangePassword(ctx, "Bob", "wrong", "newpass"); !errors.Is(err, ErrBadCredentials) {
		t.Errorf("wrong current password = %v, want ErrBadCredentials", err)
	}
	if err := svc.ChangePassword(ctx, "Bob", "hunter2", "newpass"); err != nil {
		t.Fatal(err)
	}
	ok, _ := svc.Authenticate(ctx, "Bob", "newpass")
	if !ok {
		t.Error("new password should authenticate after change")
	}
}

func TestResetPassword(t *testing.T) {
	svc := newSvc(newFakeStore())
	ctx := context.Background()
	_ = svc.Register(ctx, "Bob", "hunter2")
	found, err := svc.ResetPassword(ctx, "bob", "reset123")
	if err != nil || !found {
		t.Fatalf("ResetPassword found=%v err=%v", found, err)
	}
	ok, _ := svc.Authenticate(ctx, "Bob", "reset123")
	if !ok {
		t.Error("reset password should authenticate")
	}
	found, _ = svc.ResetPassword(ctx, "nobody", "reset123")
	if found {
		t.Error("reset of unknown account should report not found")
	}
}

func TestListAccountsHidesBotsAndSearches(t *testing.T) {
	fs := newFakeStore()
	svc := newSvc(fs)
	ctx := context.Background()
	_ = svc.Register(ctx, "Alice", "hunter2")
	_ = svc.Register(ctx, "Bob", "hunter2")
	_ = fs.CreateAccount(ctx, "RNDBOT42", nil, nil) // a playerbot account row

	list, err := svc.ListAccounts(ctx, "")
	if err != nil {
		t.Fatal(err)
	}
	if len(list) != 2 {
		t.Fatalf("want 2 real accounts (bots hidden), got %d: %v", len(list), list)
	}
	for _, a := range list {
		if a.Username == "RNDBOT42" {
			t.Error("bot account must be hidden from the list")
		}
	}

	list, _ = svc.ListAccounts(ctx, "ali")
	if len(list) != 1 || list[0].Username != "ALICE" {
		t.Errorf("search 'ali' should return only ALICE, got %v", list)
	}
}

func TestBanUnban(t *testing.T) {
	fs := newFakeStore()
	svc := newSvc(fs)
	ctx := context.Background()
	_ = svc.Register(ctx, "Bob", "hunter2")

	found, err := svc.BanAccount(ctx, "bob")
	if err != nil || !found {
		t.Fatalf("BanAccount = (%v,%v), want (true,nil)", found, err)
	}
	if !fs.banned["BOB"] {
		t.Error("BOB should be banned")
	}
	if found, _ := svc.BanAccount(ctx, "ghost"); found {
		t.Error("banning an unknown account should report not found")
	}

	found, err = svc.UnbanAccount(ctx, "bob")
	if err != nil || !found {
		t.Fatalf("UnbanAccount = (%v,%v), want (true,nil)", found, err)
	}
	if fs.banned["BOB"] {
		t.Error("BOB should be unbanned")
	}
	if found, _ := svc.UnbanAccount(ctx, "bob"); found {
		t.Error("unbanning an account with no active ban should report not found")
	}
}
