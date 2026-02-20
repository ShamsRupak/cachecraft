#include <catch2/catch_test_macros.hpp>

#include "store/lru_shard.h"

using namespace cachecraft;

TEST_CASE("LRU basic operations", "[lru]") {
    LRUShard shard(5, 0);  // max 5 entries, no byte limit

    SECTION("Set and get") {
        shard.set("key1", "value1", std::nullopt);
        auto val = shard.get("key1");
        REQUIRE(val.has_value());
        REQUIRE(*val == "value1");
    }

    SECTION("Get nonexistent key") {
        auto val = shard.get("missing");
        REQUIRE(!val.has_value());
    }

    SECTION("Delete key") {
        shard.set("key1", "value1", std::nullopt);
        REQUIRE(shard.del("key1"));
        REQUIRE(!shard.get("key1").has_value());
    }

    SECTION("Delete nonexistent key") { REQUIRE(!shard.del("missing")); }

    SECTION("Exists") {
        shard.set("key1", "value1", std::nullopt);
        REQUIRE(shard.exists("key1"));
        REQUIRE(!shard.exists("missing"));
    }

    SECTION("Overwrite") {
        shard.set("key1", "v1", std::nullopt);
        shard.set("key1", "v2", std::nullopt);
        auto val = shard.get("key1");
        REQUIRE(val.has_value());
        REQUIRE(*val == "v2");
        REQUIRE(shard.size() == 1);
    }
}

TEST_CASE("LRU eviction", "[lru]") {
    LRUShard shard(3, 0);  // max 3 entries

    SECTION("Evicts LRU entry when at capacity") {
        shard.set("a", "1", std::nullopt);
        shard.set("b", "2", std::nullopt);
        shard.set("c", "3", std::nullopt);
        shard.set("d", "4", std::nullopt);  // evicts "a"

        REQUIRE(!shard.get("a").has_value());
        REQUIRE(shard.get("b").has_value());
        REQUIRE(shard.get("c").has_value());
        REQUIRE(shard.get("d").has_value());
    }

    SECTION("Access promotes entry in LRU") {
        shard.set("a", "1", std::nullopt);
        shard.set("b", "2", std::nullopt);
        shard.set("c", "3", std::nullopt);

        shard.get("a");  // promote "a"
        shard.set("d", "4", std::nullopt);  // should evict "b" not "a"

        REQUIRE(shard.get("a").has_value());
        REQUIRE(!shard.get("b").has_value());
    }

    SECTION("Eviction count") {
        shard.set("a", "1", std::nullopt);
        shard.set("b", "2", std::nullopt);
        shard.set("c", "3", std::nullopt);
        shard.set("d", "4", std::nullopt);
        shard.set("e", "5", std::nullopt);

        REQUIRE(shard.eviction_count() == 2);
    }
}

TEST_CASE("LRU INCR", "[lru]") {
    LRUShard shard(100, 0);

    SECTION("INCR nonexistent key creates it") {
        auto val = shard.incr("counter");
        REQUIRE(val.has_value());
        REQUIRE(*val == 1);
    }

    SECTION("INCR existing integer") {
        shard.set("counter", "10", std::nullopt);
        auto val = shard.incr("counter");
        REQUIRE(val.has_value());
        REQUIRE(*val == 11);
    }

    SECTION("INCR non-integer fails") {
        shard.set("str", "hello", std::nullopt);
        auto val = shard.incr("str");
        REQUIRE(!val.has_value());
    }
}

TEST_CASE("LRU prefix keys", "[lru]") {
    LRUShard shard(100, 0);

    shard.set("user:1", "alice", std::nullopt);
    shard.set("user:2", "bob", std::nullopt);
    shard.set("post:1", "hello", std::nullopt);

    auto keys = shard.keys_with_prefix("user:");
    REQUIRE(keys.size() == 2);
}
