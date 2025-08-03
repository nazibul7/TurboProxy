<div align="center">

# 🚀 TurboProxy

*A blazing-fast, modular HTTP proxy server built in C*

![Build Status](https://img.shields.io/badge/build-passing-brightgreen?style=flat-square&logo=github-actions)
![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square&logo=opensourceinitiative)
![Version](https://img.shields.io/badge/version-v1.0-orange?style=flat-square&logo=git)
![Language](https://img.shields.io/badge/language-C-blue?style=flat-square&logo=c)
![Platform](https://img.shields.io/badge/platform-Linux-yellow?style=flat-square&logo=linux)

</div>

---

## 📖 About
TurboProxy is a high-performance HTTP proxy server written in C, showcasing the evolution of system-level network programming across multiple versions:

- 🔹 **v1.0** — Blocking, single-threaded proxy
- 🔹 **v2.0** — Threaded proxy with basic concurrency
- 🔹 **v3.0** — Epoll-based event-driven architecture
- 🔹 **v4.0** — Epoll + thread pool + keep-alive + LRU caching

This project is ideal for anyone exploring:
- Systems programming in C
- Efficient I/O with epoll
- Building high-performance backend infrastructure


---

## 📦 Versions

| Version               | Description                                                     |    Status   |
|-----------------------|-----------------------------------------------------------------|-------------| 
| `v1-single-threaded`  | Single-threaded, blocking I/O proxy server                      | ✅ Current  |
| `v2-epoll`            | Event-driven server using `epoll` (coming)                      | 📅 Planned  |
| `v3-threaded`         | Thread pool with concurrent request handling (planned)          | 📅 Planned  |
| `v4-advanced`         | Full performance proxy: epoll + threads + LRU caching (planned) | 📅 Planned  |
---
## 🔧 Features (v1-single-threaded) ✅ *Current*

- 🌐 HTTP 1.0/1.1 request forwarding
- 🛣️ Basic routing to destination server  
- 📁 Clean, versioned architecture
- ⚙️ Easily extensible design
- 📊 Request/response logging
- 🎛️ Basic configuration system
- 📈 Benchmark automation
---

## 🛠️ Build & Run

### 🔨 Compile

```bash
make VERSION=v1-single-threaded
```

This creates the binary at:

```bash
./bin/v1-single-threaded-server
```

### 🚀 Run

```bash
./bin/v1-single-threaded-server
```

(Listens on port 8000 by default — configurable in main.c)

---

## 🧱 Project Structure

```
TurboProxy/
├── include/                  # Shared headers
├── src/
│   ├── v1-single-threaded/   # ✅ Current: Blocking version (single-threaded)
│   ├── v2-epoll/             # 📅 Future: epoll version
│   └── v3-threaded/          # 📅 Future: thread pool version
│   └── v4-advanced/          # 📅 Future: full performance version
├── docs/                     # Design docs and architecture notes
│   ├── v1-single-threaded/      # ✅ Current version docs
│   ├── v2-epoll/                # 📅 Future docs
│   ├── v3-threaded/             # 📅 Future docs
│   └── v4-advanced/             # 📅 Future docs
├── benchmarks/               # Performance test results
├── build/                    # Object files (auto-generated)
├── bin/                      # Binaries (auto-generated)
├── scripts/benchmark.sh      # Script to run benchmarks
├── Makefile
└── README.md
```
---
### **Current Architecture (v1.0)**
```
Client → TurboProxy (Single Thread) → Backend Server
         ↓
    [HTTP Parser] → [Connection Handler] → [Response Processor]
```

### **Target Architecture (v4.0)**
```
                                                      ┌─ [Worker Thread 1 (epoll)] ─┐
[Client] → [ Load Balancer] →  [Main Thread (epoll)] ─┼─ [Worker Thread 2 (epoll)] ─┼→ [Backend Server]
                                        ↓             └─ [Worker Thread N (epoll)] ─┘
                                   [LRU Cache]
```
---

## 🔭 Roadmap

### **v1.0 - Foundation** ✅ *Current*
[current completed features]
### **v2.0 - Concurrency** 🔄 *In Progress*
- [ ] Thread pool implementation
- [ ] Connection pooling
- [ ] Thread-safe logging
- [ ] Performance improvements

### **v3.0 - Event-Driven** 📅(*Planned*)
- [ ] Epoll-based I/O
- [ ] Asynchronous request processing
- [ ] Zero-copy networking

### **v4.0 - Intelligence** 📅(*Planned*)
- [ ] LRU response caching
- [ ] Smart connection management
- [ ] Load balancing algorithms
- [ ] Graceful shutdown and connection pooling
- [ ] Auto-scaling capabilities
- [ ] Advanced monitoring

---

## 🧪 Benchmarking

Benchmarks are collected using:

- [`wrk`](https://github.com/wg/wrk)
- [`ab` (ApacheBench)](https://httpd.apache.org/docs/2.4/programs/ab.html)
- [`siege`](https://www.joedog.org/siege-home/)

To run all benchmarks and save results to versioned folders:

```bash
VERSION=v1-single-threaded ./scripts/benchmark.sh
```

Benchmark results will be saved in:

```
benchmarks/
└── v1-single-threaded/
    ├── wrk.txt
    ├── ab.txt
    └── siege.txt
```

Repeat with other versions by changing VERSION=... (e.g., v2-epoll, v3-threaded, etc.).

---
## 📄 Docs

See docs/v1-single-threaded/ for design decisions of version 1.

Future versions will live under:

- docs/v2-epoll/
- docs/v3-threaded/
- docs/v4-lru/
  
---

## 🧑‍💻 Author

Nazibul — GitHub | LinkedIn

---

## 📜 License

MIT License — See LICENSE file

---


