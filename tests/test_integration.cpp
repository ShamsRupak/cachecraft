#include <catch2/catch_test_macros.hpp>

#include <asio.hpp>
#include <chrono>
#include <string>
#include <thread>

#include "server/server.h"
#include "store/cache_store.h"

using namespace cachecraft;

namespace {

class TestClient {
public:
    TestClient(const std::string& host, uint16_t port) {
        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        asio::connect(socket_, endpoints);
    }

    std::string send(const std::string& cmd) {
        std::string msg = cmd + "\n";
        asio::write(socket_, asio::buffer(msg));

        asio::streambuf response;
        asio::read_until(socket_, response, '\n');

        std::istream is(&response);
        std::string line;
        std::getline(is, line);

        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return line;
    }

private:
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_{io_context_};
};

/// RAII helper: starts server in background, stops on destruction.
struct TestServer {
    StoreConfig config;
    CacheStore store;
    asio::io_context io_context;
    Server server;
    std::thread thread;

    TestServer()
        : config{.num_shards = 2, .max_entries_per_shard = 100, .max_bytes_per_shard = 0},
          store(config),
          server(io_context, 0, store),
          thread([this]() { io_context.run(); }) {
        store.start_sweeper();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestServer() {
        io_context.stop();
        thread.join();
        store.stop_sweeper();
    }

    uint16_t port() const { return server.port(); }
};

}  // namespace

TEST_CASE("Integration: PING", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    REQUIRE(client.send("PING") == "PONG");
}

TEST_CASE("Integration: SET and GET", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    REQUIRE(client.send("SET mykey myvalue") == "OK");
    REQUIRE(client.send("GET mykey") == "VALUE myvalue");
}

TEST_CASE("Integration: GET missing key", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    REQUIRE(client.send("GET missing") == "NIL");
}

TEST_CASE("Integration: DEL", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    client.send("SET delme val");
    REQUIRE(client.send("DEL delme") == "OK");
    REQUIRE(client.send("GET delme") == "NIL");
}

TEST_CASE("Integration: EXISTS", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    client.send("SET exists_key val");
    REQUIRE(client.send("EXISTS exists_key") == "VALUE 1");
    REQUIRE(client.send("EXISTS no_such_key") == "VALUE 0");
}

TEST_CASE("Integration: INCR", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    REQUIRE(client.send("INCR counter") == "VALUE 1");
    REQUIRE(client.send("INCR counter") == "VALUE 2");
    REQUIRE(client.send("INCR counter") == "VALUE 3");
}

TEST_CASE("Integration: MGET", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    client.send("SET a 1");
    client.send("SET b 2");
    REQUIRE(client.send("MGET a b c") == "VALUES 1 2 NIL");
}

TEST_CASE("Integration: SET with TTL", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    REQUIRE(client.send("SET temp val 100") == "OK");
    REQUIRE(client.send("GET temp") == "VALUE val");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    REQUIRE(client.send("GET temp") == "NIL");
}

TEST_CASE("Integration: STATS", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    auto resp = client.send("STATS");
    REQUIRE(resp.rfind("STATS", 0) == 0);
}

TEST_CASE("Integration: unknown command", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    auto resp = client.send("FOOBAR");
    REQUIRE(resp.rfind("ERR", 0) == 0);
}

TEST_CASE("Integration: error on bad SET", "[integration]") {
    TestServer srv;
    TestClient client("127.0.0.1", srv.port());
    auto resp = client.send("SET");
    REQUIRE(resp.rfind("ERR", 0) == 0);
}
