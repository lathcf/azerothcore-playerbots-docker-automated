package main

import (
	"regexp"
)

// ValidationError is a user-safe error message (safe to show in the UI).
type ValidationError struct{ Msg string }

func (e ValidationError) Error() string { return e.Msg }

// WoW 3.3.5a clients cap username and password at 16 characters. Usernames are
// restricted to a conservative charset; passwords forbid spaces/control chars.
var usernameRe = regexp.MustCompile(`^[A-Za-z0-9_]{3,16}$`)
var passwordRe = regexp.MustCompile(`^[\x21-\x7e]{4,16}$`) // printable ASCII, no space

func ValidateUsername(s string) error {
	if !usernameRe.MatchString(s) {
		return ValidationError{"username must be 3-16 characters: letters, numbers, underscore"}
	}
	return nil
}

func ValidatePassword(s string) error {
	if !passwordRe.MatchString(s) {
		return ValidationError{"password must be 4-16 characters, no spaces"}
	}
	return nil
}
