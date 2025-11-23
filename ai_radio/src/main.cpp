#include "audio/engine.h"
#include "audio/device.h"
#include "config/mood_loader.h"
#include "util/logger.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    bool loaded = false;
    auto pack = config::MoodLoader::loadFromFile("config/moods.json", loaded);

    audio::Engine engine(48000.0f, 512);
    engine.setMoodPack(pack);
    engine.setIntensity(0.75f);

    audio::AudioDevice device(engine, 48000, 512);
    if (!device.init()) {
        util::logError("Audio init failed.");
        return 1;
    }

    if (!device.start()) {
        util::logError("Audio start failed.");
        return 1;
    }

    util::logInfo(loaded ? "Loaded mood pack from config/moods.json"
                         : "Using default embedded mood pack");
    util::logInfo("Keegan audio running. Press Enter to quit.");

    std::atomic<bool> running{true};

    // Simple tick loop to drive heuristics
    std::thread tickThread([&]() {
        const std::string proc = "idle";
        const float dt = 0.1f;
        while (running.load()) {
            engine.tick(proc, dt);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Keep process alive until user input
    std::cin.get();

    running.store(false);
    if (tickThread.joinable()) tickThread.join();
    device.stop();
    device.shutdown();
    return 0;
}
