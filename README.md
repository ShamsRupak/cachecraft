# CacheCraft

![CI](https://github.com/ShamsRupak/cachecraft/actions/workflows/ci.yml/badge.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

**A high-performance, in-memory cache server built with modern C++ (C++20)**

CacheCraft is a Redis-inspired cache server that demonstrates production-grade systems programming: custom protocol design, O(1) LRU eviction, sharded concurrent data structures, async I/O, TTL expiration, and comprehensive testing with sanitizer support.

## Skills Showcased

| Area | Details |
|------|---------|
| **Data Structures** | Hash map + doubly-linked list for O(1) LRU eviction |
| **Concurrency** | Lock-striped sharding with `shared_mutex` for read-heavy workloads |
| **Networking** | Async TCP server using standalone Asio with thread pool |
| **Memory Safety** | ASAN/UBSAN build presets, defensive parsing, graceful disconnect handling |
| **Testing** | Unit tests (Catch2), integration tests with ephemeral ports, concurrent stress tests |
| **Performance** | Custom benchmark tool reporting ops/sec, p50/p95/p99 latencies |
| **DevOps** | CMake build system, GitHub Actions CI (Ubuntu + macOS), clang-format |

## Architecture

```
                    ┌─────────────┐
                    │  TCP Client  │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Asio Async  │  Thread pool (N threads)
                    │  Acceptor   │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │   Session   │  Per-connection async read/write
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  Protocol   │  Parse command, validate args
                    │   Parser    │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │  Command    │  Route to store operations
                    │  Handler    │
                    └──────┬──────┘
                           │
              ┌────────────▼────────────┐
              │    Sharded Cache Store   │
              │                          │
    ┌─────────┼─────────┬───────────┐   │
    │ Shard 0 │ Shard 1 │ ... │ N-1 │   │
    │ RW Lock │ RW Lock │     │     │   │
    │ LRU List│ LRU List│     │     │   │
    │ HashMap │ HashMap │     │     │   │
    └─────────┴─────────┴─────┴─────┘   │
              │                          │
              │  Background Sweeper ─────┤  Periodic TTL cleanup
              └──────────────────────────┘
```

## Build Instructions

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, Apple Clang 15+)
- CMake 3.16+
- Standalone Asio (`brew install asio` or `apt install libasio-dev`)
- Catch2 v3 (`brew install catch2` — or auto-fetched by CMake)

### Build

```bash
# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Debug build with sanitizers
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build-debug -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

### Run

```bash
# Start the server (default: port 9090, 16 shards)
./build/cachecraftd

# With custom options
./build/cachecraftd --port 6379 --threads 8 --shards 32 --max-entries 500000

# Connect with the CLI client
./build/cachecraft-cli
./build/cachecraft-cli --port 6379
```

### Test

```bash
ctest --test-dir build --output-on-failure
```

### Benchmark

```bash
# Start the server first, then:
./build/cachecraft-bench --clients 8 --requests 100000 --keyspace 10000
```

## Command Protocol (RESP-lite)

Commands are newline-delimited text strings sent over TCP.

```
SET <key> <value> [ttl_ms]   → OK
GET <key>                     → VALUE <value> | NIL
DEL <key>                     → OK | NIL
EXISTS <key>                  → VALUE 1 | VALUE 0
INCR <key>                    → VALUE <new_value> | ERR ...
MGET <k1> <k2> ...           → VALUES <v1|NIL> <v2|NIL> ...
KEYS <prefix*>               → VALUES <k1> <k2> ...
STATS                         → STATS key1=val1 key2=val2 ...
PING                          → PONG
```

### Example Session

```
cachecraft> PING
PONG
cachecraft> SET user:1 alice
OK
cachecraft> SET session:abc token123 60000
OK
cachecraft> GET user:1
VALUE alice
cachecraft> MGET user:1 user:2
VALUES alice NIL
cachecraft> INCR counter
VALUE 1
cachecraft> STATS
STATS keys=3 bytes=... hits=2 misses=0 evictions=0 expired=0 ...
```

## Design Decisions & Tradeoffs

### Sharded Store with Lock Striping

Instead of a single global mutex, the store is split into N shards (default 16). Each shard has its own `std::shared_mutex`. Keys are routed to shards via `std::hash<string>(key) % N`.

**Why:** Read-heavy workloads benefit from multiple concurrent readers per shard. Lock striping reduces contention compared to a single global lock, at the cost of slightly more complex code and per-shard overhead.

### O(1) LRU Eviction

Each shard maintains a doubly-linked list (most-recent at front) plus a hash map from key → list iterator. `set/get` promotes entries to the front in O(1) via `std::list::splice`. Eviction removes from the back.

**Tradeoff:** Per-shard LRU means eviction is local — a globally "cold" key in a hot shard may survive while a "warmer" key in a full shard is evicted. A global LRU would be more precise but would require cross-shard coordination, hurting concurrency.

### TTL: Lazy + Background Sweep

- **Lazy expiration:** Every `get/exists` checks the entry's expiry and removes it if stale. This is O(1) and prevents serving stale data.
- **Background sweeper:** A dedicated thread periodically scans all shards and removes expired keys. This bounds memory usage even if expired keys are never accessed.

**Tradeoff:** The sweeper acquires a write lock on each shard during cleanup, which may briefly block writers. The sweep interval (default 1s) balances memory reclamation vs lock contention.

### Async I/O with Asio

The server uses Asio's async TCP operations with a thread pool. Each client connection is handled by a `Session` that reads one line, processes the command, writes the response, then loops.

**Why async:** Async I/O scales to many concurrent connections without spawning a thread per client. The thread pool processes ready handlers across all sessions.

## Roadmap

- [ ] Pipelining support (batch multiple commands per read)
- [ ] Persistence (AOF / snapshot to disk)
- [ ] Pub/sub channels
- [ ] Cluster mode with consistent hashing
- [ ] TLS support
- [ ] Lua scripting
- [ ] Memory-efficient value encoding (small strings, integers)

## Project Structure

```
cachecraft/
├── CMakeLists.txt              # Build system
├── .clang-format               # Code style
├── .github/workflows/ci.yml    # CI pipeline
├── src/
│   ├── common/
│   │   ├── types.h             # CacheEntry, StoreStats
│   │   ├── protocol.h          # Command parsing + response formatting
│   │   └── protocol.cpp
│   ├── store/
│   │   ├── lru_shard.h         # Per-shard LRU cache (thread-safe)
│   │   ├── lru_shard.cpp
│   │   ├── cache_store.h       # Sharded store + sweeper
│   │   └── cache_store.cpp
│   ├── server/
│   │   ├── server.h            # Async TCP acceptor
│   │   ├── server.cpp
│   │   ├── session.h           # Per-client session
│   │   ├── session.cpp
│   │   └── main.cpp            # cachecraftd entry point
│   ├── client/
│   │   └── main.cpp            # cachecraft-cli entry point
│   └── bench/
│       └── main.cpp            # cachecraft-bench load tester
├── tests/
│   ├── test_protocol.cpp       # Parser unit tests
│   ├── test_lru.cpp            # LRU eviction tests
│   ├── test_store.cpp          # Store + concurrency tests
│   ├── test_ttl.cpp            # TTL expiration tests
│   └── test_integration.cpp    # End-to-end TCP tests
├── docs/
│   ├── ARCHITECTURE.md
│   └── BENCHMARKS.md
└── scripts/
    ├── build.sh
    └── run-tests.sh
```

## License

MIT
