#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace util {

namespace {
std::string nowString() {
    using clock = std::chrono::system_clock;
    const auto t = clock::to_time_t(clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << tm.tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm.tm_sec;
    return oss.str();
}
} // namespace

void log(LogLevel level, const std::string &msg) {
    const char *tag = "";
    switch (level) {
    case LogLevel::Info: tag = "[info]"; break;
    case LogLevel::Warn: tag = "[warn]"; break;
    case LogLevel::Error: tag = "[error]"; break;
    }
    
    std::ostringstream oss;
    oss << nowString() << " " << tag << " " << msg << "\n";
    std::string output = oss.str();

#ifdef _WIN32
    // Output to debugger (visible in Visual Studio Output window)
    OutputDebugStringA(output.c_str());
    
    // Also write to a log file for non-debug builds
    static FILE* logFile = nullptr;
    if (!logFile) {
        char path[MAX_PATH];
        if (GetEnvironmentVariableA("APPDATA", path, MAX_PATH) > 0) {
            std::string logPath = std::string(path) + "\\Keegan\\keegan.log";
            // Create directory if needed
            CreateDirectoryA((std::string(path) + "\\Keegan").c_str(), nullptr);
            logFile = fopen(logPath.c_str(), "a");
        }
    }
    if (logFile) {
        fputs(output.c_str(), logFile);
        fflush(logFile);
    }
#else
    // Console output for non-Windows
    fputs(output.c_str(), stdout);
#endif
}

} // namespace util
