#pragma once

#include <atomic>
#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/types.h"

namespace cachecraft {

/// A single LRU shard — O(1) get/set/delete via hash map + doubly-linked list.
/// Thread-safe with shared_mutex (readers-writer lock).
class LRUShard {
public:
    explicit LRUShard(size_t max_entries, size_t max_bytes);

    bool set(const std::string& key, const std::string& value,
             std::optional<std::chrono::milliseconds> ttl);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    std::optional<int64_t> incr(const std::string& key);
    std::vector<std::string> keys_with_prefix(const std::string& prefix);

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t bytes_used() const;
    [[nodiscard]] uint64_t hit_count() const;
    [[nodiscard]] uint64_t miss_count() const;
    [[nodiscard]] uint64_t eviction_count() const;
    [[nodiscard]] uint64_t expired_count() const;

    /// Remove all expired entries. Returns count removed.
    size_t sweep_expired();

private:
    using EntryList = std::list<CacheEntry>;
    using EntryMap = std::unordered_map<std::string, EntryList::iterator>;

    void promote(EntryList::iterator it);
    void evict_if_needed();
    void remove_entry(EntryMap::iterator it);
    static bool is_expired(const CacheEntry& entry);

    mutable std::shared_mutex mutex_;
    EntryList entries_;
    EntryMap map_;

    size_t max_entries_;
    size_t max_bytes_;
    size_t current_bytes_ = 0;

    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};
    std::atomic<uint64_t> evictions_{0};
    std::atomic<uint64_t> expired_{0};
};

}  // namespace cachecraft
