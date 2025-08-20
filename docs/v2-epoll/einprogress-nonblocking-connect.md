The EINPROGRESS error only appears when you call connect() on a non-blocking socket.

🌐 Client side (connect())

Client calls connect() on a non-blocking socket

Kernel starts the 3-way handshake (SYN → SYN/ACK ← ACK).

Since it cannot complete immediately, connect() returns -1 with errno = EINPROGRESS.

During handshake in progress

The socket is not yet writable.

After 3-way handshake completes (connection established)

The client’s socket becomes writable, so epoll reports EPOLLOUT.

This means: “you can now send data (connection is ready).”

✅ So: Client gets EPOLLOUT after handshake success.



🖥️ Server side (accept())

Server is listening socket in epoll (registered with EPOLLIN).

When handshake finishes (after final ACK from client), the kernel queues the new connection.

At this moment, the listening socket becomes readable, so epoll reports EPOLLIN on the listening fd.

That’s the signal to call accept().

After accept() returns a client fd

That fd is also registered in epoll.

When the client actually sends application data (e.g., HTTP request), epoll will notify with EPOLLIN on the client socket.

✅ So: Server gets EPOLLIN on the listening socket after handshake, then later EPOLLIN on the accepted client socket when real data arrives.





Client                          Server
  |                               |
  |                         [Listening socket]
  |                         monitoring EPOLLIN
  |                               |
connect() called (non-blocking)   |
returns EINPROGRESS               |
  |                               |
[Client socket monitoring EPOLLOUT]|
  |                               |
  |---- SYN ------------------>    |
  |                               |
  |<--- SYN+ACK ---------------    |   ← 🔥 Server: EPOLLIN triggers HERE!
  |                               |     (SYN+ACK sent, kernel has queued connection)
  |                               |     accept() can be called safely
  |---- ACK ------------------>    |
  |                               |
  | ← 🔥 Client: EPOLLOUT triggers HERE!
  |                               |
