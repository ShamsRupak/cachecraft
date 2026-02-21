#include "store/lru_shard.h"

#include <mutex>
#include <stdexcept>

namespace cachecraft {

LRUShard::LRUShard(size_t max_entries, size_t max_bytes)
    : max_entries_(max_entries), max_bytes_(max_bytes) {}

bool LRUShard::set(const std::string& key, const std::string& value,
                   std::optional<std::chrono::milliseconds> ttl) {
    std::unique_lock lock(mutex_);

    std::optional<TimePoint> expiry;
    if (ttl.has_value()) {
        expiry = Clock::now() + *ttl;
    }

    auto it = map_.find(key);
    if (it != map_.end()) {
        // Update existing entry
        current_bytes_ -= it->second->memory_usage();
        it->second->value = value;
        it->second->expiry = expiry;
        current_bytes_ += it->second->memory_usage();
        promote(it->second);
    } else {
        // Insert new entry
        CacheEntry entry{key, value, expiry};
        size_t entry_size = entry.memory_usage();
        entries_.push_front(std::move(entry));
        map_[key] = entries_.begin();
        current_bytes_ += entry_size;
    }

    evict_if_needed();
    return true;
}

std::optional<std::string> LRUShard::get(const std::string& key) {
    std::unique_lock lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        ++misses_;
        return std::nullopt;
    }

    // Lazy expiration
    if (is_expired(*it->second)) {
        current_bytes_ -= it->second->memory_usage();
        entries_.erase(it->second);
        map_.erase(it);
        ++expired_;
        ++misses_;
        return std::nullopt;
    }

    ++hits_;
    promote(it->second);
    return it->second->value;
}

bool LRUShard::del(const std::string& key) {
    std::unique_lock lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        return false;
    }

    remove_entry(it);
    return true;
}

bool LRUShard::exists(const std::string& key) {
    std::shared_lock lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        return false;
    }

    if (is_expired(*it->second)) {
        // Need write lock to remove — upgrade by releasing + reacquiring
        lock.unlock();
        std::unique_lock wlock(mutex_);
        auto it2 = map_.find(key);
        if (it2 != map_.end() && is_expired(*it2->second)) {
            remove_entry(it2);
            ++expired_;
        }
        return false;
    }

    return true;
}

std::optional<int64_t> LRUShard::incr(const std::string& key) {
    std::unique_lock lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        // Create with value "1"
        CacheEntry entry{key, "1", std::nullopt};
        current_bytes_ += entry.memory_usage();
        entries_.push_front(std::move(entry));
        map_[key] = entries_.begin();
        evict_if_needed();
        return 1;
    }

    if (is_expired(*it->second)) {
        remove_entry(it);
        ++expired_;
        CacheEntry entry{key, "1", std::nullopt};
        current_bytes_ += entry.memory_usage();
        entries_.push_front(std::move(entry));
        map_[key] = entries_.begin();
        evict_if_needed();
        return 1;
    }

    try {
        int64_t val = std::stoll(it->second->value);
        ++val;
        current_bytes_ -= it->second->memory_usage();
        it->second->value = std::to_string(val);
        current_bytes_ += it->second->memory_usage();
        promote(it->second);
        return val;
    } catch (...) {
        return std::nullopt;  // Not an integer
    }
}

std::vector<std::string> LRUShard::keys_with_prefix(const std::string& prefix) {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;

    for (const auto& [key, _] : map_) {
        if (prefix.empty() || key.rfind(prefix, 0) == 0) {
            result.push_back(key);
        }
    }

    return result;
}

size_t LRUShard::size() const {
    std::shared_lock lock(mutex_);
    return map_.size();
}

size_t LRUShard::bytes_used() const {
    std::shared_lock lock(mutex_);
    return current_bytes_;
}

uint64_t LRUShard::hit_count() const { return hits_.load(std::memory_order_relaxed); }
uint64_t LRUShard::miss_count() const { return misses_.load(std::memory_order_relaxed); }
uint64_t LRUShard::eviction_count() const { return evictions_.load(std::memory_order_relaxed); }
uint64_t LRUShard::expired_count() const { return expired_.load(std::memory_order_relaxed); }

size_t LRUShard::sweep_expired() {
    std::unique_lock lock(mutex_);
    size_t count = 0;

    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (is_expired(*it)) {
            auto map_it = map_.find(it->key);
            current_bytes_ -= it->memory_usage();
            it = entries_.erase(it);
            if (map_it != map_.end()) {
                map_.erase(map_it);
            }
            ++expired_;
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

void LRUShard::promote(EntryList::iterator it) {
    entries_.splice(entries_.begin(), entries_, it);
}

void LRUShard::evict_if_needed() {
    while ((max_entries_ > 0 && map_.size() > max_entries_) ||
           (max_bytes_ > 0 && current_bytes_ > max_bytes_)) {
        if (entries_.empty()) {
            break;
        }

        auto& back = entries_.back();
        auto map_it = map_.find(back.key);
        current_bytes_ -= back.memory_usage();
        entries_.pop_back();
        if (map_it != map_.end()) {
            map_.erase(map_it);
        }
        ++evictions_;
    }
}

void LRUShard::remove_entry(EntryMap::iterator it) {
    current_bytes_ -= it->second->memory_usage();
    entries_.erase(it->second);
    map_.erase(it);
}

bool LRUShard::is_expired(const CacheEntry& entry) { return entry.is_expired(); }

}  // namespace cachecraft
