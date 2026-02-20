#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "common/types.h"
#include "store/lru_shard.h"

namespace cachecraft {

struct StoreConfig {
    size_t num_shards = 16;
    size_t max_entries_per_shard = 10000;
    size_t max_bytes_per_shard = 64 * 1024 * 1024;  // 64 MB per shard
    std::chrono::seconds sweep_interval{1};
};

class CacheStore {
public:
    explicit CacheStore(const StoreConfig& config = {});
    ~CacheStore();

    CacheStore(const CacheStore&) = delete;
    CacheStore& operator=(const CacheStore&) = delete;

    bool set(const std::string& key, const std::string& value,
             std::optional<std::chrono::milliseconds> ttl = std::nullopt);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    std::optional<int64_t> incr(const std::string& key);
    std::vector<std::optional<std::string>> mget(const std::vector<std::string>& keys);
    std::vector<std::string> keys(const std::string& pattern);
    StoreStats stats() const;

    void start_sweeper();
    void stop_sweeper();

private:
    LRUShard& shard_for(const std::string& key);
    const LRUShard& shard_for(const std::string& key) const;
    [[nodiscard]] size_t shard_index(const std::string& key) const;

    void sweeper_loop();

    StoreConfig config_;
    std::vector<std::unique_ptr<LRUShard>> shards_;

    std::atomic<bool> sweeper_running_{false};
    std::thread sweeper_thread_;
};

}  // namespace cachecraft
