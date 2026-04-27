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

Rather than slapping `#pragma pack` on everything, the code handles alignment explicitly. The idea is simple — a CPU reads memory in fixed-size chunks (8 bytes on 64-bit), so if a field straddles a chunk boundary it either causes extra reads or crashes on ARM. The fix is to insert filler bytes before any field that would split across a boundary, and pad the end of the struct so the total size is a multiple of 8.

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

The generator parses the schema, sorts fields in descending size order to minimize padding, calculates where fillers need to go, and generates a C++ struct + encode/decode functions — no `#pragma pack` needed because the layout is already correct by construction.

The inspiration came from how trading firms handle message definitions — instead of writing encode/decode by hand for every message type, you define the schema once and generate the boilerplate.

Currently supported:
- Primitive types — `double`, `uint32`, `uint16`, `uint8`
- Fixed-size char arrays — `char[N]`
- Automatic filler insertion for inter-field and end padding
- Generates `.hxx` (struct definition) and `.cxx` (encode/decode implementation)

Work in progress:
- Nested custom struct as field type
- Header encode/decode with runtime `bodyLen` calculation
- Dependency ordering — generate leaf structs before structs that use them

---

## How to run

```bash
# Build server and client
g++ -std=c++17 -o server server.cxx message.cxx -lpthread
g++ -std=c++17 -o client client.cxx message.cxx -lpthread

# Run server first, then client
./server
./client

# Run the code generator
python3 generator.py messages.msg
# Produces: Messages/Order.hxx, Messages/Order.cxx
```

---

## What's next

- Nested struct support in the generator
- Header encode/decode generation
- Ring buffer for partial TCP read handling
- Replace thread-per-client with an `epoll` / `kqueue` event loop
- RESP protocol parser (Redis wire format)
- Mini Redis clone — SET, GET, DEL, TTL over TCP