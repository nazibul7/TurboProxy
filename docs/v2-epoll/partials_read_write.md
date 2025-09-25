## Partial Reads and Writes in TCP

### 📌 What is a Partial Read?

A partial read occurs when a single recv() system call does not return the full application-level message you expect.
This means you didn’t drain the kernel buffer fully in one recv() call. This is normal in TCP because:

1. TCP is a byte stream protocol, not message-oriented.

2. Data may arrive in fragments over the network.

3. The kernel may return fewer bytes than you asked for, even if more are available.

#### 🔹 Example: Partial Read
Suppose an HTTP request is 1024 bytes long and already queued in the kernel buffer:

```c
[GET /index.html HTTP/1.1\r\nHost: example.com\r\n...]
```

You call:

```c
char buf[512];
int n = recv(fd, buf, 512, 0);
```

🟣 Kernel copies 512 bytes into buf.
🟣 Remaining 512 bytes stay in the kernel buffer.
🟣 You got a partial read.
🟣 The next recv() will return the rest.



### 📌 What is a Partial Write?

A partial write occurs when a single send() (or write()) system call writes fewer bytes than requested.

This happens when the kernel’s socket send buffer is full. You must retry sending the remaining data.

#### 🔹 Example: Partial Write
```c
char data[4096];
ssize_t sent = send(fd, data, sizeof(data), 0);

if (sent < sizeof(data)) {
    // Only part of the data was sent
    size_t remaining = sizeof(data) - sent;
    // Must retry sending `remaining` bytes
}
```



### Data arriving later (separate scenario)

This means you did drain the kernel buffer completely, but the sender hasn’t finished sending yet.

Example: peer sends 1024 bytes total, but only 300 have arrived in the kernel when you call recv().

You call recv(fd, buf, 4096, 0) and get 300. The kernel buffer is now empty.

The remaining 724 bytes arrive later, generating another event (in LT mode) or requiring you to loop until EAGAIN (in ET mode).

This is not a “partial read” in the strict sense — it’s just TCP delivering data gradually.