#!/usr/bin/env bash
set -euo pipefail

SERVER_BIN="./atm_server"
CLIENT_BIN="./atm_client"
DB_PATH="data/atm.db"
PORT=5555
PASS_COUNT=0
FAIL_COUNT=0

cleanup() {
    if [ -n "${SERVER_PID:-}" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

pass()  { PASS_COUNT=$((PASS_COUNT + 1)); echo "  PASS: $1"; }
fail()  { FAIL_COUNT=$((FAIL_COUNT + 1)); echo "  FAIL: $1"; }

run_client() {
    printf "$1" | "$CLIENT_BIN" 2>/dev/null
}

if [ ! -f "$SERVER_BIN" ] || [ ! -f "$CLIENT_BIN" ]; then
    echo "FAIL: Build binaries first with 'make'"
    exit 1
fi

rm -f "$DB_PATH"

export ATM_ADMIN_USERNAME="admin"
export ATM_ADMIN_PASSWORD="SecurePass123"

# Start server
"$SERVER_BIN" "$PORT" "$DB_PATH" &
SERVER_PID=$!
sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "FAIL: Server failed to start"
    exit 1
fi

echo ""
echo "=== Test Suite ==="
echo ""

# Test 1: Create Account
OUTPUT=$(run_client "1\n1001\nAlice\n1234\n4\n")
if echo "$OUTPUT" | grep -q "Account created successfully"; then
    pass "Create account"
else
    fail "Create account"
fi

# Test 2: Duplicate account number rejected
OUTPUT=$(run_client "1\n1001\nAlice2\n5678\n4\n")
if echo "$OUTPUT" | grep -q "already exists"; then
    pass "Reject duplicate account number"
else
    fail "Reject duplicate account number"
fi

# Test 3: Login
OUTPUT=$(run_client "2\n1001\n1234\n7\n4\n")
if echo "$OUTPUT" | grep -q "Welcome, Alice"; then
    pass "Customer login"
else
    fail "Customer login"
fi

# Test 4: Wrong PIN rejected
OUTPUT=$(run_client "2\n1001\n9999\n4\n")
if echo "$OUTPUT" | grep -q "Invalid"; then
    pass "Reject wrong PIN"
else
    fail "Reject wrong PIN"
fi

# Test 5: Deposit
OUTPUT=$(run_client "2\n1001\n1234\n2\n200.00\n7\n4\n")
if echo "$OUTPUT" | grep -q "200.00"; then
    pass "Deposit 200.00"
else
    fail "Deposit 200.00"
fi

# Test 6: Withdrawal
OUTPUT=$(run_client "2\n1001\n1234\n3\n50.00\n7\n4\n")
if echo "$OUTPUT" | grep -q "150.00"; then
    pass "Withdraw 50.00 (balance 150.00)"
else
    fail "Withdraw 50.00"
fi

# Test 7: Balance inquiry
OUTPUT=$(run_client "2\n1001\n1234\n1\n7\n4\n")
if echo "$OUTPUT" | grep -q "150.00"; then
    pass "Balance inquiry shows 150.00"
else
    fail "Balance inquiry"
fi

# Test 8: Create second account for transfer
OUTPUT=$(run_client "1\n2001\nBob\n5678\n4\n")
if echo "$OUTPUT" | grep -q "created"; then
    pass "Create second account"
else
    fail "Create second account"
fi

# Test 9: Transfer between accounts
OUTPUT=$(run_client "2\n1001\n1234\n4\n2001\n30.00\n7\n4\n")
if echo "$OUTPUT" | grep -q "120.00"; then
    pass "Transfer 30.00 to Bob (sender balance 120.00)"
else
    fail "Transfer"
fi

# Test 10: Mini statement
OUTPUT=$(run_client "2\n1001\n1234\n6\n7\n4\n")
if echo "$OUTPUT" | grep -q "MINI STATEMENT"; then
    pass "Mini statement generated"
else
    fail "Mini statement"
fi

# Test 11: Monthly summary
OUTPUT=$(run_client "2\n1001\n1234\n5\n\n7\n4\n")
if echo "$OUTPUT" | grep -q "MONTHLY SUMMARY"; then
    pass "Monthly summary generated"
else
    fail "Monthly summary"
fi

# Test 12: Admin login
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n14\n4\n")
if echo "$OUTPUT" | grep -q "Admin login successful"; then
    pass "Admin login"
else
    fail "Admin login"
fi

# Test 13: Admin dashboard
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n11\n14\n4\n")
if echo "$OUTPUT" | grep -q "ADMIN DASHBOARD"; then
    pass "Admin dashboard"
else
    fail "Admin dashboard"
fi

# Test 14: List all accounts
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n1\n14\n4\n")
if echo "$OUTPUT" | grep -q "Alice"; then
    pass "List all accounts"
else
    fail "List all accounts"
fi

# Test 15: View account details
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n2\n1001\n14\n4\n")
if echo "$OUTPUT" | grep -q "Alice"; then
    pass "View account details"
else
    fail "View account details"
fi

# Test 16: Lock and unlock account
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n5\n1001\n6\n1001\n14\n4\n")
if echo "$OUTPUT" | grep -q "Account unlocked"; then
    pass "Lock and unlock account"
else
    fail "Lock and unlock account"
fi

# Test 17: Recent transactions
OUTPUT=$(run_client "3\nadmin\nSecurePass123\n12\n10\n14\n4\n")
if echo "$OUTPUT" | grep -q "RECENT TRANSACTIONS"; then
    pass "Recent transactions"
else
    fail "Recent transactions"
fi

# Test 18: Invalid menu choice
OUTPUT=$(run_client "9\n4\n")
if echo "$OUTPUT" | grep -q "Invalid"; then
    pass "Invalid menu choice handled gracefully"
else
    fail "Invalid menu choice"
fi

echo ""
echo "========================================"
echo "Results: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "========================================"

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
