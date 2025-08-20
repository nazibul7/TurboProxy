# Epoll Documentation - Raw Format

## fcntl() Function

```c
/**
 * fcntl() → Get or set file descriptor flags or other properties.
 *
 * int fcntl(int fd, int cmd, ... /* arg */);
 *
 * In this context, we use it to make a socket non-blocking.
 *
 * Parameters:
 * - fd → The file descriptor you want to modify (socket, file, pipe, etc.).
 * - cmd → What action to perform:
 *   - F_GETFL → Get the current file status flags (e.g., O_NONBLOCK, O_APPEND).
 *   - F_SETFL → Set new file status flags.
 * - arg → (Only used with F_SETFL) The new flags you want to set.
 */
```

## epoll_event Structure

```c
/**
 * struct epoll_event event is just a C struct that you fill out to tell the epoll system call:
 *
 *   #include <sys/epoll.h>
 *
 *   struct epoll_event {
 *       uint32_t events;    // Epoll events (bitmask)
 *       epoll_data_t data;  // Arbitrary user data
 *   };
 *
 *   typedef union epoll_data {
 *       void     *ptr;      // Pointer to user-defined data
 *       int       fd;       // File descriptor
 *       uint32_t  u32;      // 32-bit integer
 *       uint64_t  u64;      // 64-bit integer
 *   } epoll_data_t;
 *
 *  - Which events you want to monitor (event.events)
 *  - Which user data should be returned to you when those events happen (event.data)
 *
 * event.data.fd means-
 *      "Hey, watch my server_id socket, and let me know when there's something to read
 *       — in this case, that means an incoming connection request."
 */
```

## EPOLLIN - "Ready to Read"

```
==============================================
🔹 EPOLLIN — "Ready to Read"
----------------------------------------------
📌 What it is:
    - Event flag for epoll that tells you:
      "This file descriptor (socket) has data
       you can read *without blocking*."

📌 When it triggers:
    - New incoming data is available to read.
    - For a listening socket: a new connection
      is waiting to be accepted.
    - For a regular socket: the peer sent some
      data or closed the connection (read returns 0).

📌 Where to use:
    - In your epoll_event struct:
        event.events = EPOLLIN;
    - When monitoring sockets for *incoming* data.

📌 Advantages:
    - You only call recv()/accept() when it's safe
      (no blocking, no busy-wait).
    - Efficient for handling many connections at once.

📌 Disadvantages:
    - If you forget to read all available data,
      epoll will keep triggering again (spinning).
    - You must still check for EOF (0 bytes read).
==============================================
```

## EPOLLOUT - "Ready to Write"

```
==============================================
🔹 EPOLLOUT — "Ready to Write"
----------------------------------------------
📌 What it is:
    - Event flag for epoll that tells you:
      "This file descriptor (socket) can accept data
       to be sent *without blocking*."

📌 When it triggers:
    - The send buffer has space for you to write more.
    - Common after:
        1. A connection is first established.
        2. Your socket was previously full (EAGAIN)
           and now has room.
    - For non-blocking sockets, you'll get this a lot
      unless you disable it when you don't need to send.

📌 Where to use:
    - In your epoll_event struct:
        event.events = EPOLLOUT;
    - Useful if send() previously returned EAGAIN/EWOULDBLOCK.

📌 Advantages:
    - Prevents blocking on send().
    - Lets you queue data and send only when possible.

📌 Disadvantages:
    - If always enabled, it will keep triggering even
      when you have nothing to send (wastes CPU).
    - Best practice: enable EPOLLOUT *only when needed*
      and disable it after sending.
==============================================
```

## Best Practice for Non-blocking IO

```
==============================================
💡 Best Practice for Non-blocking IO:
    - EPOLLIN → Read until recv() returns EAGAIN or 0.
    - EPOLLOUT → Write until send() returns EAGAIN,
      then disable EPOLLOUT until you have more data.
==============================================
```

## EDGE-TRIGGERED (EPOLLET) vs LEVEL-TRIGGERED

```
======================================
🔹 EDGE-TRIGGERED (EPOLLET) vs LEVEL-TRIGGERED
======================================

# 1️⃣ Level-Triggered (LT) — DEFAULT
   - Works like a **reminder alarm** that keeps ringing until you deal with it.
   - If `EPOLLIN` (readable) is true, epoll will keep telling you
     EVERY TIME you call `epoll_wait()` — until you read ALL the data from the socket.
   - Same for `EPOLLOUT` — keeps firing until you've sent everything or the buffer is full.
   - Good for simple blocking-like loops, harder to forget data.
   - But — can lead to **epoll_wait() wakes too often** if you don't drain the socket fully.

# 2️⃣ Edge-Triggered (ET)
   - Works like a **doorbell** — rings ONLY once when the state changes
     from "no data" → "data available" (EPOLLIN) or
     from "can't send" → "can send" (EPOLLOUT).
   - After that, epoll will stay silent until the next state change.
   - This means: YOU must read/write **until EAGAIN** to avoid losing data/events.
   - Faster (less syscalls) but trickier — if you forget to read all data,
     you won't get another EPOLLIN until new data arrives.

# 3️⃣ How ET/LT Relates to EPOLLIN/EPOLLOUT:
   - `EPOLLIN` (read event):
       LT → fires until socket read buffer is empty.
       ET → fires only when socket goes from empty → has data.
   - `EPOLLOUT` (write event):
       LT → fires until socket send buffer is full.
       ET → fires only when socket goes from full → has free space.

# 4️⃣ Who Triggers?
   - The **kernel** triggers EPOLLIN/EPOLLOUT events based on socket state changes.
   - ET just changes how often it tells you (once per change).
   - LT keeps telling you every time until condition is gone.

# 5️⃣ When to Use:
   - LT: Safer for beginners, easier logic, but may wake CPU more often.
   - ET: Best for high-performance servers (Nginx style)
         but requires careful EAGAIN handling.

# 6️⃣ EAGAIN with ET:
   - Always read/write in a loop until:
       recv()/read() → returns -1 && errno == EAGAIN
       send()/write() → returns -1 && errno == EAGAIN
   - This ensures you completely drain/fill the buffer before waiting again.

# 7️⃣ Advantages / Disadvantages:

   LT:
     ✅ Easier to code
     ✅ No missed data even if you forget EAGAIN loop
     ❌ More wakeups → slightly slower

   ET:
     ✅ Fewer syscalls → faster for high load
     ❌ Easy to miss data if not careful
     ❌ Must always use non-blocking sockets + EAGAIN loop

# Example epoll_ctl:
   Level-triggered (default):
       event.events = EPOLLIN | EPOLLOUT;
   Edge-triggered:
       event.events = EPOLLIN | EPOLLOUT | EPOLLET;
```