#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace cachecraft {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct CacheEntry {
    std::string key;
    std::string value;
    std::optional<TimePoint> expiry;

    [[nodiscard]] bool is_expired() const {
        return expiry.has_value() && Clock::now() >= *expiry;
    }

    [[nodiscard]] size_t memory_usage() const {
        return key.capacity() + value.capacity() + sizeof(CacheEntry);
    }
};

struct StoreStats {
    size_t total_keys = 0;
    size_t total_bytes = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    uint64_t expired = 0;
    size_t max_entries = 0;
    size_t max_bytes = 0;
    size_t num_shards = 0;
};

}  // namespace cachecraft
