//go:build integration

package main

import (
	"context"
	"os"
	"testing"
)

// Run on the server:
//   WEBREG_TEST_DSN='root:<rootpw>@tcp(127.0.0.1:3306)/acore_auth' \
//     go test -tags=integration ./... -run TestMySQLStore -v
func TestMySQLStore(t *testing.T) {
	dsn := os.Getenv("WEBREG_TEST_DSN")
	if dsn == "" {
		t.Skip("set WEBREG_TEST_DSN to run the store integration test")
	}
	st, err := NewMySQLStore(dsn)
	if err != nil {
		t.Fatal(err)
	}
	ctx := context.Background()
	name := "ZZTEST_WEBREG"
	_, _ = st.db.ExecContext(ctx, "DELETE FROM account WHERE username = ?", name)
	t.Cleanup(func() { st.db.ExecContext(ctx, "DELETE FROM account WHERE username = ?", name) })

	salt, _ := MakeSalt()
	v := CalculateVerifier(name, "hunter2", salt)
	if err := st.CreateAccount(ctx, name, salt, v); err != nil {
		t.Fatalf("CreateAccount: %v", err)
	}
	if err := st.CreateAccount(ctx, name, salt, v); err != ErrUserExists {
		t.Fatalf("duplicate CreateAccount = %v, want ErrUserExists", err)
	}
	gotSalt, gotV, found, err := st.GetAccount(ctx, name)
	if err != nil || !found {
		t.Fatalf("GetAccount found=%v err=%v", found, err)
	}
	if !VerifyPassword(name, "hunter2", gotSalt, gotV) {
		t.Error("stored credentials should verify the original password")
	}
	salt2, _ := MakeSalt()
	v2 := CalculateVerifier(name, "newpass", salt2)
	found, err = st.UpdateVerifier(ctx, name, salt2, v2)
	if err != nil || !found {
		t.Fatalf("UpdateVerifier found=%v err=%v", found, err)
	}
	gotSalt, gotV, _, _ = st.GetAccount(ctx, name)
	if !VerifyPassword(name, "newpass", gotSalt, gotV) {
		t.Error("updated credentials should verify the new password")
	}
}

// Run on the server:
//   WEBREG_TEST_DSN='root:<rootpw>@tcp(127.0.0.1:3306)/acore_auth' \
//     go test -tags=integration ./... -run TestMySQLListBanUnban -v
func TestMySQLListBanUnban(t *testing.T) {
	dsn := os.Getenv("WEBREG_TEST_DSN")
	if dsn == "" {
		t.Skip("set WEBREG_TEST_DSN to run the store integration test")
	}
	st, err := NewMySQLStore(dsn)
	if err != nil {
		t.Fatal(err)
	}
	ctx := context.Background()
	name := "ZZTEST_ADMIN"
	cleanup := func() {
		st.db.ExecContext(ctx, "DELETE FROM account_banned WHERE id IN (SELECT id FROM account WHERE username = ?)", name)
		st.db.ExecContext(ctx, "DELETE FROM account WHERE username = ?", name)
	}
	cleanup()
	t.Cleanup(cleanup)

	salt, _ := MakeSalt()
	if err := st.CreateAccount(ctx, name, salt, CalculateVerifier(name, "hunter2", salt)); err != nil {
		t.Fatalf("CreateAccount: %v", err)
	}

	banned := func() *AccountInfo {
		list, err := st.ListAccounts(ctx, "rndbot", name, 100)
		if err != nil {
			t.Fatalf("ListAccounts: %v", err)
		}
		for i := range list {
			if list[i].Username == name {
				return &list[i]
			}
		}
		t.Fatalf("account %s not present in list", name)
		return nil
	}

	if ai := banned(); ai.Banned {
		t.Error("freshly created account should not be banned")
	}
	if ok, err := st.BanAccount(ctx, name, "tester", "integration test"); err != nil || !ok {
		t.Fatalf("BanAccount = (%v,%v), want (true,nil)", ok, err)
	}
	if ai := banned(); !ai.Banned {
		t.Error("account should show banned after BanAccount")
	}
	if ok, err := st.UnbanAccount(ctx, name); err != nil || !ok {
		t.Fatalf("UnbanAccount = (%v,%v), want (true,nil)", ok, err)
	}
	if ai := banned(); ai.Banned {
		t.Error("account should show unbanned after UnbanAccount")
	}

	// Bot-prefixed accounts must be excluded from the listing.
	list, _ := st.ListAccounts(ctx, name, "", 500) // use the test name as the "bot" prefix
	for _, a := range list {
		if a.Username == name {
			t.Error("ListAccounts must exclude accounts matching the bot prefix")
		}
	}
}
