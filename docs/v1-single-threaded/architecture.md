# v1 Architecture: Blocking Single-Threaded Proxy Server

## Overview

This version implements a basic HTTP proxy server using:

- A single thread
- Blocking I/O
- Connection: close behavior (no keep-alive)
- No `epoll` or thread pool

The server accepts one connection at a time and processes it sequentially.

## Components

- `main.c`: Entry point; sets up socket, binds, listens, accepts
- `request_parser.c`: Parses HTTP requests
- `rebuild_request.c`: Reconstruct HTTP request to be sent to target backend
- `proxy.c`: Connects to target server,forwards request and sends back the response to the client
- `server.c`: Sets up socket, binds, listens, accepts
- `route_config.c`:Handles routing configuration

## Flow Diagram

[client] → [accept()] → [parse request] → [connect to target] → [forward] → [recv response] → [send to client] → [close]


## Limitations

- Can handle only one connection at a time
- Blocking calls make it inefficient under load
- No resource reuse (sockets or threads)


## Known Issues
- Connection handling: See [troubleshooting/connection-issues.md](troubleshooting/connection-issues.md)
- SIGPIPE crashes: See [troubleshooting/sigpipe_connection_reset_analysis.md](troubleshooting/sigpipe_connection_reset_analysis.md)
- Memory analysis: See [troubleshooting/valgrind-memory-error.md](troubleshooting/valgrind-memory-error.md)
