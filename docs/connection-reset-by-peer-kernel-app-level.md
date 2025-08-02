# 🔍 Connection Reset by Peer — Kernel-Level RST Before `accept()`

This note explores the low-level mechanics of how a TCP connection can be reset **before the server ever calls `accept()`**. This complements the [SIGPIPE crash scenario](./sigpipe_connection_reset_analysis.md) and explains the other side of the “Connection reset by peer” error.

---

## 🧠 What Does "Connection Reset by Peer" Mean?

It means that the **peer** (usually the client or maybe server) has **forcefully closed the TCP connection** — by sending a **TCP RST (Reset)** packet — instead of doing a graceful FIN/ACK(Finished/Acknowledge) shutdown.

---

## 🔬 Root Cause Scenario

Even before a server accepts a client connection, the **client may give up** waiting. This can happen if:

- The server is overloaded and doesn't call `accept()` quickly
- The client's timeout elapses
- The client crashes or closes early

---

## ⚙️ Kernel-Level Behavior (Before `accept()`)

### 📌 Timeline:

1. TCP **3-way handshake** completes:
   - SYN → SYN-ACK → ACK
   - Kernel queues the connection in the **backlog queue**

2. Server hasn't yet called `accept()`.

3. The **client times out or closes** the socket.

4. Client sends a **TCP RST** (Reset) packet to the server.

5. The kernel:
   - Sees the RST
   - **Removes** the connection from the backlog queue
   - Or sometimes still allows `accept()` to succeed — with a socket already in error

---

## 🛠️ Application-Level Behavior (After `accept()`)

If your server **accepts** a connection that was reset:

- `accept()` may return a **valid socket descriptor**
- But the connection is already broken
- As soon as you try `send()` or `write()`:
  
```c
ssize_t n = send(fd, buffer, len, 0);
if (n == -1) {
    perror("send");
    // errno == 104 → Connection reset by peer
}


🧠 Two kernel behaviors (depends on timing + kernel version):
✅ 1. Drop the connection silently
The RST is seen.

Kernel removes the connection from the backlog.

accept() does not return that connection.

Your server never sees the client.

This is ideal — you avoid a bad socket.

⚠️ 2. Allow accept() to return a socket
The 3-way handshake completed → connection was established.

RST arrives after that but before accept().

The kernel still lets accept() succeed, but:

The returned socket is already closed/reset.

Any recv() or send() on it will fail immediately with ECONNRESET.

This happens because TCP is connection-oriented: once the handshake is complete, the kernel “technically” has a valid connection to return — even if it’s broken now.

📌 Why this happens:
TCP state machine: once in ESTABLISHED, the kernel moves the socket to the accept queue.

RST updates internal state, but doesn’t always remove from accept queue in time.

Some OSes delay processing of RST until after accept() is called — especially under high load.

