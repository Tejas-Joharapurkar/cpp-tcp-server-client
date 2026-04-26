This project implements a TCP-based client-server system in C++ with support for handling multiple concurrent client connections. It features a custom binary message encoding and decoding mechanism along with a TCP framing protocol to ensure reliable and structured communication over streaming sockets.

The system demonstrates low-level network programming using POSIX socket APIs, efficient memory handling, and concurrency management. It simulates real-world backend communication by enabling structured data exchange between multiple clients and the server.

Key features include:

.Concurrent client handling using multithreading
.Custom message encoding/decoding for efficient data transfer
.TCP framing protocol to handle partial reads/writes
.Low-level socket programming using POSIX APIs

This is version one i will be imporving it and also make it more clean.