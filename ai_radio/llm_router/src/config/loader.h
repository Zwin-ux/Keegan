#pragma once
#include <string>
#include <vector>
#include <optional>
#include <map>
#include "vjson/vjson.h"

namespace config {

struct ServerConfig {
    std::string host;
    int port;
    int maxConcurrentRequests;
    int requestTimeoutMs;
};

enum class TargetType {
    OpenAI,
    Anthropic,
    Http,
    Unknown
};

struct Target {
    std::string name;
    TargetType type;
    std::string baseUrl;
    std::string apiKeyEnv;
    std::vector<std::string> models;
};

struct Match {
    std::optional<std::string> path;
    std::optional<std::string> modelPrefix;
};

enum class StrategyType {
    RoundRobin,
    WeightedRandom,
    Unknown
};

struct Strategy {
    StrategyType type;
    std::vector<std::string> targetNames;
};

struct Route {
    std::string name;
    Match match;
    Strategy strategy;
};

struct LoggingConfig {
    std::string mode;
    std::string level;
};

struct Config {
    ServerConfig server;
    std::vector<Target> targets;
    std::vector<Route> routes;
    LoggingConfig logging;
};

class ConfigLoader {
public:
    static Config load(const std::string& path);
};

} // namespace config
