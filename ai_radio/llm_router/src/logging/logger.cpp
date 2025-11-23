#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>

namespace logging {

void log(const std::string& level, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X") << "] [" << level << "] " << msg << std::endl;
}

void logInfo(const std::string& msg) {
    log("INFO", msg);
}

void logError(const std::string& msg) {
    log("ERROR", msg);
}

void logDebug(const std::string& msg) {
    log("DEBUG", msg);
}

}
