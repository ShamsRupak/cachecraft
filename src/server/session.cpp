#include "server/session.h"

#include "common/protocol.h"

namespace cachecraft {

Session::Session(asio::ip::tcp::socket socket, CacheStore& store)
    : socket_(std::move(socket)), store_(store) {}

void Session::start() { do_read(); }

void Session::do_read() {
    auto self = shared_from_this();
    asio::async_read_until(socket_, buffer_, '\n',
                           [this, self](std::error_code ec, std::size_t /*length*/) {
                               if (!ec) {
                                   std::istream is(&buffer_);
                                   std::string line;
                                   std::getline(is, line);

                                   // Strip trailing \r
                                   if (!line.empty() && line.back() == '\r') {
                                       line.pop_back();
                                   }

                                   auto response = handle_command(line);
                                   do_write(response);
                               }
                           });
}

void Session::do_write(const std::string& response) {
    auto self = shared_from_this();
    auto buf = std::make_shared<std::string>(response);
    asio::async_write(socket_, asio::buffer(*buf),
                      [this, self, buf](std::error_code ec, std::size_t /*length*/) {
                          if (!ec) {
                              do_read();
                          }
                      });
}

std::string Session::handle_command(const std::string& line) {
    if (line.size() > Protocol::kMaxLineLength) {
        return Protocol::error("line too long");
    }

    auto cmd = Protocol::parse(line);

    switch (cmd.type) {
        case CommandType::PING:
            return Protocol::pong();

        case CommandType::SET: {
            if (cmd.args.size() < 2 || cmd.args.size() > 3) {
                return Protocol::error("usage: SET <key> <value> [ttl_ms]");
            }
            std::optional<std::chrono::milliseconds> ttl;
            if (cmd.args.size() == 3) {
                try {
                    auto ms = std::stoll(cmd.args[2]);
                    if (ms <= 0) {
                        return Protocol::error("TTL must be positive");
                    }
                    ttl = std::chrono::milliseconds(ms);
                } catch (...) {
                    return Protocol::error("invalid TTL value");
                }
            }
            store_.set(cmd.args[0], cmd.args[1], ttl);
            return Protocol::ok();
        }

        case CommandType::GET: {
            if (cmd.args.size() != 1) {
                return Protocol::error("usage: GET <key>");
            }
            auto val = store_.get(cmd.args[0]);
            return val ? Protocol::value(*val) : Protocol::nil();
        }

        case CommandType::DEL: {
            if (cmd.args.size() != 1) {
                return Protocol::error("usage: DEL <key>");
            }
            return store_.del(cmd.args[0]) ? Protocol::ok() : Protocol::nil();
        }

        case CommandType::EXISTS: {
            if (cmd.args.size() != 1) {
                return Protocol::error("usage: EXISTS <key>");
            }
            return store_.exists(cmd.args[0]) ? Protocol::value("1") : Protocol::value("0");
        }

        case CommandType::INCR: {
            if (cmd.args.size() != 1) {
                return Protocol::error("usage: INCR <key>");
            }
            auto val = store_.incr(cmd.args[0]);
            return val ? Protocol::value(std::to_string(*val))
                       : Protocol::error("value is not an integer");
        }

        case CommandType::MGET: {
            if (cmd.args.empty()) {
                return Protocol::error("usage: MGET <key1> [key2] ...");
            }
            auto vals = store_.mget(cmd.args);
            return Protocol::values(vals);
        }

        case CommandType::KEYS: {
            if (cmd.args.size() != 1) {
                return Protocol::error("usage: KEYS <prefix*>");
            }
            auto keys = store_.keys(cmd.args[0]);
            std::vector<std::optional<std::string>> opt_keys;
            opt_keys.reserve(keys.size());
            for (auto& k : keys) {
                opt_keys.push_back(std::move(k));
            }
            return Protocol::values(opt_keys);
        }

        case CommandType::STATS: {
            auto s = store_.stats();
            return Protocol::stats({
                {"keys", std::to_string(s.total_keys)},
                {"bytes", std::to_string(s.total_bytes)},
                {"hits", std::to_string(s.hits)},
                {"misses", std::to_string(s.misses)},
                {"evictions", std::to_string(s.evictions)},
                {"expired", std::to_string(s.expired)},
                {"max_entries", std::to_string(s.max_entries)},
                {"max_bytes", std::to_string(s.max_bytes)},
                {"shards", std::to_string(s.num_shards)},
            });
        }

        case CommandType::UNKNOWN:
        default:
            return Protocol::error("unknown command");
    }
}

}  // namespace cachecraft
