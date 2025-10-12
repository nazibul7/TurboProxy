# 🔄 Non-blocking connect() and accept() with epoll

When using **non-blocking sockets**, `epoll` reports readiness at different stages for **client** and **server**.

**The client's blocking/non-blocking mode and the server's blocking/non-blocking mode are completely independent and do NOT affect each other.**

---

## 🌐 Client side (`connect()`)

1. Client calls `connect()` on a non-blocking socket.  
   - Kernel begins the **3-way handshake**: `SYN → SYN/ACK ← ACK`.  
   - `connect()` immediately returns `-1` with `errno = EINPROGRESS`.

2. While the handshake is in progress:  
   - The socket is **not yet writable**.  

3. When the handshake finishes:  
   - The socket becomes **writable**, so `epoll` reports **`EPOLLOUT`**.  
   - This means the connection is ready, and you can now `send()` data.

✅ **Client sees `EPOLLOUT` after handshake success.**

---

## 🖥️ Server side (`accept()`)

1. Server has the **listening socket** registered with `EPOLLIN`.  

2. When the handshake completes (final ACK received from client):  
   - The kernel **queues the new connection**.  
   - At this moment, `epoll` reports **`EPOLLIN`** on the **listening socket**.  
   - This is the signal to call `accept()`.

3. After `accept()` returns a new client socket:  
   - Register it with `epoll`.  
   - Later, when the client sends real application data (e.g., HTTP request),  
     `epoll` reports **`EPOLLIN`** on the client socket.

✅ **Server gets EPOLLIN on the listening socket after handshake, then later EPOLLIN on the accepted client socket when real data arrives**
---
## After 3-way handshake

✅ Both sides get kernel send/receive buffers (automatically managed)
✅ Send buffers are empty → EPOLLOUT ready on both sides
✅ Receive buffers are empty → EPOLLIN not ready on both sides

### Server Side (after accept()):

✅ EPOLLOUT set immediately → Send buffer is empty, ready to write
✅ EPOLLIN NOT set → No data from client yet, nothing to read

### Client Side (after connect()):

✅ EPOLLOUT set immediately → Send buffer is empty, ready to write
✅ EPOLLIN NOT set → No data from server yet, nothing to read
---

## 🔁 Sequence Diagram

```text
Client                          Server
  |                               |
  |                         [Listening socket]
  |                         (monitor EPOLLIN)
  |                               |
connect() non-blocking            |
→ returns EINPROGRESS             |
  |                               |
[monitor EPOLLOUT]                |
  |                               |
  |---- SYN ------------------>   |
  |                               |
  |<--- SYN+ACK ---------------   |  🔥 EPOLLIN triggers HERE!
  |                               |    (SYN+ACK sent, kernel has queued connection)
  |                               |    accept() can be called safely
  |---- ACK ------------------>   |
  |                               |
  | ← 🔥 EPOLLOUT here!           |
  |                               |  (client can now send data)
  |                               |
  |---- HTTP Request ---------->  |
  |                               |  🔥 EPOLLIN on accepted client socket
```

| Client Operation        | Server Operation            | Notes / Behavior                                      |
|------------------------|----------------------------|------------------------------------------------------|
| `connect()` - initiates connection | (no equivalent)             | Non-blocking client returns `-1` with `errno = EINPROGRESS` while handshake is in progress |
| `send()` - sends data    | `send()` - sends data       | Both sides return `-1` with `errno = EAGAIN` if the operation would block |
| `recv()` - receives data | `recv()` - receives data    | Both sides return `-1` with `errno = EAGAIN` if no data is available |
| (no equivalent)          | `accept()` - accepts connection | Non-blocking server returns `-1` with `errno = EAGAIN` if no connection is ready in the queue |


**The server has no operation equivalent to connect()!**

## What About listen()?
```
listen(server_sock, 10);
// This is NOT like connect()
// It's just saying "I'm ready to accept connections"
// It completes instantly, never blocks
// Even in non-blocking mode, it just succeeds immediately
```
listen() doesn't initiate a network operation, it just configures the socket.

**❌ Server does NOT get EINPROGRESS**
**✅ Server gets EAGAIN/EWOULDBLOCK from non-blocking accept()**
**✅ Client gets EINPROGRESS from non-blocking connect()**

