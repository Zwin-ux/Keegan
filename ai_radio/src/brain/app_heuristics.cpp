#include "app_heuristics.h"
#include "../util/logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

#include <algorithm>
#include <chrono>

namespace brain {

// --- AppHeuristics ---

AppHeuristics AppHeuristics::WithDefaults() {
    AppHeuristics h;
    // IDEs and code editors -> Focus
    h.addRule("code\\.exe|devenv\\.exe|idea64\\.exe|sublime_text\\.exe|atom\\.exe", "focus_room", -0.05f);
    // Productivity apps -> Focus
    h.addRule("notepad.*\\.exe|word\\.exe|excel\\.exe|winword\\.exe|powerpnt\\.exe", "focus_room", 0.0f);
    // Browsers can be anything, slight focus bias
    h.addRule("chrome\\.exe|firefox\\.exe|msedge\\.exe|brave\\.exe", "focus_room", 0.0f);
    // Games and creative -> Arcade
    h.addRule(".*game.*\\.exe|steam\\.exe|unrealengine|unity\\.exe|blender\\.exe", "arcade_night", 0.15f);
    // Media players -> Sleep/Rain
    h.addRule("vlc\\.exe|spotify\\.exe|netflix|wmplayer\\.exe|musicbee\\.exe", "sleep_ship", -0.1f);
    // Video calls -> Rain (calm focus)
    h.addRule("zoom\\.exe|teams\\.exe|slack\\.exe|discord\\.exe", "rain_cave", -0.05f);
    // File explorer / idle -> Sleep
    h.addRule("explorer\\.exe", "sleep_ship", -0.15f);
    return h;
}

void AppHeuristics::addRule(const std::string &regexPattern, const std::string &moodId, float energyBias) {
    try {
        rules_.push_back({std::regex(regexPattern, std::regex::icase), moodId, energyBias});
    } catch (const std::regex_error& e) {
        util::logError("AppHeuristics: Invalid regex pattern: " + regexPattern);
    }
}

void AppHeuristics::update() {
    std::string process = detectActiveProcess();
    if (!process.empty() && process != activeProcess_) {
        activeProcess_ = process;
        setActiveProcess(process);
    }
}

void AppHeuristics::setActiveProcess(const std::string &processName) {
    activeProcess_ = processName;
    
    for (const auto &rule : rules_) {
        try {
            if (std::regex_search(processName, rule.pattern)) {
                currentBias_ = {rule.moodId, rule.energyBias};
                return;
            }
        } catch (...) {
            // Ignore regex errors during matching
        }
    }
    currentBias_ = {"focus_room", 0.0f}; // default bias
}

std::string AppHeuristics::detectActiveProcess() {
#ifdef _WIN32
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return "";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return "";

    WCHAR path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    BOOL success = QueryFullProcessImageNameW(hProcess, 0, path, &size);
    CloseHandle(hProcess);

    if (!success) return "";

    // Extract just the filename
    std::wstring fullPath(path);
    size_t pos = fullPath.rfind(L'\\');
    if (pos != std::wstring::npos) {
        fullPath = fullPath.substr(pos + 1);
    }

    // Convert to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return "";

    std::string result(utf8Len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, &result[0], utf8Len, nullptr, nullptr);

    return result;
#else
    // Linux/macOS: could use xdotool, wmctrl, or similar
    return "";
#endif
}

// --- ActivityMonitor ---

ActivityMonitor::ActivityMonitor() {
    lastInputTick_ = getLastInputTime();
}

void ActivityMonitor::update(float dtSeconds) {
    uint64_t currentInput = getLastInputTime();
    
    // Calculate idle time in seconds
    uint64_t now = 0;
#ifdef _WIN32
    now = GetTickCount64();
#else
    now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif

    if (currentInput > lastInputTick_) {
        // There was input since last check
        idleSeconds_ = 0.0f;
        lastInputTick_ = currentInput;
    } else {
        idleSeconds_ += dtSeconds;
    }

    // Calculate activity based on idle time
    // 0 seconds idle = 1.0 activity
    // 30+ seconds idle = 0.0 activity
    float targetActivity = std::max(0.0f, 1.0f - (idleSeconds_ / 30.0f));

    // Smooth the activity change
    const float smoothing = 0.1f; // Lower = smoother
    smoothedActivity_ += (targetActivity - smoothedActivity_) * smoothing;
    smoothedActivity_ = std::clamp(smoothedActivity_, 0.0f, 1.0f);
}

uint64_t ActivityMonitor::getLastInputTime() {
#ifdef _WIN32
    LASTINPUTINFO lii = {};
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&lii)) {
        return static_cast<uint64_t>(lii.dwTime);
    }
    return 0;
#else
    // Linux/macOS: would need XScreenSaver or similar
    return 0;
#endif
}

} // namespace brain
