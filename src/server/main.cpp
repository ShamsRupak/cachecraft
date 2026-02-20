#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "server/server.h"

static asio::io_context* g_io_context = nullptr;

void signal_handler(int /*sig*/) {
    if (g_io_context) {
        g_io_context->stop();
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 9090;
    size_t threads = std::thread::hardware_concurrency();
    size_t max_entries = 100000;
    size_t max_bytes = 512ULL * 1024 * 1024;
    size_t num_shards = 16;

    for (int i = 1; i < argc; i += 2) {
        std::string arg = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return 1;
        }
        std::string val = argv[i + 1];

        if (arg == "--port") {
            port = static_cast<uint16_t>(std::stoi(val));
        } else if (arg == "--threads") {
            threads = static_cast<size_t>(std::stoi(val));
        } else if (arg == "--max-entries") {
            max_entries = static_cast<size_t>(std::stoi(val));
        } else if (arg == "--max-bytes") {
            max_bytes = static_cast<size_t>(std::stoll(val));
        } else if (arg == "--shards") {
            num_shards = static_cast<size_t>(std::stoi(val));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Usage: cachecraftd [--port N] [--threads N] [--max-entries N] "
                         "[--max-bytes N] [--shards N]\n";
            return 1;
        }
    }

    cachecraft::StoreConfig config;
    config.num_shards = num_shards;
    config.max_entries_per_shard = max_entries / num_shards;
    config.max_bytes_per_shard = max_bytes / num_shards;

    cachecraft::CacheStore store(config);
    store.start_sweeper();

    asio::io_context io_context;
    g_io_context = &io_context;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    cachecraft::Server server(io_context, port, store);

    std::cout << "CacheCraft server listening on port " << server.port() << " with " << threads
              << " threads and " << num_shards << " shards\n";

    std::vector<std::thread> thread_pool;
    for (size_t i = 1; i < threads; ++i) {
        thread_pool.emplace_back([&io_context]() { io_context.run(); });
    }

    io_context.run();

    for (auto& t : thread_pool) {
        t.join();
    }

    store.stop_sweeper();
    std::cout << "Server stopped.\n";
    return 0;
}
