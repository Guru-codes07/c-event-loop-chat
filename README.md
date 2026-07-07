# Event-Driven Multi-Client Chat Server (C)

A high-performance **multi-client chat server** written in **C** using **POSIX sockets**, an **event loop with `poll()`**, a **custom binary protocol**, and **SQLite** for persistent chat history.

This project was built to learn real-world systems programming concepts such as event-driven networking, binary protocols, socket programming, database integration, and scalable server architecture.

---

## Features

* Event-driven server using `poll()`
* Multi-client communication
* Custom binary protocol
* Username authentication
* Public chat
* Private messaging
* `/who` command to list connected users
* Chat history stored using SQLite
* CRC32 checksum validation
* Non-blocking message reception
* Modular project structure
* Makefile for easy compilation

---

## Technologies Used

* C (C11)
* POSIX Sockets
* poll()
* SQLite3
* GCC
* Linux

---

## Project Structure

```text
c-event-loop-chat/
│
├── client/
│   └── client.c
│
├── include/
│   ├── commands.h
│   ├── common.h
│   ├── connections.h
│   ├── database.h
│   ├── network.h
│   └── protocol.h
│
├── src/
│   ├── server.c
│   ├── protocol.c
│   ├── network.c
│   ├── database.c
│   ├── commands.c
│   └── connections.c
│
├── chat_history.db/
│
├── build/
│
├── Makefile
└── README.md
```

---

## Concepts Covered

### Networking

* TCP sockets
* Client-server architecture
* Socket APIs
* Connection management
* IPv4 networking

### Event-Driven Programming

* `poll()`
* Event loops
* Handling multiple clients without threads
* Non-blocking I/O

### Binary Protocol

Custom protocol containing:

* Version
* Message type
* Flags
* Payload length
* Message ID
* Timestamp
* CRC32 checksum
* Payload

---

## Supported Message Types

| Type                  | Description                  |
| --------------------- | ---------------------------- |
| `MSG_CONNECTION`      | User joins the server        |
| `MSG_CHAT`            | Public chat message          |
| `MSG_PRIVATE_MSG`     | Private message              |
| `MSG_WHO_COMMAND`     | List connected users         |
| `MSG_HISTORY_COMMAND` | Retrieve stored chat history |
| `MSG_SERVER`          | Server notifications         |
| `MSG_ERROR`           | Error message                |
| `MSG_DISCONNECT`      | Client disconnect            |

---

## Database

SQLite is used to persist chat messages.

Stored information includes:

* Username
* Message
* Timestamp

This allows chat history to remain available even after the server restarts.

---

## Building

```bash
make
```

---

## Running

Start the server:

```bash
./build/server
```

Start a client:

```bash
./build/client
```

Open multiple terminals to connect multiple clients.

---

## Learning Outcomes

This project helped me understand:

* Event-driven server architecture
* Network programming in C
* POSIX socket APIs
* Binary protocol design
* Serialization and deserialization
* CRC32 checksum verification
* SQLite database integration
* Modular software architecture
* Memory management in C
* Building scalable networking applications

---

## Future Improvements

* End-to-end encryption
* File transfer
* Group chat support
* User authentication
* Configuration file support
* Logging system
* Epoll backend for Linux
* IPv6 support
* Better error recovery
* Unit tests

---

## Requirements

* Linux
* GCC
* SQLite3 development library
* Make

---

## Compile Dependencies (Fedora)

```bash
sudo dnf install gcc make sqlite sqlite-devel
```

---

## Author

**Guru Prasad Mishra**

Computer Science Engineering Student

Interested in:

* Operating Systems
* Linux Kernel Development
* Systems Programming
* Networking
* Device Drivers
* Low-Level Software Development

---

## License

This project is open-source and intended for learning and educational purposes.
