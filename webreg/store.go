package main

import (
	"context"
	"database/sql"
	"errors"
	"time"

	"github.com/go-sql-driver/mysql"
)

var (
	ErrUserExists = errors.New("username already taken")
)

// AccountInfo is a row in the admin account list.
type AccountInfo struct {
	ID        uint32
	Username  string
	Created   time.Time
	LastLogin *time.Time // nil = never logged in
	Banned    bool       // an active ban exists
}

// Store is the account persistence boundary (real impl: MySQLStore).
type Store interface {
	CreateAccount(ctx context.Context, username string, salt, verifier []byte) error
	GetAccount(ctx context.Context, username string) (salt, verifier []byte, found bool, err error)
	UpdateVerifier(ctx context.Context, username string, salt, verifier []byte) (found bool, err error)
	ListAccounts(ctx context.Context, botPrefix, search string, limit int) ([]AccountInfo, error)
	BanAccount(ctx context.Context, username, bannedBy, reason string) (found bool, err error)
	UnbanAccount(ctx context.Context, username string) (found bool, err error)
}

type MySQLStore struct {
	db *sql.DB
}

func NewMySQLStore(dsn string) (*MySQLStore, error) {
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		return nil, err
	}
	db.SetConnMaxLifetime(3 * time.Minute)
	db.SetMaxOpenConns(5)
	if err := db.Ping(); err != nil {
		return nil, err
	}
	return &MySQLStore{db: db}, nil
}

func (s *MySQLStore) CreateAccount(ctx context.Context, username string, salt, verifier []byte) error {
	_, err := s.db.ExecContext(ctx,
		"INSERT INTO account (username, salt, verifier) VALUES (?, ?, ?)",
		username, salt, verifier)
	var myErr *mysql.MySQLError
	if errors.As(err, &myErr) && myErr.Number == 1062 { // duplicate key
		return ErrUserExists
	}
	return err
}

func (s *MySQLStore) GetAccount(ctx context.Context, username string) ([]byte, []byte, bool, error) {
	var salt, verifier []byte
	err := s.db.QueryRowContext(ctx,
		"SELECT salt, verifier FROM account WHERE username = ?", username).
		Scan(&salt, &verifier)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, nil, false, nil
	}
	if err != nil {
		return nil, nil, false, err
	}
	return salt, verifier, true, nil
}

func (s *MySQLStore) UpdateVerifier(ctx context.Context, username string, salt, verifier []byte) (bool, error) {
	res, err := s.db.ExecContext(ctx,
		"UPDATE account SET salt = ?, verifier = ?, session_key = NULL WHERE username = ?",
		salt, verifier, username)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	return n > 0, err
}

// ListAccounts returns real (non-bot) accounts, optionally name-filtered, with an
// active-ban flag. Usernames matching botPrefix% are excluded (LIKE is case-insensitive
// under the table's ci collation). Capped at limit rows.
func (s *MySQLStore) ListAccounts(ctx context.Context, botPrefix, search string, limit int) ([]AccountInfo, error) {
	rows, err := s.db.QueryContext(ctx, `
SELECT a.id, a.username, a.joindate, a.last_login,
       EXISTS(SELECT 1 FROM account_banned b
              WHERE b.id = a.id AND b.active = 1
                AND (b.unbandate = b.bandate OR b.unbandate > UNIX_TIMESTAMP())) AS banned
FROM account a
WHERE a.username NOT LIKE CONCAT(?, '%')
  AND (? = '' OR a.username LIKE CONCAT('%', ?, '%'))
ORDER BY a.username
LIMIT ?`, botPrefix, search, search, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var out []AccountInfo
	for rows.Next() {
		var ai AccountInfo
		var created, last sql.NullTime
		if err := rows.Scan(&ai.ID, &ai.Username, &created, &last, &ai.Banned); err != nil {
			return nil, err
		}
		if created.Valid {
			ai.Created = created.Time
		}
		if last.Valid {
			t := last.Time
			ai.LastLogin = &t
		}
		out = append(out, ai)
	}
	return out, rows.Err()
}

// BanAccount writes a permanent active ban for the account (reversible via UnbanAccount).
// found is false when no such account exists. Re-banning reactivates the existing row.
func (s *MySQLStore) BanAccount(ctx context.Context, username, bannedBy, reason string) (bool, error) {
	res, err := s.db.ExecContext(ctx, `
INSERT INTO account_banned (id, bandate, unbandate, bannedby, banreason, active)
SELECT id, UNIX_TIMESTAMP(), UNIX_TIMESTAMP(), ?, ?, 1 FROM account WHERE username = ?
ON DUPLICATE KEY UPDATE active = 1, unbandate = VALUES(unbandate),
                        bannedby = VALUES(bannedby), banreason = VALUES(banreason)`,
		bannedBy, reason, username)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	return n > 0, err
}

// UnbanAccount clears active bans for the account. found is false when there was no
// active ban (or no such account) — the caller treats that as a benign no-op.
func (s *MySQLStore) UnbanAccount(ctx context.Context, username string) (bool, error) {
	res, err := s.db.ExecContext(ctx, `
UPDATE account_banned b JOIN account a ON a.id = b.id
SET b.active = 0 WHERE a.username = ? AND b.active = 1`, username)
	if err != nil {
		return false, err
	}
	n, err := res.RowsAffected()
	return n > 0, err
}
