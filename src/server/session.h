#pragma once

#include <asio.hpp>
#include <memory>
#include <string>

#include "store/cache_store.h"

namespace cachecraft {

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(asio::ip::tcp::socket socket, CacheStore& store);
    void start();

private:
    void do_read();
    void do_write(const std::string& response);
    std::string handle_command(const std::string& line);

    asio::ip::tcp::socket socket_;
    CacheStore& store_;
    asio::streambuf buffer_;
};

}  // namespace cachecraft
