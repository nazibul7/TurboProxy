# Why Non-Blocking Sockets Busy Loop But Blocking Sockets Don't


## The Problem Discovery

### During development of an epoll-based server, this error appeared repeatedly in an infinite loop:

```c
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
[ERROR] accept failed: Resource temporarily unavailable
[ERROR] Failed to accept the client fd
```

## Whether accept() blocks or not depends on whether the listening socket fd is in blocking or non-blocking mode.

## The Question
Why does a `while(1)` loop with **non-blocking** sockets cause a busy loop, but the same `while(1)` loop with **blocking** sockets does not?

## Blocking Sockets Pattern

```c
while (1) {  // Infinite loop
    int client_fd = accept(server_fd, ...);  // BLOCKS HERE - thread pauses
    
    if (client_fd < 0) {
        log_error("Accept failed");
        continue;  // Back to while(1)
    }
    
    // Process connection
    handle_client(client_fd);
    close(client_fd);
    
    // Loop back to accept() - will BLOCK again
}
```

### Execution Timeline (Blocking):
```
Time 0: Thread blocks in accept() - WAITING
Time 5: Connection arrives
Time 5: accept() returns valid fd 
Time 6: Process connection
Time 7: Close connection
Time 8: Loop back to accept() - BLOCKS again - WAITING
Time 15: Next connection arrives...
```

**Result:** No busy loop. Thread spends 90%+ of time blocked/waiting in `accept()`.

## Non-Blocking Sockets Pattern (Incorrect Error Handling)

```c
while (1) {
    epoll_wait(...);  // Wait for "ready" events
    
    for (each ready event) {
        int client_fd = accept(server_fd, ...);  // Returns immediately
        
        if (client_fd < 0) {  // Gets EAGAIN - "would block"
            log_error("Accept failed: Resource temporarily unavailable");
            continue;  // Back to for loop - SAME epoll event still active
        }
        
        // Process connection
    }
}
```

### Execution Timeline (Non-Blocking with Bad Error Handling):
```
Time 0: epoll_wait() blocks - WAITING
Time 5: Epoll says "server socket ready"
Time 5: accept() returns EAGAIN immediately (no connection actually waiting)
Time 5: continue - back to for loop
Time 5: accept() returns EAGAIN immediately again
Time 5: continue - back to for loop
Time 5: accept() returns EAGAIN immediately again
... TIGHT BUSY LOOP - never returns to epoll_wait()
```

**Result:** Busy loop. Thread spends 100% of time spinning, never waiting.

## Non-Blocking Sockets Pattern (Correct Error Handling)

```c
int accept_client(int server_fd) {
    int client_fd = accept(server_fd, ...);
    
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // "No connection available" - not an error
        }
        log_error("Real accept error");
        return -1;  // Actual error
    }
    return client_fd;
}

while (1) {
    epoll_wait(...);  // Wait for ready events
    
    for (each ready event) {
        int client_fd = accept_client(server_fd);
        
        if (client_fd == 0) {
            continue;  // No connection - move to next epoll event
        }
        if (client_fd < 0) {
            continue;  // Real error - move to next epoll event
        }
        
        // Process connection (client_fd > 0)
    }
    // Exit for loop, return to epoll_wait() - BLOCKS again
}
```

### Execution Timeline (Non-Blocking with Correct Error Handling):
```
Time 0: epoll_wait() blocks - WAITING
Time 5: Epoll says "server socket ready" 
Time 5: accept_client() returns 0 (EAGAIN handled correctly)
Time 5: continue - move to next epoll event
Time 5: Exit for loop - back to epoll_wait() - BLOCKS again - WAITING
Time 10: Real connection arrives...
```

**Result:** No busy loop. Thread returns to blocking `epoll_wait()` between events.

## Key Insights

### Why Blocking Doesn't Busy Loop
- The `accept()` call itself provides the **waiting mechanism**
- Thread naturally pauses between loop iterations
- `while(1)` doesn't matter because execution time is spent blocked in `accept()`

### Why Non-Blocking Can Busy Loop
- No automatic waiting in `accept()` - returns immediately
- Must rely on `epoll_wait()` for the **waiting mechanism**
- Incorrect `EAGAIN` handling prevents return to `epoll_wait()`
- `while(1)` becomes problematic because there's no pause between iterations

### The Real Culprit
The busy loop isn't caused by:
- `while(1)` loops themselves
- Non-blocking I/O itself

The busy loop is caused by:
- **Non-blocking I/O + Incorrect error handling**
- Treating `EAGAIN` ("would block") as a retryable error instead of "no data available"

### Memory Aid
- **Blocking**: System call provides the pause
- **Non-blocking**: You must provide the pause (via correct error handling)