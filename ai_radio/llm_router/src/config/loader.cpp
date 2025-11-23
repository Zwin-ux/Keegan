#include "loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace config {

Config ConfigLoader::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Could not open config file: " + path);
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string content = buffer.str();

    vjson::Object root;
    vjson::ParseContext ctx;
    if (!root.ParseJSON(content, &ctx)) {
        throw std::runtime_error("Failed to parse config: " + ctx.error_message + " at line " + std::to_string(ctx.error_line));
    }

    Config cfg;

    // Server
    const auto& serverObj = root.ObjectAtKeyOrEmpty("server");
    cfg.server.host = serverObj.StringAtKey("host", "0.0.0.0");
    cfg.server.port = serverObj.IntAtKey("port", 8080);
    cfg.server.maxConcurrentRequests = serverObj.IntAtKey("maxConcurrentRequests", 256);
    cfg.server.requestTimeoutMs = serverObj.IntAtKey("requestTimeoutMs", 30000);

    // Targets
    const auto& targetsArr = root.ArrayAtKeyOrEmpty("targets");
    for (const auto& val : targetsArr) {
        if (!val.IsObject()) continue;
        const auto& tObj = val.GetObject();
        Target t;
        t.name = tObj.StringAtKey("name", "");
        std::string typeStr = tObj.StringAtKey("type", "");
        if (typeStr == "openai") t.type = TargetType::OpenAI;
        else if (typeStr == "anthropic") t.type = TargetType::Anthropic;
        else if (typeStr == "http") t.type = TargetType::Http;
        else t.type = TargetType::Unknown;

        t.baseUrl = tObj.StringAtKey("baseUrl", "");
        t.apiKeyEnv = tObj.StringAtKey("apiKeyEnv", "");
        
        const auto& modelsArr = tObj.ArrayAtKeyOrEmpty("models");
        for (const auto& m : modelsArr) {
            if (m.IsString()) t.models.push_back(m.GetString());
        }
        cfg.targets.push_back(t);
    }

    // Routes
    const auto& routesArr = root.ArrayAtKeyOrEmpty("routes");
    for (const auto& val : routesArr) {
        if (!val.IsObject()) continue;
        const auto& rObj = val.GetObject();
        Route r;
        r.name = rObj.StringAtKey("name", "");
        
        const auto& matchObj = rObj.ObjectAtKeyOrEmpty("match");
        if (matchObj.HasKey("path")) r.match.path = matchObj.StringAtKey("path", "");
        if (matchObj.HasKey("modelPrefix")) r.match.modelPrefix = matchObj.StringAtKey("modelPrefix", "");

        const auto& stratObj = rObj.ObjectAtKeyOrEmpty("strategy");
        std::string stratType = stratObj.StringAtKey("type", "");
        if (stratType == "round_robin") r.strategy.type = StrategyType::RoundRobin;
        else if (stratType == "weighted_random") r.strategy.type = StrategyType::WeightedRandom;
        else r.strategy.type = StrategyType::Unknown;

        const auto& stratTargets = stratObj.ArrayAtKeyOrEmpty("targets");
        for (const auto& st : stratTargets) {
            if (st.IsString()) r.strategy.targetNames.push_back(st.GetString());
        }
        cfg.routes.push_back(r);
    }

    // Logging
    const auto& logObj = root.ObjectAtKeyOrEmpty("logging");
    cfg.logging.mode = logObj.StringAtKey("mode", "stdout");
    cfg.logging.level = logObj.StringAtKey("level", "info");

    return cfg;
}

} // namespace config
