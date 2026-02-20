#include "common/protocol.h"

#include <algorithm>
#include <sstream>

namespace cachecraft {

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

}  // namespace

Command Protocol::parse(const std::string& line) {
    if (line.size() > kMaxLineLength) {
        return {CommandType::UNKNOWN, {}};
    }

    auto tokens = split(line);
    if (tokens.empty()) {
        return {CommandType::UNKNOWN, {}};
    }

    auto cmd = to_upper(tokens[0]);
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    if (cmd == "SET") return {CommandType::SET, std::move(args)};
    if (cmd == "GET") return {CommandType::GET, std::move(args)};
    if (cmd == "DEL") return {CommandType::DEL, std::move(args)};
    if (cmd == "EXISTS") return {CommandType::EXISTS, std::move(args)};
    if (cmd == "INCR") return {CommandType::INCR, std::move(args)};
    if (cmd == "MGET") return {CommandType::MGET, std::move(args)};
    if (cmd == "KEYS") return {CommandType::KEYS, std::move(args)};
    if (cmd == "STATS") return {CommandType::STATS, std::move(args)};
    if (cmd == "PING") return {CommandType::PING, std::move(args)};

    return {CommandType::UNKNOWN, std::move(args)};
}

std::string Protocol::ok() { return "OK\n"; }
std::string Protocol::error(const std::string& msg) { return "ERR " + msg + "\n"; }
std::string Protocol::value(const std::string& val) { return "VALUE " + val + "\n"; }
std::string Protocol::nil() { return "NIL\n"; }
std::string Protocol::pong() { return "PONG\n"; }

std::string Protocol::values(const std::vector<std::optional<std::string>>& vals) {
    std::string result = "VALUES";
    for (const auto& v : vals) {
        result += ' ';
        result += v.has_value() ? *v : "NIL";
    }
    result += '\n';
    return result;
}

std::string Protocol::stats(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string result = "STATS";
    for (const auto& [k, v] : kv) {
        result += ' ';
        result += k;
        result += '=';
        result += v;
    }
    result += '\n';
    return result;
}

}  // namespace cachecraft
