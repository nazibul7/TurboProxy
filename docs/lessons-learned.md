# Lessons Learned from v1: Blocking Proxy

## ✅ What Worked

- Basic proxy logic and parsing validated
- Easy to debug with tools like `strace`, `valgrind`
- Code is modular and prepares for future extensions

## ❌ What Didn't Scale

- One connection at a time = poor concurrency
- Clients dropped when backlog fills up
- High tail latency under load

## 🛠 Useful Tools

- `ab` and `wrk` for load testing
- `valgrind` for memory leak detection
- `netstat`/`ss` for socket state monitoring

## 💡 Takeaways

- Blocking model is insufficient for real-world loads
- Need to shift to `epoll` + thread pool for scalability
- Connection reuse (keep-alive) will need careful socket state tracking
