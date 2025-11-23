#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

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
    std::cout << nowString() << " " << tag << " " << msg << "\n";
}

} // namespace util
