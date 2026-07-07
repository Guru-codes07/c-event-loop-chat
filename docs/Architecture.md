# Architecture

## Overview

The Event-Driven Chat Server follows a modular architecture where each component has a single responsibility. The server uses an event loop based on `poll()` to handle multiple clients efficiently without creating a thread for every connection.

```
                   +------------------+
                   |     Clients      |
                   | (TCP Sockets)    |
                   +--------+---------+
                            |
                            |
                    TCP Connections
                            |
                            v
                +-----------------------+
                |      Event Loop       |
                |      poll()           |
                +-----------+-----------+
                            |
        +-------------------+-------------------+
        |                   |                   |
        v                   v                   v
+---------------+   +----------------+   +----------------+
|  Networking   |   |    Commands    |   |    Protocol    |
| Socket APIs   |   | Command Parser |   | Packet Parsing |
+-------+-------+   +--------+-------+   +--------+-------+
        |                    |                     |
        +--------------------+---------------------+
                             |
                             v
                  +-----------------------+
                  |      Database         |
                  |      SQLite3          |
                  +-----------------------+
```

---

# Directory Structure

```
client/
include/
src/
docs/
build/
Makefile
```

Each directory has a dedicated purpose.

---

# Module Responsibilities

## server.c

The server is the entry point of the application.

Responsibilities:

* Create listening socket
* Bind to a TCP port
* Listen for incoming connections
* Initialize the event loop
* Accept new clients
* Handle client disconnections
* Dispatch received packets

This file controls the entire server lifecycle.

---

## network.c

Responsible for low-level networking.

Responsibilities:

* Socket creation
* Sending packets
* Receiving packets
* Connection handling
* Client management
* Error handling

This module hides socket implementation details from the rest of the application.

---

## protocol.c

Responsible for the custom binary protocol.

Responsibilities:

* Packet serialization
* Packet deserialization
* Header validation
* CRC32 verification
* Length validation

Every packet entering or leaving the server passes through this module.

---

## commands.c

Responsible for processing client commands.

Examples:

```
/who
/msg
/history
/help
```

Responsibilities:

* Parse commands
* Execute server-side logic
* Generate responses

---

## database.c

Responsible for persistent storage.

Responsibilities:

* Initialize SQLite
* Store chat history
* Retrieve chat history
* Execute SQL queries
* Handle database errors

No other module directly communicates with SQLite.

---

## connections.c

Responsible for connected clients.

Responsibilities:

* Register new clients
* Remove disconnected clients
* Search clients
* Broadcast messages
* Private messaging

This module maintains the active client list.

---

# Data Flow

A typical chat message follows this path.

```
Client
   │
   ▼
Socket
   │
   ▼
poll()
   │
   ▼
network.c
   │
   ▼
protocol.c
   │
   ▼
commands.c
   │
   ▼
database.c (optional)
   │
   ▼
connections.c
   │
   ▼
Other Clients
```

---

# Event-Driven Design

Instead of creating one thread per client, the server uses a single event loop.

Advantages:

* Lower memory usage
* Fewer context switches
* Better scalability
* Simpler synchronization
* Predictable performance

---

# Layered Architecture

```
Application Layer
│
├── Commands
├── Chat Logic
└── User Management

Protocol Layer
│
├── Serialization
├── CRC32
└── Packet Validation

Networking Layer
│
├── poll()
├── TCP Sockets
└── Connection Management

Storage Layer
│
└── SQLite Database
```

Each layer communicates only with the layer directly above or below it.

---

# Design Principles

The project follows several software engineering principles.

## Separation of Concerns

Each module has one clearly defined responsibility.

---

## Modularity

Networking, protocol handling, command processing, and database logic are separated into independent modules.

---

## Maintainability

Adding a new command or packet type requires changes to only a small portion of the codebase.

---

## Scalability

Using an event loop allows the server to support many simultaneous clients without creating large numbers of threads.

---

# Summary

The architecture separates networking, protocol handling, command processing, client management, and database operations into dedicated modules. This modular design improves readability, maintainability, and scalability while serving as a practical example of an event-driven systems programming project written in C.
