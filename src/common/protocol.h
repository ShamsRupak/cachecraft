#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cachecraft {

enum class CommandType { SET, GET, DEL, EXISTS, INCR, MGET, KEYS, STATS, PING, UNKNOWN };

struct Command {
    CommandType type = CommandType::UNKNOWN;
    std::vector<std::string> args;
};

class Protocol {
public:
    static constexpr size_t kMaxLineLength = 65536;

    static Command parse(const std::string& line);

    static std::string ok();
    static std::string error(const std::string& msg);
    static std::string value(const std::string& val);
    static std::string nil();
    static std::string pong();
    static std::string values(const std::vector<std::optional<std::string>>& vals);
    static std::string stats(const std::vector<std::pair<std::string, std::string>>& kv);
};

}  // namespace cachecraft
