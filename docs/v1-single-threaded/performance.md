# Performance (v1): Blocking Proxy Server

## Benchmark Setup

- **Tool**: Apache Bench (`ab`) and `wrk`
- **Command**: 
  - `ab -n 1000 -c 10 http://localhost:8000/`
  - `wrk -t4 -c50 -d10s http://localhost:8000/`
- **Backend**: Returns 31KB HTTP responses
- **Environment**: Localhost (no real network latency)
- **Concurrency**: 10–50 clients
- **Duration**: 10–33 seconds

## Observations

- Only **one request** is processed at a time
- All other connections wait in the **backlog queue**
- High **latency** under concurrency
- Poor **throughput**
- **No crashes** or memory leaks (confirmed via `valgrind`)
- Server responds with `Connection: close` indicating **no Keep-Alive support**

## Resource Usage

- **Low CPU usage** due to blocking I/O
- **Minimal memory usage**
- No **parallelism**, so **doesn’t scale** under load

## Bottlenecks

- `accept()` blocks new client connections
- `recv()` / `send()` block the **entire thread**
- Kernel **backlog queue** can overflow under high load

---

## Benchmark Results

### ApacheBench (`ab`)

| Metric              | Run 1     | Run 2     | Average   |
|---------------------|-----------|-----------|-----------|
| Requests/sec        | 29.80     | 31.18     | 30.49     |
| Mean Latency        | 1,677 ms  | 1,603 ms  | 1,640 ms  |
| Test Duration       | 33.56 s   | 32.07 s   | 32.82 s   |
| Failed Requests     | 0         | 5         | 0.25%     |
| Success Rate        | 100%      | 99.5%     | 99.75%    |

### wrk

- **Command**: `wrk -t4 -c50 -d10s http://localhost:8000/`
- **Requests/sec**: 31.72
- **Average latency**: 1.39 s
- **Total requests**: 318 in 10s
- **Timeout errors**: 8
- **Success rate**: 97.5%

---

## Summary

The v1 blocking proxy demonstrates stable but limited performance typical of single-threaded synchronous I/O systems. While reliability is high (up to **99.75%** success), both **throughput** and **latency** are constrained by the sequential nature of the architecture.

This phase establishes a correct and clean foundation to build on. The lack of concurrency highlights the need for improvements in **Phase 2**, where we’ll add **Keep-Alive**, **epoll**, and a **thread pool** for scalable performance.

📁 See full logs in [`benchmarks/`](../../benchmarks/v1-single-threaded)