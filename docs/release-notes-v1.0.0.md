# Release Notes — Version 1.0.0

## Highlights

Initial public release of the ATM Management System, a client-server
application written in C demonstrating socket programming, SQLite-backed
persistence, SHA-256 salted hashing, and modular C architecture.

## Features

**Customer Operations**
- Account creation with 4-digit PIN validation
- Secure login with account lockout after 3 failed attempts
- Real-time balance inquiry
- Cash deposits and withdrawals
- Fund transfers between accounts
- Mini statement showing the last 5 transactions
- Monthly transaction summary with deposits, withdrawals, and transfers

**Admin Operations**
- System-wide admin dashboard with statistics
- View all registered accounts with lock status
- Detailed account information including latest activity
- Lock and unlock customer accounts
- Credit and debit customer accounts
- Edit account numbers, names, and PINs
- Delete accounts with zero balance
- View any customer's mini statement or monthly summary
- View recent transactions across the entire system
- Administrator password change

**Security**
- PINs and passwords hashed with SHA-256 and a unique per-account 64-bit
  random salt
- Account lockout after 3 consecutive failed login attempts
- No hardcoded credentials — admin credentials are read from environment
  variables on first run
- No credential logging anywhere in the codebase
- Input validation on both client and server for all fields
- Bounds-checked string operations using snprintf throughout
- All SQLite statements properly finalized to prevent resource leaks

## Architecture

The project is organized into a clean modular structure:

```
include/     — Public headers
src/         — Implementation files (11 modules)
data/        — Runtime SQLite database (gitignored)
tests/       — Automated test script
```

Key modules: `auth.c`, `account.c`, `database.c`, `network.c`, `protocol.c`,
`sha256.c`, `logger.c`, `utils.c`, `server.c`, `client.c`, `main.c`

The server uses a length-prefixed TCP protocol for client communication and
SQLite for persistent storage.

## Testing

18 automated tests covering:
- Account creation and duplicate rejection
- Customer login and wrong PIN rejection
- Deposit, withdrawal, and balance inquiry
- Fund transfer between accounts
- Mini statement and monthly summary generation
- Admin login, dashboard, and account listing
- Account details view
- Account lock and unlock
- Recent transactions
- Invalid menu choice handling

Run with:
```bash
make test
```

## Platform Support

- Linux (Debian/Ubuntu, Arch, Fedora)
- macOS (via Homebrew for SQLite3)

## Build Requirements

- GCC or Clang with C11 support
- SQLite3 development libraries
- GNU Make

## Known Limitations

- Network communication is not encrypted (TLS is outside educational scope)
- SHA-256 with salt is used for hashing; production systems should use
  argon2, bcrypt, or scrypt
- Server handles one client at a time (sequential accept loop)
- Single admin account supported
- PINs and passwords are transmitted in plaintext over TCP
