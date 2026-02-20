#include <catch2/catch_test_macros.hpp>

#include "common/protocol.h"

using namespace cachecraft;

TEST_CASE("Protocol parsing", "[protocol]") {
    SECTION("SET command") {
        auto cmd = Protocol::parse("SET mykey myvalue");
        REQUIRE(cmd.type == CommandType::SET);
        REQUIRE(cmd.args.size() == 2);
        REQUIRE(cmd.args[0] == "mykey");
        REQUIRE(cmd.args[1] == "myvalue");
    }

    SECTION("SET with TTL") {
        auto cmd = Protocol::parse("SET mykey myvalue 5000");
        REQUIRE(cmd.type == CommandType::SET);
        REQUIRE(cmd.args.size() == 3);
        REQUIRE(cmd.args[2] == "5000");
    }

    SECTION("GET command") {
        auto cmd = Protocol::parse("GET mykey");
        REQUIRE(cmd.type == CommandType::GET);
        REQUIRE(cmd.args.size() == 1);
        REQUIRE(cmd.args[0] == "mykey");
    }

    SECTION("DEL command") {
        auto cmd = Protocol::parse("DEL mykey");
        REQUIRE(cmd.type == CommandType::DEL);
        REQUIRE(cmd.args.size() == 1);
    }

    SECTION("EXISTS command") {
        auto cmd = Protocol::parse("EXISTS mykey");
        REQUIRE(cmd.type == CommandType::EXISTS);
    }

    SECTION("INCR command") {
        auto cmd = Protocol::parse("INCR counter");
        REQUIRE(cmd.type == CommandType::INCR);
        REQUIRE(cmd.args[0] == "counter");
    }

    SECTION("MGET command") {
        auto cmd = Protocol::parse("MGET k1 k2 k3");
        REQUIRE(cmd.type == CommandType::MGET);
        REQUIRE(cmd.args.size() == 3);
    }

    SECTION("KEYS command") {
        auto cmd = Protocol::parse("KEYS user:*");
        REQUIRE(cmd.type == CommandType::KEYS);
        REQUIRE(cmd.args[0] == "user:*");
    }

    SECTION("STATS command") {
        auto cmd = Protocol::parse("STATS");
        REQUIRE(cmd.type == CommandType::STATS);
    }

    SECTION("PING command") {
        auto cmd = Protocol::parse("PING");
        REQUIRE(cmd.type == CommandType::PING);
    }

    SECTION("Case insensitive") {
        REQUIRE(Protocol::parse("ping").type == CommandType::PING);
        REQUIRE(Protocol::parse("set KEY VAL").type == CommandType::SET);
        REQUIRE(Protocol::parse("Get foo").type == CommandType::GET);
    }

    SECTION("Unknown command") {
        auto cmd = Protocol::parse("FOOBAR");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }

    SECTION("Empty input") {
        auto cmd = Protocol::parse("");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }

    SECTION("Whitespace only") {
        auto cmd = Protocol::parse("   ");
        REQUIRE(cmd.type == CommandType::UNKNOWN);
    }
}

TEST_CASE("Protocol responses", "[protocol]") {
    REQUIRE(Protocol::ok() == "OK\n");
    REQUIRE(Protocol::nil() == "NIL\n");
    REQUIRE(Protocol::pong() == "PONG\n");
    REQUIRE(Protocol::error("bad") == "ERR bad\n");
    REQUIRE(Protocol::value("hello") == "VALUE hello\n");

    SECTION("VALUES response") {
        std::vector<std::optional<std::string>> vals = {"a", std::nullopt, "c"};
        REQUIRE(Protocol::values(vals) == "VALUES a NIL c\n");
    }

    SECTION("STATS response") {
        auto resp = Protocol::stats({{"keys", "10"}, {"hits", "5"}});
        REQUIRE(resp == "STATS keys=10 hits=5\n");
    }
}
