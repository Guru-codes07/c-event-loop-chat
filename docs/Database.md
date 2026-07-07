# Database

## Overview

The Event-Driven Chat Server uses **SQLite3** as its embedded database to provide persistent storage for chat messages. Unlike an in-memory solution, SQLite stores data on disk, allowing chat history to remain available even after the server is restarted.

SQLite was chosen because it is lightweight, serverless, and easy to integrate into C applications.

---

# Why SQLite?

SQLite offers several advantages for this project:

* Lightweight and fast
* No separate database server required
* Simple integration with C
* ACID-compliant transactions
* Stores data in a single file
* Cross-platform support
* Ideal for small to medium-sized applications

---

# Database Architecture

```text
                +----------------+
                |   server.c     |
                +-------+--------+
                        |
                        |
                Stores/Retrieves
                        |
                        ▼
                +----------------+
                |  database.c    |
                +-------+--------+
                        |
                 SQLite3 API
                        |
                        ▼
                +----------------+
                | chat_history.db|
                +----------------+
```

Only the database module communicates directly with SQLite.

---

# Database Initialization

When the server starts, it performs the following steps:

1. Open the database file.
2. Create required tables if they do not exist.
3. Prepare SQL statements.
4. Verify successful initialization.
5. Continue accepting client connections.

If the database cannot be opened, the server reports an error and exits.

---

# Database File

The chat history is stored in a single database file.

```text
chat_history.db
```

SQLite automatically creates this file if it does not already exist.

---

# Database Schema

A typical table for storing chat messages might look like:

```sql
CREATE TABLE IF NOT EXISTS chat_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    message TEXT NOT NULL,
    timestamp INTEGER NOT NULL
);
```

### Column Description

| Column      | Description                              |
| ----------- | ---------------------------------------- |
| `id`        | Unique identifier for each message       |
| `username`  | Name of the sender                       |
| `message`   | Chat message                             |
| `timestamp` | Unix timestamp when the message was sent |

---

# Saving Messages

Whenever a client sends a chat message:

```text
Client
   │
   ▼
Server
   │
   ▼
Validate Packet
   │
   ▼
Store in SQLite
   │
   ▼
Broadcast Message
```

This ensures that every valid message is persisted before being delivered to other clients.

---

# Retrieving Chat History

When a client requests previous messages:

```text
Client
   │
   ▼
History Command
   │
   ▼
database.c
   │
   ▼
SQLite Query
   │
   ▼
Results Returned
   │
   ▼
Client
```

The database module executes a query, formats the results, and returns them to the requesting client.

---

# SQL Operations

The database module primarily performs four operations:

## Create Table

Creates the required tables during server startup.

---

## Insert Message

Stores a new chat message.

Example:

```sql
INSERT INTO chat_history(username, message, timestamp)
VALUES (?, ?, ?);
```

Prepared statements should be used to safely bind values.

---

## Retrieve History

Returns previously stored chat messages.

Example:

```sql
SELECT username, message, timestamp
FROM chat_history
ORDER BY timestamp ASC;
```

---

## Close Database

Before shutting down, the server closes the database connection to ensure all resources are released properly.

---

# Prepared Statements

Instead of constructing SQL queries manually, prepared statements should be used.

Advantages include:

* Better performance
* Protection against SQL injection
* Cleaner code
* Easier parameter binding

Typical workflow:

1. Prepare SQL statement.
2. Bind parameters.
3. Execute statement.
4. Read results (if applicable).
5. Finalize statement.

---

# Error Handling

The database module checks for errors after every SQLite operation.

Common errors include:

* Database cannot be opened
* SQL syntax error
* Failed query execution
* Disk full
* Database locked
* Corrupted database file

Meaningful error messages simplify debugging and maintenance.

---

# Transactions

SQLite supports transactions, allowing multiple database operations to be grouped into a single atomic unit.

Benefits include:

* Improved consistency
* Better performance for batch operations
* Automatic rollback if an error occurs

While this project mainly performs individual inserts, transactions can be useful when implementing more advanced features.

---

# Resource Management

To prevent resource leaks, the database module should:

* Finalize prepared statements
* Close the database before exiting
* Check all SQLite return codes
* Free allocated resources

Proper cleanup contributes to long-running server stability.

---

# Future Improvements

The database layer can be extended with features such as:

* User accounts
* Password authentication
* Group chat storage
* File transfer metadata
* Message editing
* Message deletion
* Conversation history
* User activity logs
* Indexed searches for faster lookups

---

# Why Keep Database Logic Separate?

Separating database functionality into its own module provides several advantages:

* Easier maintenance
* Improved readability
* Better modularity
* Reusable database functions
* Simpler testing and debugging

Other parts of the application interact with the database through well-defined functions rather than issuing SQL queries directly.

---

# Summary

The database module provides persistent storage for chat messages using SQLite3. By isolating all database operations within a dedicated component, the project remains modular, maintainable, and easy to extend. SQLite's lightweight design makes it an excellent choice for an event-driven chat server while offering reliable storage without requiring a separate database server.
