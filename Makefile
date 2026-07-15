CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -O2 -Iinclude
LDFLAGS_SERVER = -lsqlite3

SRC_DIR = src
INC_DIR = include

CLIENT_SRCS = $(SRC_DIR)/client.c $(SRC_DIR)/utils.c $(SRC_DIR)/protocol.c \
              $(SRC_DIR)/network.c $(SRC_DIR)/logger.c
SERVER_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/server.c $(SRC_DIR)/auth.c \
              $(SRC_DIR)/account.c $(SRC_DIR)/database.c $(SRC_DIR)/network.c \
              $(SRC_DIR)/logger.c $(SRC_DIR)/sha256.c $(SRC_DIR)/utils.c \
              $(SRC_DIR)/protocol.c

CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

.PHONY: all clean distclean debug release test

all: atm_client atm_server

atm_client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

atm_server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS_SERVER) -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/*.h
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS += -g -DDEBUG -O0
debug: all

release: CFLAGS += -O3 -DNDEBUG
release: all

clean:
	rm -f $(SRC_DIR)/*.o atm_client atm_server

distclean: clean
	rm -f data/atm.db data/*.txt

test: atm_server atm_client
	@echo "Running tests..."
	@ATM_ADMIN_USERNAME=admin ATM_ADMIN_PASSWORD=SecurePass123 ./tests/test_atm.sh
	@echo "Tests complete."
