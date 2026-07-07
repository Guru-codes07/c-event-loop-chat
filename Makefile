# Makefile for c-event-loop-chat
# A multi-client TCP chat server with binary protocol and SQLite persistence

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lsqlite3

# Directories
SRC_DIR = src
CLIENT_DIR = client
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
SERVER_SRCS = $(SRC_DIR)/server.c \
              $(SRC_DIR)/protocol.c \
              $(SRC_DIR)/connections.c \
              $(SRC_DIR)/commands.c \
              $(SRC_DIR)/network.c \
              $(SRC_DIR)/database.c

CLIENT_SRCS = $(CLIENT_DIR)/client.c

# Output binaries
SERVER_BIN = $(BUILD_DIR)/server
CLIENT_BIN = $(BUILD_DIR)/client

# Phony targets (not actual files)
.PHONY: all clean help build run-server run-client format

# Default target
all: $(SERVER_BIN) $(CLIENT_BIN)
	@echo ""
	@echo "✓ Build successful!"
	@echo "  Server: $(SERVER_BIN)"
	@echo "  Client: $(CLIENT_BIN)"
	@echo ""
	@echo "Run with:"
	@echo "  make run-server    (Terminal 1)"
	@echo "  make run-client    (Terminal 2)"
	@echo ""

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@echo "[mkdir] $(BUILD_DIR)"

# Build server binary
$(SERVER_BIN): $(SERVER_SRCS) | $(BUILD_DIR)
	@echo "[CC] Compiling server..."
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -o $@ $(SERVER_SRCS) $(LDFLAGS)
	@echo "[OK] Server built: $@"

# Build client binary
$(CLIENT_BIN): $(CLIENT_SRCS) | $(BUILD_DIR)
	@echo "[CC] Compiling client..."
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -o $@ $(CLIENT_SRCS)
	@echo "[OK] Client built: $@"

# Build only server
server: $(SERVER_BIN)

# Build only client
client: $(CLIENT_BIN)

# Run server
run-server: $(SERVER_BIN)
	@echo "Starting server on port 8080..."
	@echo "(Press Ctrl+C to stop)"
	@echo ""
	@$(SERVER_BIN)

# Run client
run-client: $(CLIENT_BIN)
	@echo "Connecting to server..."
	@echo ""
	@$(CLIENT_BIN)

# Clean build artifacts
clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR)
	@rm -f *.db *.db-shm *.db-wal
	@echo "✓ Cleaned"

# Format code with clang-format (optional, requires clang-format)
format:
	@echo "Formatting code..."
	@clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(CLIENT_DIR)/*.c
	@echo "✓ Formatted"

# Show help
help:
	@echo "c-event-loop-chat — Makefile targets"
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
	@echo "Examples:"
	@echo "  make                # Compile both"
	@echo "  make run-server     # Build and start server"
	@echo "  make run-client     # Build and start client"
	@echo "  make clean          # Clean everything"
	@echo ""

# Info target (show build configuration)
info:
	@echo "Build Configuration:"
	@echo "  Compiler:      $(CC)"
	@echo "  Flags:         $(CFLAGS)"
	@echo "  Linker Flags:  $(LDFLAGS)"
	@echo "  Source Dir:    $(SRC_DIR)"
	@echo "  Client Dir:    $(CLIENT_DIR)"
	@echo "  Include Dir:   $(INCLUDE_DIR)"
	@echo "  Build Dir:     $(BUILD_DIR)"
	@echo ""
	@echo "Source Files (Server):"
	@for f in $(SERVER_SRCS); do echo "  $$f"; done
	@echo ""
	@echo "Source Files (Client):"
	@for f in $(CLIENT_SRCS); do echo "  $$f"; done