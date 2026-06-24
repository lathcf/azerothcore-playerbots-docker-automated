package main

import "testing"

func TestValidateUsername(t *testing.T) {
	good := []string{"abc", "Bob_99", "ABCDEFGHIJKLMNOP"} // 16 chars max
	for _, s := range good {
		if err := ValidateUsername(s); err != nil {
			t.Errorf("ValidateUsername(%q) = %v, want nil", s, err)
		}
	}
	bad := []string{"", "ab", "this_name_is_way_too_long", "has space", "bad!char", "drop;table"}
	for _, s := range bad {
		if err := ValidateUsername(s); err == nil {
			t.Errorf("ValidateUsername(%q) = nil, want error", s)
		}
	}
}

func TestValidatePassword(t *testing.T) {
	good := []string{"hunter2", "abcd", "ABCDEFGHIJKLMNOP"}
	for _, s := range good {
		if err := ValidatePassword(s); err != nil {
			t.Errorf("ValidatePassword(%q) = %v, want nil", s, err)
		}
	}
	bad := []string{"", "abc", "this_password_is_too_long", "has space"}
	for _, s := range bad {
		if err := ValidatePassword(s); err == nil {
			t.Errorf("ValidatePassword(%q) = nil, want error", s)
		}
	}
}
