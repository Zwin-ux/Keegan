#pragma once

#include <string>

namespace util {

enum class LogLevel { Info, Warn, Error };

void log(LogLevel level, const std::string &msg);
inline void logInfo(const std::string &msg) { log(LogLevel::Info, msg); }
inline void logWarn(const std::string &msg) { log(LogLevel::Warn, msg); }
inline void logError(const std::string &msg) { log(LogLevel::Error, msg); }

} // namespace util
