#include "audio/engine.h"
#include "audio/device.h"
#include "config/mood_loader.h"
#include "ui/tray.h"
#include "ui/web_server.h"
#include "util/logger.h"
#include "util/platform.h"
#include "util/telemetry.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// Global engine pointer for callbacks
static audio::Engine* g_engine = nullptr;
static audio::AudioDevice* g_device = nullptr;

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
#else
int main() {
#endif
    util::fixWorkingDirectory();
    util::logInfo("Keegan starting up...");
    util::Telemetry::instance().init("exe");

    // Load mood configuration
    bool loaded = false;
    auto pack = config::MoodLoader::loadFromFile("config/moods.json", loaded);

    // Initialize audio engine
    audio::Engine engine(48000.0f, 512);
    engine.setMoodPack(pack);
    engine.setIntensity(0.75f);
    g_engine = &engine;
    util::Telemetry::instance().record("engine_start", {
        {"mood", engine.currentMoodId()}
    });

    // Start Web Server
    uisrv::WebServer server(engine, 3000);
    server.start();

    // Initialize audio device
    audio::AudioDevice device(engine, 48000, 512);
    if (!device.init()) {
        util::logError("Audio init failed.");
        return 1;
    }
    g_device = &device;

    if (!device.start()) {
        util::logError("Audio start failed.");
        return 1;
    }

    util::logInfo(loaded ? "Loaded mood pack from config/moods.json"
                         : "Using default embedded mood pack");

#ifdef _WIN32
    // Initialize system tray
    ui::TrayController tray;
    if (!tray.init(hInstance)) {
        util::logError("Tray init failed, falling back to console mode.");
        
        // Console fallback
        util::logInfo("Keegan audio running. Press Enter to quit.");
        std::atomic<bool> running{true};
        std::thread tickThread([&]() {
            while (running.load()) {
                engine.tick("", 0.1f);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        std::cin.get();
        running.store(false);
        if (tickThread.joinable()) tickThread.join();
    } else {
        // Set up tray callbacks
        tray.setOnMoodSelect([](ui::MoodId mood) {
            if (g_engine) {
                std::string moodStr = ui::moodIdToString(mood);
                g_engine->setMood(moodStr);
                util::logInfo("Mood changed to: " + moodStr);
                util::Telemetry::instance().record("mood_change", {
                    {"mood", moodStr}
                });
            }
        });

        tray.setOnPlayPause([]() {
            if (g_engine) {
                bool newState = !g_engine->isPlaying();
                g_engine->setPlaying(newState);
                util::logInfo(newState ? "Playback resumed" : "Playback paused");
                util::Telemetry::instance().record(newState ? "playback_start" : "playback_stop");
            }
        });

        tray.setOnQuit([]() {
            util::logInfo("Quit requested from tray");
        });

        // Start with playing state
        engine.setPlaying(true);
        tray.setPlaying(true);
        tray.setTooltip("Keegan - " + engine.currentMoodId());
        tray.show();

        util::logInfo("Keegan audio running in system tray.");

        // Background thread for engine tick and heuristics update
        std::atomic<bool> running{true};
        std::thread tickThread([&]() {
            brain::AppHeuristics heuristics = brain::AppHeuristics::WithDefaults();
            std::string lastProcess;
            
            while (running.load()) {
                // Update heuristics with real active window
                heuristics.update();
                std::string activeProcess = heuristics.activeProcess();
                if (activeProcess != lastProcess) {
                    lastProcess = activeProcess;
                    if (!activeProcess.empty()) {
                        util::Telemetry::instance().record("app_focus_change", {
                            {"process", activeProcess}
                        });
                    }
                }
                
                engine.tick(activeProcess, 0.1f);
                
                // Update tray energy indicator
                tray.setEnergy(engine.currentEnergy());
                
                // Update tooltip with current state
                std::string tooltip = "Keegan - " + engine.currentMoodId();
                if (!activeProcess.empty()) {
                    tooltip += " (" + activeProcess + ")";
                }
                tray.setTooltip(tooltip);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        // Run the Windows message loop (blocking)
        tray.runMessageLoop();

        // Cleanup
        running.store(false);
        if (tickThread.joinable()) tickThread.join();
        tray.hide();
    }
#else
    // Non-Windows: console mode
    util::logInfo("Keegan audio running. Press Enter to quit.");
    std::atomic<bool> running{true};
    std::thread tickThread([&]() {
        while (running.load()) {
            engine.tick("", 0.1f);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    std::cin.get();
    running.store(false);
    if (tickThread.joinable()) tickThread.join();
#endif

    device.stop();
    device.shutdown();
    util::logInfo("Keegan shutdown complete.");
    util::Telemetry::instance().record("engine_shutdown");
    return 0;
}
