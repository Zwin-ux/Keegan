#pragma once

#include <string>
#include <vector>
#include <utility>
#include <mutex>

namespace util {

class Telemetry {
public:
    static Telemetry& instance();

    void init(const std::string& source);
    void record(const std::string& event,
                const std::vector<std::pair<std::string, std::string>>& fields = {});
    bool enabled() const;

private:
    Telemetry() = default;
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;

    std::string source_;
    std::string path_;
    bool enabled_ = false;
    mutable std::mutex mutex_;
};

} // namespace util
