#pragma once

#include <asio.hpp>
#include <cstdint>

#include "store/cache_store.h"

namespace cachecraft {

class Server {
public:
    Server(asio::io_context& io_context, uint16_t port, CacheStore& store);
    [[nodiscard]] uint16_t port() const;

private:
    void do_accept();

    asio::ip::tcp::acceptor acceptor_;
    CacheStore& store_;
};

}  // namespace cachecraft
