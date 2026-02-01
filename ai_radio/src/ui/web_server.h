#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>
#include "../audio/engine.h"
#include "ws_server.h"

namespace uisrv {

struct StationConfig {
    std::string id;
    std::string name = "Keegan Station";
    std::string region = "us-midwest";
    float frequency = 98.7f;
    std::string description = "Local vibe engine";
    std::string streamUrl;
    std::string registryUrl = "http://localhost:8090";
};

class WebServer {
public:
    WebServer(audio::Engine& engine, int port = 3000);
    ~WebServer();

    bool start();
    void stop();

private:
    audio::Engine& engine_;
    int port_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    std::thread registryThread_;
    StationConfig stationConfig_;
    std::string stationId_;
    std::unique_ptr<WsServer> wsServer_;
    std::mutex stationMutex_;
    std::mutex broadcastMutex_;
    bool broadcasting_ = false;
    uint64_t broadcastStartedMs_ = 0;
    uint64_t broadcastUpdatedMs_ = 0;
    std::string broadcastSessionId_;
    uint64_t broadcastTokenExpiryMs_ = 0;
    std::string bridgeApiKey_;
    std::string registryApiKey_;
    std::string broadcastSecret_;
    
    void run();
    void loadStationConfig();
    void runRegistryClient();
};

} // namespace uisrv
