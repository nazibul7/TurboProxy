# Design Decisions for v1: Blocking Proxy

## Why Use Blocking I/O?

- Easiest model to get started with
- Simple to write, test, and debug
- Helps focus on core logic like:
  - HTTP request parsing
  - Connecting to backend servers
  - Forwarding data correctly

## Why No Thread Pool or `epoll`?

- This is the first version that's why kept it minimal
- One request at a time
- Adding threads and event loops adds complexity — we’ll handle that in the next phase

## Why Use `Connection: close`?

- Makes socket handling much easier
- Server closes connection after one request-response
- No need to manage idle or long-lived connections

## File Structure and Responsibilities


| File                     | Responsibility                                                                 |
|--------------------------|--------------------------------------------------------------------------------|
| `main.c`                 | Entry point; sets up the proxy server loop and handles connection lifecycle   |
| `server.c`               | Socket setup (bind, listen, accept), basic server operations                   |
| `proxy.c`                | Connects to the target server, forwards the request, and relays the response  |
| `request_parser.c`       | Parses HTTP requests (method, path, headers, etc.)                             |
| `rebuild_request.c`      | Reconstructs the HTTP request to be sent to the target after parsing          |
| `route_config.c`         | (Optional logic) Handles routing configuration (e.g., based on host/path)     |

---

This version gives us a clean, working baseline to improve from in future versions.
