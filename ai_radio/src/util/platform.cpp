#include "platform.h"
#include "logger.h"
#include <filesystem>
#include <Windows.h>

namespace util {

bool fixWorkingDirectory() {
    namespace fs = std::filesystem;

    // Check if config exists in current CWD
    if (fs::exists("config/moods.json")) {
        util::logInfo("CWD is correct: config/moods.json found.");
        return true;
    }

    // Get executable path
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    fs::path exePath(buffer);
    fs::path exeDir = exePath.parent_path();

    util::logInfo("Exe dir: " + exeDir.string());

    // Check standard build layout:
    // build/Release/keegan.exe -> project root is 2 levels up
    fs::path rootPath = exeDir.parent_path().parent_path();
    fs::path configPath = rootPath / "config/moods.json";

    if (fs::exists(configPath)) {
        std::error_code ec;
        fs::current_path(rootPath, ec);
        if (!ec) {
            util::logInfo("Changed CWD to project root: " + rootPath.string());
            return true;
        }
    }
    
    // Check if we are in build/ (1 level up)
    rootPath = exeDir.parent_path();
    configPath = rootPath / "config/moods.json";
    if (fs::exists(configPath)) {
         std::error_code ec;
        fs::current_path(rootPath, ec);
        if (!ec) {
            util::logInfo("Changed CWD to parent: " + rootPath.string());
            return true;
        }
    }

    util::logError("Could not locate project root with config/moods.json");
    return false;
}

} // namespace util
