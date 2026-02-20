#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "store/cache_store.h"
#include "store/lru_shard.h"

using namespace cachecraft;

TEST_CASE("TTL expiration on access", "[ttl]") {
    LRUShard shard(100, 0);

    SECTION("Key expires after TTL") {
        shard.set("key1", "value1", std::chrono::milliseconds(50));

        auto val = shard.get("key1");
        REQUIRE(val.has_value());
        REQUIRE(*val == "value1");

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        val = shard.get("key1");
        REQUIRE(!val.has_value());
    }

    SECTION("Key without TTL never expires") {
        shard.set("key1", "value1", std::nullopt);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto val = shard.get("key1");
        REQUIRE(val.has_value());
    }

    SECTION("EXISTS detects expired key") {
        shard.set("key1", "value1", std::chrono::milliseconds(50));
        REQUIRE(shard.exists("key1"));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        REQUIRE(!shard.exists("key1"));
    }
}

TEST_CASE("TTL background sweep", "[ttl]") {
    LRUShard shard(100, 0);

    shard.set("expire1", "v1", std::chrono::milliseconds(50));
    shard.set("expire2", "v2", std::chrono::milliseconds(50));
    shard.set("persist", "v3", std::nullopt);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t swept = shard.sweep_expired();
    REQUIRE(swept == 2);
    REQUIRE(shard.size() == 1);
    REQUIRE(shard.get("persist").has_value());
}

TEST_CASE("CacheStore TTL via lazy expiration", "[ttl][store]") {
    StoreConfig config;
    config.num_shards = 2;
    config.max_entries_per_shard = 100;
    config.max_bytes_per_shard = 0;
    CacheStore store(config);

    store.set("temp", "data", std::chrono::milliseconds(50));
    store.set("perm", "data");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(!store.get("temp").has_value());
    REQUIRE(store.get("perm").has_value());
}
