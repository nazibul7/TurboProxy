# Epoll-based HTTP Proxy: Common Errors and Solutions

This document covers the common errors encountered while building an epoll-based HTTP proxy server and their solutions.

## Error 1: "getsockopt failed: Bad file descriptor"

### Problem Description
```
[ERROR] getsockopt failed: Bad file descriptor
```

### Root Cause
- **Variable Scope Issue**: Variables like `targetfd`, `backend`, `request` were declared inside the event loop
- **Lost Context**: When EPOLLIN event processes a client request, variables are set. But when EPOLLOUT event fires later, these variables are reset to initial values because they're redeclared in each loop iteration
- **Invalid File Descriptor**: `targetfd` becomes -1 (initial value) when `getsockopt(targetfd, ...)` is called

### Code Problem
```c
for (int i = 0; i < nfds; i++) {
    // ❌ Variables declared here are reset on each iteration
    int targetfd = -1;
    Route *backend = NULL;
    char request[MAX_REQUEST_SIZE];
    // ...
}
```

### Solution
Move variable declarations outside the event loop:

```c
// ✅ Variables persist across epoll events
int targetfd = -1;
Route *backend = NULL;
char request[MAX_REQUEST_SIZE];

while (1) {
    int nfds = epoll_server_wait(epoll_fd, events, -1);
    for (int i = 0; i < nfds; i++) {
        // Variables maintain their values here
    }
}
```

## Error 2: Backend Response Treated as Client Request

### Problem Description
```
Received request:
HTTP/1.1 200 OK
set-cookie: authjs.csrf-token=...
[ERROR] find_backend: Path must start with '/' (path='200')
[ERROR] No backend found for path: 200
```

### Root Cause
- **Event Misclassification**: Backend response (EPOLLIN on backend fd) was being handled as a new client request
- **Wrong Event Handling**: All EPOLLIN events were treated as client requests regardless of which file descriptor triggered them
- **Flow Control Issue**: No distinction between client fd and backend fd in event handling

### Code Problem
```c
if (events[i].events & EPOLLIN) {
    // ❌ This treats ALL EPOLLIN events as client requests
    // Including responses from backend servers
    ssize_t bytes_read = recv_from_client(current_fd, buffer, BUFFER_SIZE - 1);
    if (parse_http_request(buffer, &req) != 0) {
        // This tries to parse "HTTP/1.1 200 OK" as a request!
    }
}
```

### Solution
Properly distinguish between different file descriptors:

```c
if (current_fd == server_fd) {
    // New client connection
} else if (current_fd == targetfd) {
    // Backend connection event
    if (events[i].events & EPOLLOUT) {
        // Send request to backend
    } else if (events[i].events & EPOLLIN) {
        // Receive response from backend
    }
} else {
    // Client connection event
    if (events[i].events & EPOLLIN) {
        // New request from client
    }
}
```

## Error 3: Resource Leaks and Connection State Issues

### Problem Description
- File descriptor leaks
- Backend connections not properly closed
- Client connections hanging
- Stale connection state affecting subsequent requests

### Root Cause
- **Inadequate Cleanup**: Resources not properly cleaned up after each request
- **Missing Client Tracking**: No way to associate backend responses with the correct client
- **Connection Reuse Issues**: Not properly resetting connection state

### Code Problem
```c
// ❌ No tracking of which client belongs to which backend connection
int targetfd = connect_to_target_nb(backend->host, backend->port);
// Later, when backend responds, we don't know which client to send it to
```

### Solution
Track client-backend relationships:

```c
// ✅ Track which client we're serving
int client_fd_for_response = current_fd;

// ✅ Proper cleanup after each request-response cycle
if (targetfd != -1) {
    close(targetfd);
    targetfd = -1;
}
if (client_fd_for_response != -1) {
    close(client_fd_for_response);
    client_fd_for_response = -1;
}
```

## Error 4: Race Conditions in Multi-Client Scenarios

### Problem Description
- Multiple clients overwriting shared variables
- Client A's request being processed with Client B's data
- Unpredictable behavior under load

### Root Cause
- **Shared Global State**: Using single global variables for multiple concurrent connections
- **No Connection Isolation**: Each client connection needs its own state

### Current Limitation
```c
// ❌ Global variables shared by all clients
char request[MAX_REQUEST_SIZE];
Route *backend = NULL;
HttpRequest req;
```

### Architectural Solutions

#### Connection State Structure
```c
typedef struct {
    int client_fd;
    int backend_fd;
    Route *backend;
    char request_buffer[MAX_REQUEST_SIZE];
    HttpRequest req;
    bool req_parsed;
    enum connection_state state;
} connection_state_t;

// Hash map or array to track multiple connections
connection_state_t connections[MAX_CONNECTIONS];
```

#### Buffer management structure

```c
#define SMALL_BUFFER_SIZE 8192
#define LARGE_BUFFER_THRESHOLD (64 * 1024)

typedef struct buffer {
    char *data;
    size_t size;
    size_t used;
    size_t processed;
    bool is_dynamic;    // true if heap allocated
    char small_buf[SMALL_BUFFER_SIZE]; // embedded for small messages
} buffer_t;
```


## Performance Considerations

1. **Buffer Sizes**: Use appropriate buffer sizes (16KB is usually good)
2. **Connection Limits**: Implement maximum connection limits
3. **Timeout Management**: Add timeouts for backend connections
4. **Keep-Alive**: Handle connection keep-alive properly
5. **Error Recovery**: Implement robust error recovery

## Testing Strategies

### Concurrent Testing
```bash
# Test multiple concurrent requests
for i in {1..10}; do
    curl http://localhost:8000/ &
done
wait
```

## Common Pitfalls to Avoid

1. **Don't ignore SIGPIPE**: Always set `signal(SIGPIPE, SIG_IGN)`
2. **Don't block on I/O**: Ensure all sockets are non-blocking
3. **Don't forget error checking**: Check return values of all system calls
4. **Don't mix up file descriptors**: Always validate which fd you're working with
5. **Don't forget cleanup**: Always clean up resources, even on error paths
6. **Don't assume complete reads/writes**: Handle partial I/O operations
7. **Don't ignore connection state**: Track the state of each connection properly

This guide should help you understand and avoid the common pitfalls when building epoll-based network servers.