# Architecture

## Request Flow

```
Client TCP Connection
        │
        ▼
  ┌───────────┐
  │  Acceptor  │  asio::ip::tcp::acceptor (async)
  └─────┬─────┘
        │ async_accept → creates Session
        ▼
  ┌───────────┐
  │  Session   │  Owns socket + streambuf, one per client
  └─────┬─────┘
        │ async_read_until('\n') → read one line
        ▼
  ┌───────────┐
  │  Protocol  │  Tokenize line, identify command type
  │   Parser   │  Validate argument count
  └─────┬─────┘
        │ returns Command { type, args }
        ▼
  ┌───────────┐
  │  Command   │  Switch on CommandType
  │  Handler   │  Call appropriate CacheStore method
  └─────┬─────┘
        │ store.get(key) / store.set(key, val, ttl) / ...
        ▼
  ┌───────────┐
  │  Cache     │  Hash key → shard index
  │  Store     │  Delegate to LRUShard
  └─────┬─────┘
        │
        ▼
  ┌───────────┐
  │  LRUShard  │  Acquire shared_mutex (read or write)
  │            │  Hash map lookup + LRU list manipulation
  └─────┬─────┘
        │ returns result
        ▼
  Response formatted by Protocol → async_write back to client
```

## Data Structures

### LRUShard

Each shard contains:

- **`std::unordered_map<string, list::iterator>`** — O(1) key lookup to find the entry's position in the LRU list.
- **`std::list<CacheEntry>`** — Doubly-linked list ordered by recency. The front is the most recently used; the back is the least recently used.

Operations:
- **GET**: Map lookup → check expiry → promote to front → return value. O(1).
- **SET**: Map lookup → update or insert at front → evict from back if over capacity. O(1).
- **DEL**: Map lookup → erase from list and map. O(1).
- **Eviction**: Pop from back of list, remove from map. O(1).

### CacheEntry

```cpp
struct CacheEntry {
    std::string key;
    std::string value;
    std::optional<TimePoint> expiry;  // nullopt = no TTL
};
```

Memory usage is tracked per-entry as `key.capacity() + value.capacity() + sizeof(CacheEntry)`.

### StoreStats

Aggregated across all shards: total keys, bytes, hits, misses, evictions, expired count.

## Concurrency Model

### Sharding

The store contains N shards (configurable, default 16). A key's shard is determined by:

```
shard_index = std::hash<string>{}(key) % num_shards
```

This distributes keys across shards, reducing lock contention.

### Locking Strategy

Each shard has a `std::shared_mutex`:
- **Read operations** (`get`, `exists`, `keys_with_prefix`, `size`, `bytes_used`): Acquire `shared_lock` — multiple readers can proceed concurrently.
- **Write operations** (`set`, `del`, `incr`, `sweep_expired`): Acquire `unique_lock` — exclusive access.

**Note:** `get` actually needs `unique_lock` because it promotes the entry in the LRU list (a write to the list). This is a deliberate trade-off: LRU accuracy at the cost of serializing reads within a shard. The sharding compensates by allowing reads across different shards to proceed in parallel.

### Thread Pool

The server runs Asio's `io_context::run()` on N threads (default: `hardware_concurrency`). Asio dispatches ready completion handlers to available threads. No thread affinity — any thread may handle any client's next operation.

### Background Sweeper

A dedicated thread periodically (every 1 second) iterates all shards, acquiring a write lock on each to remove expired entries. The sweep is interruptible: it checks a `std::atomic<bool>` flag between shards.

### Atomic Counters

Hit/miss/eviction/expired counters use `std::atomic<uint64_t>` with relaxed ordering. These are updated inside the lock but read outside it (for stats). Relaxed ordering is acceptable since exact consistency of counters is not required.

## Network Layer

- **Transport:** TCP, using standalone Asio's async API.
- **Protocol:** One command per line (`\n`-delimited). Maximum line length: 64 KB.
- **Session lifecycle:** Accept → read line → parse → handle → write response → loop. Client disconnect (EOF or error) terminates the session.
- **Graceful shutdown:** `SIGINT`/`SIGTERM` stop the `io_context`, draining in-flight handlers before exit.
