#include "telemetry.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace util {

namespace {
bool isEnabledFlag(const std::string& value) {
    if (value.empty()) return false;
    std::string lower = value;
    for (char& c : lower) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return lower == "1" || lower == "true" || lower == "yes" || lower == "on";
}

std::string getEnv(const char* key) {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string();
}

uint64_t nowMs() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string escapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}
} // namespace

Telemetry& Telemetry::instance() {
    static Telemetry instance;
    return instance;
}

void Telemetry::init(const std::string& source) {
    source_ = source;
    enabled_ = isEnabledFlag(getEnv("KEEGAN_TELEMETRY"));
    path_ = getEnv("KEEGAN_TELEMETRY_FILE");
    if (path_.empty()) {
        path_ = "cache/telemetry.jsonl";
    }
    if (!enabled_) {
        return;
    }
    std::filesystem::path p(path_);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
}

bool Telemetry::enabled() const {
    return enabled_;
}

void Telemetry::record(const std::string& event,
                       const std::vector<std::pair<std::string, std::string>>& fields) {
    if (!enabled_) return;

    std::stringstream ss;
    ss << "{";
    ss << "\"event\":\"" << escapeJson(event) << "\",";
    ss << "\"ts\":" << nowMs();
    if (!source_.empty()) {
        ss << ",\"source\":\"" << escapeJson(source_) << "\"";
    }
    for (const auto& entry : fields) {
        ss << ",\"" << escapeJson(entry.first) << "\":\"" << escapeJson(entry.second) << "\"";
    }
    ss << "}\n";

    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(path_, std::ios::app | std::ios::binary);
    if (out.good()) {
        out << ss.str();
    }
}

} // namespace util
