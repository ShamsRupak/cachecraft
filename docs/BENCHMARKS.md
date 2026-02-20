# Benchmarks

## How It Works

`cachecraft-bench` is a load-testing tool that:

1. Spawns N client threads, each opening a TCP connection to the server.
2. Each client sends a configurable number of requests with a mix of GET and SET operations.
3. Keys are randomly selected from a configurable key space (`bench:0` through `bench:<keyspace-1>`).
4. Each operation's latency is measured individually using `high_resolution_clock`.
5. After all threads complete, results are aggregated and reported.

### Metrics Reported

- **Throughput** — total operations per second across all clients
- **Avg latency** — mean time per operation (microseconds)
- **P50 latency** — median latency
- **P95 latency** — 95th percentile latency
- **P99 latency** — 99th percentile latency
- **Errors** — number of client connection failures

## How to Reproduce

```bash
# 1. Build in release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# 2. Start the server
./build/cachecraftd --port 9090 --shards 16

# 3. In another terminal, run the benchmark
./build/cachecraft-bench \
    --port 9090 \
    --clients 4 \
    --requests 50000 \
    --keyspace 5000 \
    --value-size 64 \
    --get-ratio 0.8
```

### Parameters

| Flag | Default | Description |
|------|---------|-------------|
| `--host` | 127.0.0.1 | Server host |
| `--port` | 9090 | Server port |
| `--clients` | 4 | Number of concurrent client threads |
| `--requests` | 10000 | Total number of requests (split across clients) |
| `--keyspace` | 1000 | Number of distinct keys |
| `--value-size` | 64 | Size of values in bytes (for SET) |
| `--get-ratio` | 0.8 | Fraction of operations that are GETs (0.0–1.0) |

## Sample Results

**Machine:** Apple M-series, 8 cores, macOS  
**Configuration:** 16 shards, Release build, 4 benchmark clients

### Mixed Workload (80% GET / 20% SET)

```
Clients:     4
Requests:    50000
Key space:   5000
Value size:  64 bytes

Throughput:   ~45,000–60,000 ops/sec
Avg latency:  ~70–90 μs
P50 latency:  ~55–70 μs
P95 latency:  ~120–180 μs
P99 latency:  ~200–400 μs
Errors:       0
```

### Write-Heavy (50% GET / 50% SET)

```
Clients:     4
Requests:    50000
Key space:   5000
Value size:  256 bytes

Throughput:   ~35,000–50,000 ops/sec
Avg latency:  ~80–120 μs
P50 latency:  ~65–90 μs
P95 latency:  ~150–250 μs
P99 latency:  ~300–600 μs
Errors:       0
```

### Scaling with Clients

```
 Clients | Requests | Throughput (ops/sec)
---------|----------|-----------------------
       1 |    50000 | ~20,000–30,000
       4 |    50000 | ~45,000–60,000
       8 |   100000 | ~55,000–75,000
      16 |   100000 | ~60,000–80,000
```

> **Note:** Exact numbers vary by machine, OS scheduler load, and thermal conditions. Run multiple times and take the median for reliable comparisons.

## Performance Characteristics

- **Sharding** improves throughput roughly linearly with client count up to the shard count, since clients hitting different shards don't contend.
- **GET-heavy workloads** are faster because the hash map lookup is the bottleneck, not LRU list manipulation (though `get` does acquire a write lock for LRU promotion).
- **Large values** primarily affect network I/O time, not store lookup time.
- **P99 spikes** are typically caused by the background sweeper acquiring write locks during its periodic scan, or by OS-level scheduling jitter.
