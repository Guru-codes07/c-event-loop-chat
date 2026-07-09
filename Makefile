# Makefile for c-event-loop-chat
# A multi-client TCP chat server with binary protocol and SQLite persistence

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lsqlite3 -lssl -lcrypto

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SERVER_SRCS = $(SRC_DIR)/server.c \
              $(SRC_DIR)/protocol.c \
              $(SRC_DIR)/connections.c \
              $(SRC_DIR)/commands.c \
              $(SRC_DIR)/TLS.c \
              $(SRC_DIR)/network.c \
              $(SRC_DIR)/database.c

CLIENT_SRCS = $(SRC_DIR)/client.c \
              $(SRC_DIR)/protocol.c \
              $(SRC_DIR)/TLS.c \
              $(SRC_DIR)/network.c

# Object files (mirrors src/ under build/obj/)
SERVER_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(CLIENT_SRCS))

# Auto-generated header dependency files (one per .o)
DEPS = $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

# Output binaries
SERVER_BIN = $(BUILD_DIR)/server
CLIENT_BIN = $(BUILD_DIR)/client

.PHONY: all clean help build run-server run-client format server client info

all: $(SERVER_BIN) $(CLIENT_BIN)
	@echo ""
	@echo "Build successful!"
	@echo "  Server: $(SERVER_BIN)"
	@echo "  Client: $(CLIENT_BIN)"
	@echo ""
	@echo "Run with:"
	@echo "  make run-server    (Terminal 1)"
	@echo "  make run-client    (Terminal 2)"
	@echo ""

$(SERVER_BIN): $(SERVER_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "[LD] Linking server..."
	@$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LDFLAGS)
	@echo "[OK] Server built: $@"

$(CLIENT_BIN): $(CLIENT_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "[LD] Linking client..."
	@$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(LDFLAGS)
	@echo "[OK] Client built: $@"

# Compile any .c into build/obj/, generating a matching .d dependency file
# so editing a header only rebuilds the .o files that actually include it.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

-include $(DEPS)

server: $(SERVER_BIN)
client: $(CLIENT_BIN)

run-server: $(SERVER_BIN)
	@echo "Starting server on port 8080..."
	@echo "(Press Ctrl+C to stop)"
	@echo ""
	@$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	@echo "Connecting to server..."
	@echo ""
	@$(CLIENT_BIN)

clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR)
	@rm -f *.db *.db-shm *.db-wal
	@echo "Cleaned"

format:
	@echo "Formatting code..."
	@clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h
	@echo "Formatted"

help:
	@echo "c-event-loop-chat -- Makefile targets"
	@echo ""
	@echo "Usage:"
	@echo "  make              Build both server and client"
	@echo "  make server       Build server only"
	@echo "  make client       Build client only"
	@echo "  make run-server   Build and run server"
	@echo "  make run-client   Build and run client"
	@echo "  make clean        Remove build artifacts and database"
	@echo "  make format       Format code (requires clang-format)"
	@echo "  make help         Show this help message"
	@echo ""

info:
	@echo "Build Configuration:"
	@echo "  Compiler:      $(CC)"
	@echo "  Flags:         $(CFLAGS)"
	@echo "  Linker Flags:  $(LDFLAGS)"
	@echo "  Source Dir:    $(SRC_DIR)"
	@echo "  Include Dir:   $(INCLUDE_DIR)"
	@echo "  Build Dir:     $(BUILD_DIR)"
	@echo ""
	@echo "Source Files (Server):"
	@for f in $(SERVER_SRCS); do echo "  $$f"; done
	@echo ""
	@echo "Source Files (Client):"
	@for f in $(CLIENT_SRCS); do echo "  $$f"; done