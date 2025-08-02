# 🔍 SIGPIPE and "Connection reset by peer" — Full A to Z Root Cause Analysis

## 📌 Scenario
While benchmarking a **single-threaded proxy server** using `ab` (ApacheBench), the client logs:
```
apr_socket_recv: Connection reset by peer (104)
Total of 924 requests completed
```
Meanwhile, the server crashes with:
```
Process terminating with default action of signal 13 (SIGPIPE)
==109105==    at 0x49888B0: send (send.c:28)
==109105==    by 0x109DA6: relay_response
```

---

## ✅ Step-by-Step Breakdown

### 1. **Benchmarking Begins**
- Client (`ab`) is started:
  ```bash
  ab -n 1000 -c 10 http://localhost:7000/
  ```
- This spawns 10 concurrent connections and sends 1000 requests total.

### 2. **Server Handles Requests**
- Proxy receives requests and forwards them to the backend.
- Then it uses `send(client_fd, buffer, len, 0)` to relay responses to clients.

### 3. **Client Disconnects Early**
- Due to timeout, overload, or early completion, some `ab` clients **close the socket** before the server responds.

### 4. **Server Sends to Closed Socket**
- When the server tries to `send()` to a socket that’s already closed, the kernel detects this and raises **SIGPIPE**.

### 5. **SIGPIPE Kills the Server**
- By default, unhandled `SIGPIPE` terminates the process.
- Server crashes while in `relay_response()`.

### 6. **Client Reports Connection Reset**
- Because the server crashes mid-communication, the client sees:
  ```
  Connection reset by peer (104)
  ```
- Only a partial number of requests (e.g., 924/1000) are completed.

---

## 🔄 Who Did What?

| Actor         | Action                                      | Result                            |
|---------------|---------------------------------------------|-----------------------------------|
| **Client (`ab`)** | Closed socket early (timeout, error)         | Server couldn’t send              |
| **Server**        | Called `send()` on closed socket             | Kernel raised `SIGPIPE`           |
| **Kernel**        | Sent `SIGPIPE` to server                     | Server crashed (default behavior) |
| **Client (`ab`)** | Detected abrupt close                        | Logged "Connection reset by peer" |

---

## 🛠️ Fix — How to Prevent This

### 1. **Ignore SIGPIPE in the server**
```c
#include <signal.h>

int main() {
    signal(SIGPIPE, SIG_IGN); // Prevent server crash on send()
    ...
}
```

### 2. **Check for send() errors**
```c
ssize_t sent = send(client_fd, buffer, len, 0);
if (sent == -1) {
    if (errno == EPIPE || errno == ECONNRESET) {
        // Client closed the connection
        printf("Client disconnected early\n");
    } else {
        perror("send failed");
    }
    close(client_fd);
}
```

---

## 🧠 TL;DR

> **SIGPIPE** is triggered when you write to a socket that the peer (client) has already closed.  
> If unhandled, it crashes the server. You must ignore it and check `send()` return values to avoid terminating the process.

## See more
- Connection reset by peer: See [troubleshooting/connection-reset-by-peer-kernel-app-level.md](troubleshooting/connection-reset-by-peer-kernel-app-level.md)