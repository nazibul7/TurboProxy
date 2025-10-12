# Accepted Socket Writability vs Epoll Monitoring

When building a high-performance non-blocking proxy server, it's important to understand the difference between a socket's internal writability and the epoll events your application registers. This section explains why a newly accepted socket is almost always immediately writable, even if you don't register for EPOLLOUT.


## Key Concept: Socket State ≠ Epoll Monitoring Choice

The statement **"Server's accepted socket is immediately writable after accept()"** is **TRUE regardless** of your epoll monitoring choice!

### Socket Writability vs Epoll Events Are Different Things

#### 1. Socket State (Independent of Epoll)

```c
int connfd = accept(listen_fd, NULL, NULL);

// connfd IS immediately writable because:
// - TCP connection fully established
// - Send buffer empty (has space)
// - You can call send() successfully right now

send(connfd, "Hello!", 6, 0);  // This will likely succeed immediately
```

#### 2. My Epoll Choice:

```c
event.events = EPOLLIN;  // I choose to monitor only reads despite that socket will be writable
event.data.fd = connfd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event);
```

#### Why is a socket writable immediately after accept()?
Fresh TCP connection just established via 3-way handshake.

No data has been sent yet.

Therefore, the send buffer is completely empty.

Empty buffer means writable.

From the kernel’s perspective, "socket writable" doesn’t mean the peer is ready to read — it only means you can queue data to send into your local kernel buffer.

Whether the other side actually recv()s immediately or not is a different matter.

So, right after accept(), the socket's send buffer has maximum space, which makes the socket "ready for writing."

This is why if you had registered EPOLLOUT, you’d get an event right away.

In your code, you only registered EPOLLIN, so you don’t see EPOLLOUT. But if you added it, you’d see it immediately after accept().

🔹 Why does this matter?

If you don’t care about writability most of the time, you usually don’t add EPOLLOUT when first accepting a socket.

You add EPOLLOUT only when send() returns EAGAIN (meaning send buffer is full).

Then, once epoll notifies you again that the socket is writable, you retry sending.

✅ Answer to your question:
It’s true because the send buffer is initially empty after accept(). That’s why the socket is immediately writable. The reason you don’t see EPOLLOUT in your code is because you didn’t register for it.


👉 Statement:
“An accepted socket (on the server) is immediately writable (so EPOLLOUT would fire if registered).”

✅ Why is this (almost always) true?

When a TCP 3-way handshake completes, both sides have exchanged SYN/ACK packets.

At this point, the kernel considers the connection fully established.

That means:

The socket’s send buffer is empty (no pending data).

Unless the server’s own send buffer is artificially constrained (e.g., full due to system-wide limits), the kernel is ready to accept writes.

In epoll terms: EPOLLOUT is level-triggered, and it fires when the socket is writable. Since the buffer starts empty → writable immediately → EPOLLOUT would trigger.

So, yes, right after accept(), the new connfd is writable, unless something unusual blocks it.

❌ When is it not true?

There are rare cases:

Send buffer exhaustion (global or per-socket limit)

If the system is under memory pressure or the process has hit SO_SNDBUF limits, the accepted socket may not be writable immediately.

Then EPOLLOUT won’t fire until space is available.

Non-blocking connect() (client-side)

On the client side, a non-blocking connect() may give you a socket that isn’t writable immediately (EPOLLOUT fires only once the handshake completes).

But this doesn’t apply to server-side accept(), since the handshake is done.

Exotic TCP options (e.g., zero window advertisement from peer)

If the client, right after handshake, advertises a TCP zero window (it can’t receive data), then the server’s accepted socket is technically not writable. But that’s rare.

📝 In your code:
event.events = EPOLLIN; // Only monitor for incoming data
event.data.fd = connfd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event);


You didn’t register for EPOLLOUT.
So even though the socket is writable, you just don’t get notified — because epoll won’t tell you about events you didn’t subscribe to.

👉 Final Answer:
Yes — an accepted socket is almost always immediately writable because its send buffer is empty after the handshake. That’s why EPOLLOUT would trigger if you register for it. The only exceptions are unusual conditions (buffer full, zero window, resource exhaustion).




## 🔍 What Actually Happens During accept():

### Before accept():

```c
// Listening socket - NO send buffer for individual connections
int server_socket = socket(AF_INET, SOCK_STREAM, 0);
bind(server_socket, ...);
listen(server_socket, ...);  // Just listening, no data buffers per connection
```

### During accept():

```c
int client_fd = accept(server_socket, ...);
```

### What the kernel does internally:

Takes the completed connection from the accept queue
Creates a brand new socket structure (client_fd)
Allocates fresh send/receive buffers for this new socket
Initializes these buffers as empty
Returns the new file descriptor

### 📊 Visual Representation:

```
Before accept():
┌─────────────────┐
│  server_socket  │
│   (listening)   │  ← No individual connection buffers
│                 │
│  Accept Queue:  │
│  [conn1][conn2] │  ← Completed connections waiting
└─────────────────┘

After accept():
┌─────────────────┐       ┌─────────────────┐
│  server_socket  │       │   client_fd     │ ← NEW socket created!
│   (listening)   │       │                 │
│                 │       │ Send Buffer:    │ ← FRESH/EMPTY buffer
│  Accept Queue:  │  ────▶│ [            ] │   allocated by kernel
│  [conn2]        │       │                 │
│                 │       │ Recv Buffer:    │ ← FRESH/EMPTY buffer  
└─────────────────┘       │ [            ] │   allocated by kernel
                          └─────────────────┘

```

### 💡 The Key Distinction:

**Writable** = "Can I put data INTO this buffer?"
    Empty buffer = YES, has space = **Writable**

**Readable** = "Is there data FOR ME in this buffer?"
    Empty buffer = NO, nothing to read = **NOT Readable**
