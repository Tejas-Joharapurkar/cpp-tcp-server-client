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

The `templateID` is how the receiver knows which decoder to call — same idea as a decoder registry in trading systems. The `bodyLen` solves the TCP framing problem: since TCP is a stream with no message boundaries, the receiver uses `bodyLen` to know exactly how many bytes to read.

All sizes use fixed-width types (`uint32_t`, `uint16_t` etc.) instead of `int` or `long` because the size of those types varies across platforms — on a wire protocol that is a bug waiting to happen.

**ProtocolHandler — Partial Read Handling**

TCP doesn't guarantee that a single `recv` gives you a complete message. You might get half a message, or one and a half. The `ProtocolHandler` class solves this cleanly.

It maintains an internal buffer and keeps reading until a complete message is available. It does this in two steps — first checks if there are enough bytes for the header, reads `bodyLen` from it, then waits until the full body has arrived too. Once a complete message is ready, it exposes a zero-copy pointer directly into the buffer so the caller can decode without any extra allocation.

```cpp
// Usage
while (true) {
    handler.Read();                        // blocks until complete message
    char* raw = handler.getMessage();      // zero-copy pointer into buffer
    decode(raw, order);                    // decode directly
    handler.consumeMessage();              // advance buffer
}
```

The buffer also handles the 1.5 message case — when a single `recv` returns more than one message, the leftover bytes stay in the buffer and the next call to `Parse()` picks them up immediately without another `recv`.

**Memory Alignment**

Rather than slapping `#pragma pack` on everything, the code handles alignment explicitly. The idea is simple — a CPU reads memory in fixed-size chunks, so if a field straddles a chunk boundary it either causes extra reads or crashes on ARM. The fix is to insert filler bytes before any field that would split across a boundary, and pad the end of the struct so the total size is a multiple of 8.

The alignment rule: `offset % field_size == 0` before placing any field. If not, insert a filler of `field_size - (offset % field_size)` bytes first.

**Python Code Generator**

You write a schema file:

```
message Header {
    uint32_t bodyLen
    uint32_t networkID
    uint16_t templateID
}

message Order {
    double   price
    uint32_t quantity
    char     symbol[8]
    uint32_t side
}
```

The generator parses it, sorts fields in descending size order to minimize padding, calculates where fillers need to go, and generates a C++ struct with encode/decode functions — no `#pragma pack` needed because the layout is already correct by construction.

It also supports nested custom types — you define them in a separate schema file, the generator compiles those first, builds a size map, and uses it when calculating layout for messages that reference them.

The inspiration came from how trading firms handle message definitions — instead of writing encode/decode by hand for every message type, you define the schema once and generate the boilerplate.

Currently supported:
- Primitive types — `double`, `uint32_t`, `uint16_t`, `uint8_t`
- Fixed-size char arrays — `char name[N]`
- Automatic filler insertion for inter-field and end padding
- Custom nested struct types via separate schema file
- Generates `.hxx` (struct + function declarations) and `.cxx` (encode/decode implementation)

---

## How to run

```bash
# Generate message classes from schema
python3 generator.py

# Build server and client
g++ -std=c++17 -o server server.cxx message.cxx ProtocolHndler.cxx -lpthread
g++ -std=c++17 -o client client.cxx message.cxx -lpthread

# Run server first, then client
./server
./client
```

---

## What's next

- epoll/kqueue based event loop — replace thread-per-client with a single-threaded event loop
- RESP protocol parser — Redis wire format on top of ProtocolHandler
- Mini Redis clone — SET, GET, DEL, TTL, single-threaded event loop like real Redis
- Generator — tagged union wrapper so all message types live under one `Message` struct