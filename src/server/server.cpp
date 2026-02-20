#include "server/server.h"

#include "server/session.h"

namespace cachecraft {

Server::Server(asio::io_context& io_context, uint16_t port, CacheStore& store)
    : acceptor_(io_context), store_(store) {
    auto endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    do_accept();
}

uint16_t Server::port() const { return acceptor_.local_endpoint().port(); }

void Server::do_accept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session>(std::move(socket), store_)->start();
        }
        do_accept();
    });
}

}  // namespace cachecraft
