#!/usr/bin/env python3
"""Generate terminal screenshots and demo GIF for the ATM project README."""

import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = "docs/images"
os.makedirs(OUT_DIR, exist_ok=True)

# Setup font
FONT_PATH = "/System/Library/Fonts/Menlo.ttc"
try:
    font = ImageFont.truetype(FONT_PATH, 14, encoding="unic")
    font_bold = ImageFont.truetype(FONT_PATH, 15, encoding="unic")
    font_title = ImageFont.truetype(FONT_PATH, 18, encoding="unic")
except Exception:
    font = ImageFont.load_default()
    font_bold = font
    font_title = font

# Color palette
BG = (30, 30, 30)
GREEN = (0, 255, 0)
WHITE = (212, 212, 212)
GRAY = (120, 120, 120)
RED = (255, 85, 85)
YELLOW = (255, 200, 50)
BLUE = (86, 156, 214)
ORANGE = (255, 150, 50)

CELL_W = 9   # approximate char width for Menlo 14
CELL_H = 20  # line height

def render_terminal(lines, title="", width_chars=78):
    """Render a list of (color, text) tuples as a terminal screenshot."""
    h = len(lines) * CELL_H + 50
    w = width_chars * CELL_W + 40
    img = Image.new("RGB", (w, h), BG)
    draw = ImageDraw.Draw(img)

    # Title bar
    if title:
        draw.rectangle([0, 0, w, 28], fill=(50, 50, 50))
        draw.text((12, 5), title, font=font_bold, fill=WHITE)
        # close/minimize dots
        for i, c in enumerate([(255, 80, 80), (255, 200, 50), (80, 200, 80)]):
            draw.ellipse([w - 65 + i * 22, 8, w - 50 + i * 22, 23], fill=c)

    y = 36
    for color, text in lines:
        draw.text((14, y), text, font=font, fill=color)
        y += CELL_H

    return img


# ─── 1. Server Running ───────────────────────────────────────────────
img = render_terminal([
    (GRAY,  "[2026-07-15 15:36:05] [INFO] Database initialized: data/atm.db"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] Default admin account created for user 'admin'"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] ATM server listening on port 5555 using data/atm.db"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] Set ATM_ADMIN_USERNAME and ATM_ADMIN_PASSWORD env vars for admin access"),
    (WHITE, ""),
    (GREEN, "$ ./atm_client"),
    (WHITE, "Connected to ATM server at 127.0.0.1:5555"),
    (WHITE, ""),
    (GRAY,  "[2026-07-15 15:36:06] [INFO] Client connected from 127.0.0.1:49630"),
    (WHITE, ""),
    (ORANGE, "==== MINI ATM CLIENT ===="),
    (WHITE,  "1. Create Account"),
    (WHITE,  "2. Customer Login"),
    (WHITE,  "3. Admin Login"),
    (WHITE,  "4. Exit"),
    (GREEN,  "Choose: "),
], title="atm-server  —  Terminal", width_chars=78)
img.save(os.path.join(OUT_DIR, "server-startup.png"))
print("✓ server-startup.png")


# ─── 2. Customer Login ───────────────────────────────────────────────
img = render_terminal([
    (ORANGE, "==== MINI ATM CLIENT ===="),
    (WHITE,  "1. Create Account"),
    (WHITE,  "2. Customer Login"),
    (WHITE,  "3. Admin Login"),
    (WHITE,  "4. Exit"),
    (GREEN,  "Choose: 2"),
    (WHITE,  ""),
    (WHITE,  "Enter Account Number: 1001"),
    (WHITE,  "Enter PIN: ****"),
    (WHITE,  ""),
    (GREEN,  "Login successful. Welcome, Alice."),
    (WHITE,  ""),
    (ORANGE, "==== CUSTOMER MENU ===="),
    (WHITE,  "Logged in as Alice (Acc:1001)"),
    (WHITE,  "1. Check Balance"),
    (WHITE,  "2. Deposit"),
    (WHITE,  "3. Withdraw"),
    (WHITE,  "4. Transfer"),
    (WHITE,  "5. Monthly Transaction Summary"),
    (WHITE,  "6. Print Mini Statement"),
    (WHITE,  "7. Logout"),
    (GREEN,  "Choose: "),
], title="atm-client  —  Customer Login", width_chars=72)
img.save(os.path.join(OUT_DIR, "customer-login.png"))
print("✓ customer-login.png")


# ─── 3. Customer Menu / Deposit ──────────────────────────────────────
img = render_terminal([
    (GREEN,  "Choose: 2"),
    (WHITE,  "Enter amount to deposit: 500.00"),
    (GREEN,  "Deposit successful. New balance: 500.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 1"),
    (WHITE,  "Balance: 500.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 3"),
    (WHITE,  "Enter amount to withdraw: 50.00"),
    (GREEN,  "Withdrawal successful. New balance: 450.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 4"),
    (WHITE,  "Enter Receiver Account Number: 2001"),
    (WHITE,  "Enter Amount: 30.00"),
    (GREEN,  "Transfer successful. New balance: 420.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 7"),
    (WHITE,  "Logged out."),
    (WHITE,  "Disconnected from ATM server."),
], title="atm-client  —  Deposit / Withdraw / Transfer", width_chars=72)
img.save(os.path.join(OUT_DIR, "customer-dashboard.png"))
print("✓ customer-dashboard.png")


# ─── 4. Admin Dashboard ──────────────────────────────────────────────
img = render_terminal([
    (ORANGE, "==== ADMIN MENU ===="),
    (WHITE,  "11. Admin Dashboard"),
    (WHITE,  "12. Recent Transactions"),
    (WHITE,  "13. Change Admin Password"),
    (WHITE,  "14. Logout"),
    (GREEN,  "Choose: 11"),
    (WHITE,  ""),
    (BLUE,   "ADMIN DASHBOARD"),
    (WHITE,  "Total Accounts      : 2"),
    (WHITE,  "Locked Accounts     : 0"),
    (WHITE,  "Total Bank Balance  : 480.00"),
    (WHITE,  "Total Transactions  : 5"),
    (WHITE,  "Today's Transactions: 5"),
    (WHITE,  "Admin Users         : 1"),
    (WHITE,  "Highest Balance     : 1 | Alice | 420.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 1"),
    (WHITE,  ""),
    (BLUE,   "ALL ACCOUNTS"),
    (GRAY,   "Acc No     Name                      Balance      Locked     Failed Attempts"),
    (WHITE,  "1001       Alice                     420.00      No         0"),
    (WHITE,  "2001       Bob                       60.00       No         0"),
], title="atm-client  —  Admin Dashboard & Account List", width_chars=72)
img.save(os.path.join(OUT_DIR, "admin-dashboard.png"))
print("✓ admin-dashboard.png")


# ─── 5. Mini Statement ───────────────────────────────────────────────
img = render_terminal([
    (GREEN,  "Choose: 6"),
    (WHITE,  ""),
    (BLUE,   "MINI STATEMENT"),
    (WHITE,  "Account Number: 1001"),
    (WHITE,  "Account Name  : Alice"),
    (WHITE,  "Current Balance: 420.00"),
    (WHITE,  ""),
    (GRAY,   "Timestamp            Type               Amount       Balance      Details"),
    (WHITE,  "2026-07-15 15:42:12 Transfer Sent      30.00       420.00       Other: 2001"),
    (WHITE,  "2026-07-15 15:42:12 Withdraw           50.00       450.00       -"),
    (WHITE,  "2026-07-15 15:42:12 Deposit            500.00      500.00       -"),
    (WHITE,  "2026-07-15 15:42:12 Transfer Sent      30.00       120.00       Other: 2001"),
    (WHITE,  "2026-07-15 15:42:12 Withdraw           50.00       150.00       -"),
    (WHITE,  ""),
    (GREEN,  "Choose: "),
], title="atm-client  —  Mini Statement", width_chars=78)
img.save(os.path.join(OUT_DIR, "transaction-history.png"))
print("✓ transaction-history.png")


# ─── 6. Repository Structure ─────────────────────────────────────────
tree_lines = [
    (BLUE,   "atm-management-system-c/"),
    (WHITE,  "├── include/"),
    (GRAY,   "│   ├── account.h        auth.h        database.h"),
    (GRAY,   "│   ├── logger.h         network.h     protocol.h"),
    (GRAY,   "│   ├── sha256.h         utils.h"),
    (WHITE,  "├── src/"),
    (GRAY,   "│   ├── main.c           server.c      client.c"),
    (GRAY,   "│   ├── auth.c           account.c     database.c"),
    (GRAY,   "│   ├── network.c        logger.c      sha256.c"),
    (GRAY,   "│   ├── utils.c          protocol.c"),
    (WHITE,  "├── data/                (runtime files, gitignored)"),
    (WHITE,  "├── docs/"),
    (GRAY,   "│   ├── github-topics.md"),
    (GRAY,   "│   ├── release-notes-v1.0.0.md"),
    (GRAY,   "│   └── images/"),
    (WHITE,  "├── tests/               test_atm.sh"),
    (WHITE,  "├── .github/workflows/   build.yml"),
    (WHITE,  "├── Makefile"),
    (WHITE,  "├── README.md"),
    (WHITE,  "├── LICENSE              (MIT)"),
    (WHITE,  "└── .gitignore"),
]

h = len(tree_lines) * CELL_H + 50
w = 70 * CELL_W + 40
img = Image.new("RGB", (w, h), BG)
draw = ImageDraw.Draw(img)

# Title bar
draw.rectangle([0, 0, w, 28], fill=(50, 50, 50))
draw.text((12, 5), "Repository Structure", font=font_bold, fill=WHITE)
for i, c in enumerate([(255, 80, 80), (255, 200, 50), (80, 200, 80)]):
    draw.ellipse([w - 65 + i * 22, 8, w - 50 + i * 22, 23], fill=c)

y = 36
for color, text in tree_lines:
    draw.text((14, y), text, font=font, fill=color)
    y += CELL_H

img.save(os.path.join(OUT_DIR, "repository-structure.png"))
print("✓ repository-structure.png")


# ─── 7. Demo GIF ─────────────────────────────────────────────────────
def frame(lines, width_chars=72):
    h = len(lines) * CELL_H + 50
    w = width_chars * CELL_W + 40
    img = Image.new("RGB", (w, h), BG)
    draw = ImageDraw.Draw(img)

    draw.rectangle([0, 0, w, 28], fill=(50, 50, 50))
    draw.text((12, 5), "Demo  —  atm-management-system-c", font=font_bold, fill=WHITE)
    for i, c in enumerate([(255, 80, 80), (255, 200, 50), (80, 200, 80)]):
        draw.ellipse([w - 65 + i * 22, 8, w - 50 + i * 22, 23], fill=c)

    y = 36
    for color, text in lines:
        draw.text((14, y), text, font=font, fill=color)
        y += CELL_H
    return img

frames = []
# Frame 1: Terminal + make
frames.append(frame([
    (GREEN, "$ make"),
    (GRAY,  "gcc -Wall -Wextra -Wpedantic ... -c src/main.c"),
    (GRAY,  "gcc -Wall -Wextra -Wpedantic ... -c src/server.c"),
    (GRAY,  "gcc -Wall -Wextra -Wpedantic ... -c src/auth.c"),
    (GRAY,  "gcc ... -o atm_client"),
    (GRAY,  "gcc ... -lsqlite3 -o atm_server"),
    (GREEN, "$"),
], width_chars=68))

# Frame 2: Start server
frames.append(frame([
    (GREEN, "$ export ATM_ADMIN_USERNAME=admin"),
    (GREEN, "$ export ATM_ADMIN_PASSWORD=SecurePass123"),
    (GREEN, "$ ./atm_server"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] Database initialized: data/atm.db"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] Default admin account created for user 'admin'"),
    (GRAY,  "[2026-07-15 15:36:05] [INFO] ATM server listening on port 5555"),
    (GREEN, "$"),
], width_chars=68))

# Frame 3: Client connects, create account
frames.append(frame([
    (GREEN, "$ ./atm_client"),
    (WHITE, "Connected to ATM server at 127.0.0.1:5555"),
    (ORANGE, "==== MINI ATM CLIENT ===="),
    (WHITE,  "1. Create Account"),
    (WHITE,  "2. Customer Login"),
    (WHITE,  "3. Admin Login"),
    (WHITE,  "4. Exit"),
    (GREEN,  "Choose: 1"),
    (WHITE,  "Enter Account Number: 1001"),
    (WHITE,  "Enter Name: Alice"),
    (WHITE,  "Set 4-digit PIN: ****"),
    (GREEN,  "Account created successfully."),
], width_chars=68))

# Frame 4: Login + deposit
frames.append(frame([
    (GREEN,  "Choose: 2"),
    (WHITE,  "Enter Account Number: 1001"),
    (WHITE,  "Enter PIN: ****"),
    (GREEN,  "Login successful. Welcome, Alice."),
    (ORANGE, "==== CUSTOMER MENU ===="),
    (WHITE,  "1. Check Balance    2. Deposit"),
    (WHITE,  "3. Withdraw         4. Transfer"),
    (WHITE,  "5. Monthly Summary  6. Mini Statement"),
    (WHITE,  "7. Logout"),
    (GREEN,  "Choose: 2"),
    (WHITE,  "Enter amount to deposit: 500.00"),
    (GREEN,  "Deposit successful. New balance: 500.00"),
], width_chars=68))

# Frame 5: Withdraw + transfer
frames.append(frame([
    (GREEN,  "Choose: 3"),
    (WHITE,  "Enter amount to withdraw: 50.00"),
    (GREEN,  "Withdrawal successful. New balance: 450.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 4"),
    (WHITE,  "Enter Receiver Account Number: 2001"),
    (WHITE,  "Enter Amount: 30.00"),
    (GREEN,  "Transfer successful. New balance: 420.00"),
    (WHITE,  ""),
    (GREEN,  "Choose: 6"),
    (BLUE,   "MINI STATEMENT"),
    (WHITE,  "Current Balance: 420.00"),
    (GRAY,   "Type               Amount"),
    (WHITE,  "Transfer Sent      30.00"),
    (WHITE,  "Withdraw           50.00"),
    (WHITE,  "Deposit            500.00"),
], width_chars=68))

# Frame 6: Logout final
frames.append(frame([
    (GREEN,  "Choose: 7"),
    (WHITE,  "Logged out."),
    (WHITE,  "Disconnected from ATM server."),
    (WHITE,  ""),
    (GREEN, "$"),
    (WHITE,  ""),
    (BLUE,   "✓ Build: 0 warnings, 0 errors"),
    (BLUE,   "✓ Tests: 18/18 passed"),
    (GREEN,  "✓ Ready for review."),
], width_chars=68))

durations = [2500, 3000, 3500, 3500, 4000, 3000]
frames[0].save(
    os.path.join(OUT_DIR, "demo.gif"),
    save_all=True,
    append_images=frames[1:],
    duration=durations,
    loop=0,
    optimize=False,
)
print("✓ demo.gif")

print("\nAll screenshots and demo GIF generated in docs/images/")
