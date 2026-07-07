# Event Loop

## Overview

The Event-Driven Chat Server uses an **event loop** based on the POSIX `poll()` system call to manage multiple client connections efficiently.

Instead of creating one thread for every client, a single event loop continuously monitors all active sockets and processes them only when an event occurs.

This design is widely used in modern network servers because it scales better while consuming fewer system resources.

---

# What is an Event Loop?

An event loop is a programming model that repeatedly waits for events and reacts when they occur.

Typical events include:

* A new client connects
* A client sends data
* A client disconnects
* A socket encounters an error

The server sleeps while no events are available and wakes only when there is work to do.

---

# Why Use an Event Loop?

Traditional servers often create one thread per client.

```text id="thrdsrv"
Client 1 ── Thread 1

Client 2 ── Thread 2

Client 3 ── Thread 3

...

Client N ── Thread N
```

Although simple, this approach has drawbacks:

* High memory usage
* Frequent context switching
* Thread synchronization complexity
* Limited scalability

The event-driven model avoids these issues.

---

# Event-Driven Architecture

```text id="eventarch"
           Clients
               │
               │
               ▼
        Listening Socket
               │
               ▼
        poll() Event Loop
               │
   ┌───────────┼───────────┐
   │           │           │
   ▼           ▼           ▼
Read Data  Accept Client  Disconnect
   │
   ▼
Process Packet
   │
   ▼
Broadcast / Respond
```

A single thread manages all connected clients.

---

# How `poll()` Works

`poll()` monitors multiple file descriptors simultaneously.

The server provides:

* An array of file descriptors
* The number of descriptors
* A timeout value

`poll()` blocks until one or more descriptors become ready for I/O.

Typical events include:

* Ready to read (`POLLIN`)
* Ready to write (`POLLOUT`)
* Error (`POLLERR`)
* Hang-up (`POLLHUP`)

This allows the server to react only when necessary.

---

# Main Event Loop

The server repeatedly performs the following steps:

```text id="loopflow"
Initialize Server
        │
        ▼
Create Listening Socket
        │
        ▼
Add Socket to poll()
        │
        ▼
Enter Infinite Loop
        │
        ▼
Wait for Events
        │
        ▼
Process Ready Sockets
        │
        ▼
Repeat
```

The loop continues until the server shuts down.

---

# Accepting New Clients

When the listening socket becomes readable, it indicates that a new client is attempting to connect.

The server then:

1. Calls `accept()`
2. Creates a client socket
3. Adds the socket to the `poll()` array
4. Begins monitoring the new client

This allows new clients to join without interrupting existing connections.

---

# Receiving Messages

When a client socket becomes readable:

```text id="recvflow"
poll()
   │
   ▼
Client Ready
   │
   ▼
recv()
   │
   ▼
Read Packet
   │
   ▼
Validate Protocol
   │
   ▼
Execute Command
```

Only sockets with available data are processed.

---

# Broadcasting Messages

After a valid chat message is received:

```text id="broadcast"
Client
   │
   ▼
Server
   │
   ▼
Process Packet
   │
   ▼
Loop Through Connected Clients
   │
   ▼
send()
```

The server sends the message to all appropriate recipients.

---

# Handling Client Disconnects

A client may disconnect intentionally or unexpectedly.

Common causes include:

* Client exits
* Network failure
* Socket error

When detected, the server:

1. Removes the socket from the `poll()` array
2. Closes the socket
3. Frees associated resources
4. Updates the active client list

This prevents invalid file descriptors from remaining in the event loop.

---

# File Descriptor Management

Each connected client is represented by a file descriptor.

Example:

```text id="fds"
Listening Socket : FD 3

Client A         : FD 4

Client B         : FD 5

Client C         : FD 6
```

These descriptors are stored in a `pollfd` array monitored by `poll()`.

---

# Advantages of `poll()`

Using `poll()` provides several benefits:

* Handles many clients in one thread
* Lower memory consumption
* Reduced context switching
* Simpler synchronization
* Portable across POSIX systems
* Straightforward implementation

For educational networking projects, `poll()` offers an excellent balance between simplicity and scalability.

---

# Limitations of `poll()`

While powerful, `poll()` has some limitations.

* Scans every file descriptor on each call
* Performance decreases as the number of connections grows
* Less efficient than platform-specific alternatives for very large servers

For thousands of concurrent connections, Linux applications often use `epoll()` instead.

---

# `poll()` vs `epoll()`

| Feature                           | poll()   | epoll()         |
| --------------------------------- | -------- | --------------- |
| POSIX Standard                    | Yes      | No (Linux only) |
| Easy to Learn                     | Yes      | Moderate        |
| Suitable for Small/Medium Servers | Yes      | Yes             |
| Large Scale Performance           | Moderate | Excellent       |
| Used in This Project              | ✅        | ❌               |

`poll()` was chosen because it is portable, easier to understand, and well-suited for learning event-driven programming.

---

# Event Loop Lifecycle

```text id="lifecycle"
Start Server
      │
      ▼
Initialize poll()
      │
      ▼
Wait for Events
      │
      ▼
Accept New Clients
      │
      ▼
Receive Packets
      │
      ▼
Process Commands
      │
      ▼
Send Responses
      │
      ▼
Handle Disconnects
      │
      ▼
Repeat
```

This cycle continues for the lifetime of the server.

---

# Best Practices

To build a reliable event-driven server:

* Keep the event loop lightweight.
* Avoid blocking operations inside the loop.
* Validate all incoming packets.
* Remove disconnected clients immediately.
* Check return values from all socket functions.
* Handle errors gracefully.
* Keep networking logic separate from application logic.

Following these practices improves responsiveness and maintainability.

---

# Summary

The event loop is the core of the Event-Driven Chat Server. By using `poll()`, the server efficiently monitors multiple client connections within a single thread, avoiding the overhead of thread-per-client designs. This architecture provides a clean, scalable foundation for learning modern network programming concepts such as asynchronous I/O, socket management, protocol handling, and event-driven application design.
