# Proxy Connection Behavior: What Was Wrong


## 🔍 How I found the problem

- First request (like `/auth/login`) worked.
- Second request (like `/auth/register`) failed or hung.
- Browser was reusing TCP socket due to `Connection: keep-alive`.
- Proxy closed both client & backend sockets after one request.
- Verified with:
    # 1️⃣ Single request works:
    curl -v -H "Connection: keep-alive" http://localhost:8000/auth/login

    # 2️⃣ Reuse TCP for second request — fails:
    curl -v -H "Connection: keep-alive" http://localhost:8000/first-path http://localhost:7000/second-path

    * Re-using existing connection!
    * Recv failure: Connection reset by peer


    # 3️⃣ Check socket states:
    netstat -tn | grep :8000


## 📌 Situation

1. Browser sends requests with `Connection: keep-alive` (HTTP/1.1 default).
2. Proxy forwards same header to backend.
3. Backend replies with `Connection: keep-alive` and `Keep-Alive: timeout=5`.
4. Proxy relays backend’s response **without changing headers**.
5. Browser sees `Connection: keep-alive` → tries to reuse TCP.
6. Proxy immediately `close(client_fd)` and `close(target_fd)` after relaying.

✅ The backend expects to keep its TCP socket open for more requests.
✅ The browser expects to keep using its TCP socket for more requests.
❌ The proxy force-closes both → causing hangs, resets, or wasted resources.

---

## ✅ The Correct Behavior

- Proxy must inject or force `Connection: close` when forwarding request to backend.
- Proxy must or replace original `Connection` headers from browser.
- This tells the backend & browser to close TCP cleanly after responding.

---

## ✅ Future TODO

- For real HTTP/1.1 keep-alive, the proxy must:
  - Track multiple requests per TCP socket.
  - Correctly parse pipelined requests.
  - Keep backend sockets open and reuse them.
  - Implement connection pooling.
