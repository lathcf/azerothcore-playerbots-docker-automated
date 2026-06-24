package main

import (
	"bytes"
	"encoding/hex"
	"testing"
)

// Golden vectors computed independently (Python, documented SRP6 v1 algorithm).
// All values are little-endian, exactly as stored in acore_auth.account.
func TestCalculateVerifierGolden(t *testing.T) {
	cases := []struct {
		user, pass   string
		saltHex      string
		verifierHex  string
	}{
		{
			user: "TEST", pass: "TEST",
			saltHex:     "0000000000000000000000000000000000000000000000000000000000000000",
			verifierHex: "3e65457b00c732e898df20fd1d9de1968aa62dd78daa4c9df26938cdce18f47e",
		},
		{
			user: "ALICE", pass: "S3CRET",
			saltHex:     "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
			verifierHex: "83d7249948e027f51f6991e56de89fcb54a34f63f4d26d1eb062da1d5798b76b",
		},
	}
	for _, c := range cases {
		salt, _ := hex.DecodeString(c.saltHex)
		want, _ := hex.DecodeString(c.verifierHex)
		got := CalculateVerifier(c.user, c.pass, salt)
		if len(got) != 32 {
			t.Fatalf("%s: verifier length = %d, want 32", c.user, len(got))
		}
		if !bytes.Equal(got, want) {
			t.Errorf("%s: verifier = %s, want %s", c.user, hex.EncodeToString(got), c.verifierHex)
		}
	}
}

// Verifier must be case-insensitive on username and password (server uppercases both).
func TestCalculateVerifierUppercases(t *testing.T) {
	salt, _ := hex.DecodeString("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")
	upper := CalculateVerifier("ALICE", "S3CRET", salt)
	lower := CalculateVerifier("alice", "s3cret", salt)
	if !bytes.Equal(upper, lower) {
		t.Error("verifier should be identical regardless of input case")
	}
}

func TestVerifyPassword(t *testing.T) {
	salt, err := MakeSalt()
	if err != nil {
		t.Fatal(err)
	}
	v := CalculateVerifier("bob", "hunter2", salt)
	if !VerifyPassword("bob", "hunter2", salt, v) {
		t.Error("correct password should verify")
	}
	if VerifyPassword("BOB", "hunter2", salt, v) {
		// same: uppercased internally, so this SHOULD still verify
	}
	if !VerifyPassword("BOB", "HUNTER2", salt, v) {
		t.Error("case-insensitive correct password should verify")
	}
	if VerifyPassword("bob", "wrong", salt, v) {
		t.Error("wrong password must not verify")
	}
}

func TestMakeSaltLength(t *testing.T) {
	s, err := MakeSalt()
	if err != nil {
		t.Fatal(err)
	}
	if len(s) != 32 {
		t.Fatalf("salt length = %d, want 32", len(s))
	}
}
