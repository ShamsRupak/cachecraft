#include <catch2/catch_test_macros.hpp>

#include <thread>
#include <vector>

#include "store/cache_store.h"

using namespace cachecraft;

TEST_CASE("CacheStore basic operations", "[store]") {
    StoreConfig config;
    config.num_shards = 4;
    config.max_entries_per_shard = 1000;
    config.max_bytes_per_shard = 0;
    CacheStore store(config);

    SECTION("Set and get") {
        store.set("key1", "value1");
        auto val = store.get("key1");
        REQUIRE(val.has_value());
        REQUIRE(*val == "value1");
    }

    SECTION("MGET") {
        store.set("a", "1");
        store.set("b", "2");
        auto vals = store.mget({"a", "b", "c"});
        REQUIRE(vals.size() == 3);
        REQUIRE(vals[0].has_value());
        REQUIRE(*vals[0] == "1");
        REQUIRE(vals[1].has_value());
        REQUIRE(*vals[1] == "2");
        REQUIRE(!vals[2].has_value());
    }

    SECTION("KEYS") {
        store.set("user:1", "alice");
        store.set("user:2", "bob");
        store.set("post:1", "hello");

        auto keys = store.keys("user:*");
        REQUIRE(keys.size() == 2);
    }

    SECTION("Stats") {
        store.set("k1", "v1");
        store.set("k2", "v2");
        store.get("k1");
        store.get("missing");

        auto s = store.stats();
        REQUIRE(s.total_keys == 2);
        REQUIRE(s.hits == 1);
        REQUIRE(s.misses == 1);
    }
}

TEST_CASE("CacheStore thread safety", "[store][concurrency]") {
    StoreConfig config;
    config.num_shards = 4;
    config.max_entries_per_shard = 10000;
    config.max_bytes_per_shard = 0;
    CacheStore store(config);

    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 1000;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key:" + std::to_string(t) + ":" + std::to_string(i);
                store.set(key, "value" + std::to_string(i));
                auto val = store.get(key);
                REQUIRE(val.has_value());
                store.del(key);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto s = store.stats();
    REQUIRE(s.total_keys == 0);
}
