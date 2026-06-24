package main

import (
	cryptorand "crypto/rand"
	"crypto/sha1"
	"crypto/subtle"
	"math/big"
	"strings"
)

// AzerothCore SRP6 v1 parameters. Reference:
// src/common/Cryptography/Authentication/SRP6.cpp
//   v = g ^ H(salt || H(UPPER(user) || ":" || UPPER(pass))) mod N
// SHA1 exponent and verifier are little-endian; salt and verifier are 32 bytes.
var (
	srpN, _ = new(big.Int).SetString(
		"894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)
	srpG = big.NewInt(7)
)

// MakeSalt returns 32 cryptographically-random bytes.
func MakeSalt() ([]byte, error) {
	s := make([]byte, 32)
	if _, err := cryptorand.Read(s); err != nil {
		return nil, err
	}
	return s, nil
}

// reverseBytes returns a reversed copy (big-endian <-> little-endian).
func reverseBytes(b []byte) []byte {
	r := make([]byte, len(b))
	for i := range b {
		r[len(b)-1-i] = b[i]
	}
	return r
}

// CalculateVerifier returns the 32-byte little-endian SRP6 verifier.
func CalculateVerifier(username, password string, salt []byte) []byte {
	u := strings.ToUpper(username)
	p := strings.ToUpper(password)

	inner := sha1.Sum([]byte(u + ":" + p)) // H(UPPER(user):UPPER(pass)), 20 bytes

	h := sha1.New()
	h.Write(salt)
	h.Write(inner[:])
	outer := h.Sum(nil) // H(salt || inner), 20 bytes

	// outer is interpreted as a little-endian integer -> big.Int wants big-endian.
	x := new(big.Int).SetBytes(reverseBytes(outer))
	v := new(big.Int).Exp(srpG, x, srpN)

	// store little-endian, zero-padded to 32 bytes (low byte first).
	le := reverseBytes(v.Bytes())
	out := make([]byte, 32)
	copy(out, le)
	return out
}

// VerifyPassword recomputes the verifier from the stored salt and compares
// it in constant time to the stored verifier.
func VerifyPassword(username, password string, salt, verifier []byte) bool {
	computed := CalculateVerifier(username, password, salt)
	return subtle.ConstantTimeCompare(computed, verifier) == 1
}
