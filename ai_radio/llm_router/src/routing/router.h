#pragma once
#include "config/loader.h"
#include "providers/base_client.h"
#include <memory>
#include <map>

namespace routing {

class Router {
public:
    Router(const config::Config& cfg);
    
    const config::Target* routeRequest(const providers::RequestContext& ctx);
    
    providers::BaseClient* getClient(config::TargetType type);

private:
    config::Config config_;
    std::map<std::string, config::Target> targets_;
    std::map<config::TargetType, std::unique_ptr<providers::BaseClient>> clients_;
    std::map<std::string, int> rr_counters_;
};

}
