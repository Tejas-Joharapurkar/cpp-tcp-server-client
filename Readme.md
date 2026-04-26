# TCP Socket Server — Binary Protocol & Code Generator

I built this to get a deeper understanding of how networked systems actually work at a low level — not just "use a library and call it a day" but actually understanding what happens on the wire.

The project has three main parts: a multithreaded TCP server/client, a custom binary wire protocol, and a Python code generator that automatically produces C++ message classes from a schema file. The protocol design is inspired by how trading systems serialize and deserialize messages in production.

---

## What's in here

**TCP Server & Client**

The server accepts multiple clients and spawns a read thread and write thread per connection. The read thread pulls messages off the socket, decodes them, and pushes responses into a queue. The write thread sits on the other end of that queue and sends responses back. Classic producer-consumer with a mutex and condition variable keeping things in sync.

The client side supports multiple simultaneous connections — each connection has its own send/recv threads and logs everything with timestamps so you can see exactly what's happening across connections.

**Binary Wire Protocol**

Every message on the wire looks like this:

```
[ Header ] [ Body ]
  bodyLen      field-by-field binary data
  networkID
  templateID
```

The `templateID` is how the receiver knows which decoder to call — same idea as a decoder registry in trading systems. The `bodyLen` solves the TCP framing problem: since TCP is a stream with no message boundaries, the receiver uses `bodyLen` to know exactly how many bytes to read. Partial reads are handled with a loop, and the leftover buffer pattern handles the case where a single `recv` gives you 1.5 messages.

All sizes use fixed-width types (`uint32_t`, `uint16_t` etc.) instead of `int` or `long` because the size of those types varies across platforms — on a wire protocol that is a bug waiting to happen.

**Memory Alignment**

Rather than slapping `#pragma pack` on everything, the code handles alignment explicitly. The idea is simple — a CPU reads memory in fixed-size chunks, so if a field straddles a chunk boundary it either causes extra reads or crashes on ARM. The fix is to insert filler bytes before any field that would split across a boundary, and pad the end of the struct so the total size is a multiple of 8.

The alignment rule: `offset % field_size == 0` before placing any field. If not, insert a filler of `field_size - (offset % field_size)` bytes first.

**Python Code Generator**

This is the part I found most interesting to build. You write a schema file:

```
message Order {
    double   price
    uint32   quantity
    char[8]  symbol
    uint32   side
}
```

The generator parses it, sorts fields in descending size order to minimize padding, calculates where fillers need to go, and spits out a C++ struct with correct alignment — no `#pragma pack` needed because the layout is already correct by construction.

The inspiration came from how trading firms handle message definitions — instead of writing encode/decode by hand for every message type, you define the schema once and generate the boilerplate.

---

## How to run

```bash
# Build
g++ -std=c++17 -o server server.cxx message.cxx -lpthread
g++ -std=c++17 -o client client.cxx message.cxx -lpthread

# Run server first, then client
./server
./client
```

---

## What's next

- Replace thread-per-client with an `epoll` event loop
- RESP protocol parser (Redis wire format)
- Mini Redis clone — SET, GET, DEL, TTL over TCP
- Hook the code generator up to produce full encode/decode functions, not just the struct







This project implements a TCP-based client-server system in C++ with support for handling multiple concurrent client connections. It features a custom binary message encoding and decoding mechanism along with a TCP framing protocol to ensure reliable and structured communication over streaming sockets.

The system demonstrates low-level network programming using POSIX socket APIs, efficient memory handling, and concurrency management. It simulates real-world backend communication by enabling structured data exchange between multiple clients and the server.

Key features include:

.Concurrent client handling using multithreading
.Custom message encoding/decoding for efficient data transfer
.TCP framing protocol to handle partial reads/writes
.Low-level socket programming using POSIX APIs

This is version one i will be imporving it and also make it more clean.


To run the application just run 
g++ main.cxx message.cxx -o Server
g++ Client.cxx message.cxx -o Client
and then start the server with ./Server
and then client as ./Client