# 🔄 Non-blocking connect() and accept() with epoll

When using **non-blocking sockets**, `epoll` reports readiness at different stages for **client** and **server**.

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
