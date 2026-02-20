#include <asio.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "9090";

    for (int i = 1; i < argc; i += 2) {
        std::string arg = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return 1;
        }
        std::string val = argv[i + 1];
        if (arg == "--host") {
            host = val;
        } else if (arg == "--port") {
            port = val;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Usage: cachecraft-cli [--host HOST] [--port PORT]\n";
            return 1;
        }
    }

    try {
        asio::io_context io_context;
        asio::ip::tcp::resolver resolver(io_context);
        asio::ip::tcp::socket socket(io_context);

        auto endpoints = resolver.resolve(host, port);
        asio::connect(socket, endpoints);

        std::cout << "Connected to " << host << ":" << port << "\n";
        std::cout << "Commands: SET, GET, DEL, EXISTS, INCR, MGET, KEYS, STATS, PING\n";
        std::cout << "Type 'quit' to exit.\n\n";
        std::cout << "cachecraft> " << std::flush;

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                std::cout << "cachecraft> " << std::flush;
                continue;
            }

            if (line == "quit" || line == "exit" || line == "QUIT" || line == "EXIT") {
                break;
            }

            line += "\n";
            asio::write(socket, asio::buffer(line));

            asio::streambuf response;
            asio::read_until(socket, response, '\n');

            std::istream is(&response);
            std::string resp_line;
            std::getline(is, resp_line);

            std::cout << resp_line << "\n";
            std::cout << "cachecraft> " << std::flush;
        }

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
