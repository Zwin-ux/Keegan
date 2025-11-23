#include "router.h"
#include "providers/openai_client.h"
#include "logging/logger.h"
#include "vjson/vjson.h"

namespace routing {

Router::Router(const config::Config& cfg) : config_(cfg) {
    for (const auto& t : cfg.targets) {
        targets_[t.name] = t;
    }
    
    clients_[config::TargetType::OpenAI] = std::make_unique<providers::OpenAIClient>();
}

providers::BaseClient* Router::getClient(config::TargetType type) {
    if (clients_.find(type) != clients_.end()) {
        return clients_[type].get();
    }
    return nullptr;
}

const config::Target* Router::routeRequest(const providers::RequestContext& ctx) {
    std::string model;
    vjson::Object bodyObj;
    // We try to parse, if fails model is empty
    bodyObj.ParseJSON(ctx.body);
    model = bodyObj.StringAtKey("model", "");
    
    for (const auto& route : config_.routes) {
        bool match = true;
        
        if (route.match.path.has_value()) {
            if (ctx.path.find(route.match.path.value()) != 0) {
                match = false;
            }
        }
        
        if (match && route.match.modelPrefix.has_value()) {
            if (model.find(route.match.modelPrefix.value()) != 0) { // starts_with
                match = false;
            }
        }
        
        if (match) {
            if (route.strategy.type == config::StrategyType::RoundRobin) {
                const auto& names = route.strategy.targetNames;
                if (names.empty()) continue;
                
                int& idx = rr_counters_[route.name];
                std::string targetName = names[idx % names.size()];
                idx++;
                
                if (targets_.count(targetName)) {
                    logging::logInfo("Routed " + ctx.path + " (model: " + model + ") to " + targetName + " via " + route.name);
                    return &targets_.at(targetName);
                }
            }
        }
    }
    
    return nullptr;
}

}
