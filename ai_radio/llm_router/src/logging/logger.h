#pragma once
#include <string>

namespace logging {

void logInfo(const std::string& msg);
void logError(const std::string& msg);
void logDebug(const std::string& msg);

}
