#include <asio.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

struct BenchConfig {
    std::string host = "127.0.0.1";
    std::string port = "9090";
    int num_clients = 4;
    int num_requests = 10000;
    int key_space = 1000;
    int value_size = 64;
    double get_ratio = 0.8;
};

struct ClientResult {
    std::vector<double> latencies_us;
    uint64_t errors = 0;
};

ClientResult run_client(const BenchConfig& config) {
    ClientResult result;

    try {
        asio::io_context io_context;
        asio::ip::tcp::resolver resolver(io_context);
        asio::ip::tcp::socket socket(io_context);

        auto endpoints = resolver.resolve(config.host, config.port);
        asio::connect(socket, endpoints);

        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> key_dist(0, config.key_space - 1);
        std::uniform_real_distribution<double> ratio_dist(0.0, 1.0);

        std::string value(static_cast<size_t>(config.value_size), 'x');

        int ops = config.num_requests / config.num_clients;

        for (int i = 0; i < ops; ++i) {
            std::string key = "bench:" + std::to_string(key_dist(rng));
            std::string cmd;

            if (ratio_dist(rng) < config.get_ratio) {
                cmd = "GET " + key + "\n";
            } else {
                cmd = "SET " + key + " " + value + "\n";
            }

            auto start = std::chrono::high_resolution_clock::now();

            asio::write(socket, asio::buffer(cmd));

            asio::streambuf response;
            asio::read_until(socket, response, '\n');

            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration<double, std::micro>(end - start).count();
            result.latencies_us.push_back(us);
        }

    } catch (std::exception& e) {
        ++result.errors;
    }

    return result;
}

int main(int argc, char* argv[]) {
    BenchConfig config;

    for (int i = 1; i < argc; i += 2) {
        std::string arg = argv[i];
        if (i + 1 >= argc) {
            break;
        }
        std::string val = argv[i + 1];

        if (arg == "--host") {
            config.host = val;
        } else if (arg == "--port") {
            config.port = val;
        } else if (arg == "--clients") {
            config.num_clients = std::stoi(val);
        } else if (arg == "--requests") {
            config.num_requests = std::stoi(val);
        } else if (arg == "--keyspace") {
            config.key_space = std::stoi(val);
        } else if (arg == "--value-size") {
            config.value_size = std::stoi(val);
        } else if (arg == "--get-ratio") {
            config.get_ratio = std::stod(val);
        }
    }

    std::cout << "CacheCraft Benchmark\n";
    std::cout << "====================\n";
    std::cout << "Host:        " << config.host << ":" << config.port << "\n";
    std::cout << "Clients:     " << config.num_clients << "\n";
    std::cout << "Requests:    " << config.num_requests << "\n";
    std::cout << "Key space:   " << config.key_space << "\n";
    std::cout << "Value size:  " << config.value_size << " bytes\n";
    std::cout << "GET ratio:   " << (config.get_ratio * 100) << "%\n";
    std::cout << "====================\n\n";

    std::vector<ClientResult> results(static_cast<size_t>(config.num_clients));
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < config.num_clients; ++i) {
        threads.emplace_back([&config, &results, i]() { results[i] = run_client(config); });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_seconds = std::chrono::duration<double>(end - start).count();

    // Aggregate
    std::vector<double> all_latencies;
    uint64_t total_errors = 0;

    for (auto& r : results) {
        all_latencies.insert(all_latencies.end(), r.latencies_us.begin(), r.latencies_us.end());
        total_errors += r.errors;
    }

    if (all_latencies.empty()) {
        std::cerr << "No successful operations!\n";
        return 1;
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    size_t n = all_latencies.size();
    double avg = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0) /
                 static_cast<double>(n);
    double p50 = all_latencies[n * 50 / 100];
    double p95 = all_latencies[n * 95 / 100];
    double p99 = all_latencies[n * 99 / 100];
    double ops_sec = static_cast<double>(n) / total_seconds;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Results\n";
    std::cout << "-------\n";
    std::cout << "Total ops:    " << n << "\n";
    std::cout << "Total time:   " << total_seconds << " s\n";
    std::cout << "Throughput:   " << ops_sec << " ops/sec\n";
    std::cout << "Avg latency:  " << avg << " us\n";
    std::cout << "P50 latency:  " << p50 << " us\n";
    std::cout << "P95 latency:  " << p95 << " us\n";
    std::cout << "P99 latency:  " << p99 << " us\n";
    std::cout << "Errors:       " << total_errors << "\n";

    return 0;
}
