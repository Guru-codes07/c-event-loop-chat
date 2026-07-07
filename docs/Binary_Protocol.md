#  Binary Protocol

## Overview

The Event-Driven Chat Server communicates using a **custom binary protocol** instead of plain text. Every message exchanged between the client and server follows a predefined packet format.

Using a binary protocol provides several advantages:

* Faster parsing
* Smaller packet size
* Consistent message format
* Easier validation
* Better scalability
* Reduced bandwidth usage

---

# Packet Structure

Every network packet consists of two parts:

1. Header
2. Payload

```text
+------------------------------------------------------+
|                     Header                           |
+------------------------------------------------------+
|                    Payload                           |
+------------------------------------------------------+
```

The header contains metadata about the message, while the payload contains the actual data.

---

# Packet Header

The packet header contains information required to validate and process a packet.

| Field          |    Size | Description                   |
| -------------- | ------: | ----------------------------- |
| Version        |  1 byte | Protocol version              |
| Message Type   |  1 byte | Type of packet                |
| Flags          | 2 bytes | Additional packet information |
| Payload Length | 2 bytes | Size of payload               |
| Reserved       | 2 bytes | Reserved for future use       |
| Message ID     | 4 bytes | Unique packet identifier      |
| Timestamp      | 8 bytes | Time when packet was created  |
| CRC32          | 4 bytes | Checksum used for validation  |

---

# Packet Layout

```text
+------------+
| Version    | 1 byte
+------------+
| Type       | 1 byte
+------------+
| Flags      | 2 bytes
+------------+
| Length     | 2 bytes
+------------+
| Reserved   | 2 bytes
+------------+
| Message ID | 4 bytes
+------------+
| Timestamp  | 8 bytes
+------------+
| CRC32      | 4 bytes
+------------+
| Payload    | Variable
+------------+
```

---

# Header Fields

## Version

Indicates the protocol version.

Current version:

```text
1
```

This allows future versions of the protocol without breaking compatibility.

---

## Message Type

Identifies the purpose of the packet.

Examples include:

* Connection
* Chat
* Private Message
* Server Notification
* Error
* Command

The receiver uses this field to determine how the payload should be interpreted.

---

## Flags

Flags provide additional information about the packet.

Possible uses include:

* Acknowledgement
* Error indicator
* Broadcast message
* Private message
* Reserved for future features

---

## Payload Length

Specifies the number of bytes contained in the payload.

Example:

```text
Payload = "Hello"

Length = 5 bytes
```

The receiver uses this value to know how many bytes to read.

---

## Reserved

Currently unused.

Reserved bytes allow the protocol to be extended in the future without changing the packet layout.

---

## Message ID

Every packet has a unique identifier.

Example:

```text
Message ID: 1042
```

Possible uses:

* Debugging
* Logging
* Request tracking
* Duplicate detection

---

## Timestamp

Stores the time when the packet was created.

Benefits:

* Event ordering
* Logging
* Chat history
* Future synchronization

---

## CRC32

CRC32 is used to verify packet integrity.

Before sending:

1. Compute CRC32 of the payload.
2. Store the checksum in the header.

After receiving:

1. Recompute CRC32.
2. Compare with the received value.
3. Reject the packet if the values differ.

This helps detect corrupted packets during transmission.

---

# Payload

The payload contains the actual message.

Examples:

```text
Hello Everyone!
```

```text
Guru
```

```text
/msg Alice Hi!
```

Its interpretation depends on the message type.

---

# Supported Message Types

| Message Type          | Description                     |
| --------------------- | ------------------------------- |
| `MSG_CONNECTION`      | Client joins the server         |
| `MSG_CHAT`            | Public chat message             |
| `MSG_PRIVATE_MSG`     | Private message                 |
| `MSG_SERVER`          | Server notification             |
| `MSG_ERROR`           | Error message                   |
| `MSG_WHO_COMMAND`     | Request list of connected users |
| `MSG_HISTORY_COMMAND` | Retrieve stored chat history    |
| `MSG_DISCONNECT`      | Client disconnect               |

---

# Packet Processing

Incoming packets follow this sequence:

```text
Socket
   │
   ▼
Receive Bytes
   │
   ▼
Read Header
   │
   ▼
Validate Length
   │
   ▼
Validate CRC32
   │
   ▼
Read Payload
   │
   ▼
Process Message Type
```

Only valid packets are processed.

---

# Serialization

Before transmission, packet fields are converted into a byte stream.

```text
Packet Structure
        │
        ▼
Serialize Fields
        │
        ▼
Send Bytes
```

Serialization ensures consistent communication between the client and server.

---

# Deserialization

When a packet is received:

```text
Receive Bytes
        │
        ▼
Parse Header
        │
        ▼
Extract Payload
        │
        ▼
Create Packet Structure
```

The server reconstructs the original packet from the received bytes.

---

# Network Byte Order

Multi-byte integer fields should be transmitted using **network byte order (big-endian)**.

Common conversion functions include:

* `htons()`
* `htonl()`
* `ntohs()`
* `ntohl()`

These functions ensure compatibility across systems with different CPU architectures.

---

# Error Handling

The protocol performs several validation checks before processing a packet:

* Invalid protocol version
* Unknown message type
* Invalid payload length
* CRC32 mismatch
* Corrupted packet
* Malformed header
* Unexpected packet size

Invalid packets are discarded to maintain server stability.

---

# Why Use a Binary Protocol?

Compared to plain-text protocols, a binary protocol offers:

* Faster parsing
* Reduced bandwidth usage
* Predictable packet layout
* Better data validation
* Improved scalability
* Easier support for future protocol extensions

---

# Summary

The custom binary protocol defines a structured and efficient way for clients and the server to communicate. By combining a fixed header, variable-length payload, CRC32 validation, and explicit message types, it provides reliable, scalable, and maintainable communication suitable for event-driven network applications written in C.
