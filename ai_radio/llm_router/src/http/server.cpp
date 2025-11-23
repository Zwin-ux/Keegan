#include "server.h"
#include "logging/logger.h"
#include <chrono>

namespace http {

Server::Server(routing::Router& router, int port) : router_(router), port_(port) {
    svr_.Post(".*", [this](const httplib::Request& req, httplib::Response& res) {
        logging::logInfo("Received request: " + req.path);
        auto start = std::chrono::steady_clock::now();
        
        providers::RequestContext ctx;
        ctx.path = req.path;
        ctx.body = req.body;
        ctx.method = req.method;
        
        const config::Target* target = router_.routeRequest(ctx);
        if (!target) {
            res.status = 404;
            res.set_content("{\"error\": \"No route found\"}", "application/json");
            logging::logError("No route found for " + req.path);
            return;
        }
        
        providers::BaseClient* client = router_.getClient(target->type);
        if (!client) {
            res.status = 500;
            res.set_content("{\"error\": \"Provider client not implemented\"}", "application/json");
            logging::logError("Client not found for target type");
            return;
        }
        
        providers::Response providerRes = client->sendRequest(*target, ctx);
        
        res.status = providerRes.status;
        res.set_content(providerRes.body, providerRes.contentType.c_str());
        
        auto end = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        logging::logInfo("Request completed in " + std::to_string(diff) + "ms");
    });
}

void Server::start() {
    logging::logInfo("Starting server on port " + std::to_string(port_));
    if (!svr_.listen("0.0.0.0", port_)) {
        logging::logError("Failed to listen on port " + std::to_string(port_));
    }
}

}
