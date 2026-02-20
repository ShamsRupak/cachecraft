#include "store/cache_store.h"

#include <algorithm>
#include <functional>

namespace cachecraft {

CacheStore::CacheStore(const StoreConfig& config) : config_(config) {
    shards_.reserve(config_.num_shards);
    for (size_t i = 0; i < config_.num_shards; ++i) {
        shards_.push_back(
            std::make_unique<LRUShard>(config_.max_entries_per_shard, config_.max_bytes_per_shard));
    }
}

CacheStore::~CacheStore() { stop_sweeper(); }

bool CacheStore::set(const std::string& key, const std::string& value,
                     std::optional<std::chrono::milliseconds> ttl) {
    return shard_for(key).set(key, value, ttl);
}

std::optional<std::string> CacheStore::get(const std::string& key) {
    return shard_for(key).get(key);
}

bool CacheStore::del(const std::string& key) { return shard_for(key).del(key); }

bool CacheStore::exists(const std::string& key) { return shard_for(key).exists(key); }

std::optional<int64_t> CacheStore::incr(const std::string& key) {
    return shard_for(key).incr(key);
}

std::vector<std::optional<std::string>> CacheStore::mget(const std::vector<std::string>& keys) {
    std::vector<std::optional<std::string>> results;
    results.reserve(keys.size());
    for (const auto& key : keys) {
        results.push_back(get(key));
    }
    return results;
}

std::vector<std::string> CacheStore::keys(const std::string& pattern) {
    std::string prefix = pattern;
    if (!prefix.empty() && prefix.back() == '*') {
        prefix.pop_back();
    }

    std::vector<std::string> result;
    for (auto& shard : shards_) {
        auto shard_keys = shard->keys_with_prefix(prefix);
        result.insert(result.end(), shard_keys.begin(), shard_keys.end());
    }

    std::sort(result.begin(), result.end());
    return result;
}

StoreStats CacheStore::stats() const {
    StoreStats s;
    s.num_shards = config_.num_shards;
    s.max_entries = config_.max_entries_per_shard * config_.num_shards;
    s.max_bytes = config_.max_bytes_per_shard * config_.num_shards;

    for (const auto& shard : shards_) {
        s.total_keys += shard->size();
        s.total_bytes += shard->bytes_used();
        s.hits += shard->hit_count();
        s.misses += shard->miss_count();
        s.evictions += shard->eviction_count();
        s.expired += shard->expired_count();
    }

    return s;
}

void CacheStore::start_sweeper() {
    sweeper_running_ = true;
    sweeper_thread_ = std::thread(&CacheStore::sweeper_loop, this);
}

void CacheStore::stop_sweeper() {
    sweeper_running_ = false;
    if (sweeper_thread_.joinable()) {
        sweeper_thread_.join();
    }
}

void CacheStore::sweeper_loop() {
    while (sweeper_running_.load(std::memory_order_relaxed)) {
        for (auto& shard : shards_) {
            if (!sweeper_running_.load(std::memory_order_relaxed)) {
                break;
            }
            shard->sweep_expired();
        }
        std::this_thread::sleep_for(config_.sweep_interval);
    }
}

LRUShard& CacheStore::shard_for(const std::string& key) { return *shards_[shard_index(key)]; }

const LRUShard& CacheStore::shard_for(const std::string& key) const {
    return *shards_[shard_index(key)];
}

size_t CacheStore::shard_index(const std::string& key) const {
    return std::hash<std::string>{}(key) % config_.num_shards;
}

}  // namespace cachecraft
