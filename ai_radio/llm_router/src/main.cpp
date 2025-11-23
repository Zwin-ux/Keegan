#include "config/loader.h"
#include "routing/router.h"
#include "http/server.h"
#include "logging/logger.h"
#include <iostream>

int main() {
    try {
        logging::logInfo("Loading config...");
        auto cfg = config::ConfigLoader::load("config/router.json");
        
        logging::logInfo("Initializing router...");
        routing::Router router(cfg);
        
        logging::logInfo("Starting HTTP server...");
        http::Server server(router, cfg.server.port);
        server.start();
    } catch (const std::exception& e) {
        logging::logError(std::string("Fatal error: ") + e.what());
        return 1;
    }
    return 0;
}
