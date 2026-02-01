#pragma once

#include <regex>
#include <string>
#include <vector>

namespace brain {

struct MoodBias {
    std::string moodId;
    float energyBias{0.0f};
};

struct HeuristicRule {
    std::regex pattern;
    std::string moodId;
    float energyBias{0.0f};
};

class AppHeuristics {
public:
    static AppHeuristics WithDefaults();

    void addRule(const std::string &regexPattern, const std::string &moodId, float energyBias);

    // Poll and update the active foreground process.
    // Call this periodically (e.g., every 1-5 seconds).
    void update();

    // Manual override (for testing or when window detection fails).
    void setActiveProcess(const std::string &processName);

    // Get current detected process name.
    const std::string& activeProcess() const { return activeProcess_; }

    MoodBias currentBias() const { return currentBias_; }

private:
    std::vector<HeuristicRule> rules_;
    MoodBias currentBias_;
    std::string activeProcess_;

    // Platform-specific active window detection.
    std::string detectActiveProcess();
};

// Tracks keyboard/mouse activity to modulate energy.
class ActivityMonitor {
public:
    ActivityMonitor();

    // Call periodically to update activity state.
    void update(float dtSeconds);

    // Get smoothed activity level (0.0 = idle, 1.0 = intense activity).
    float activity() const { return smoothedActivity_; }

    // Get seconds since last input.
    float idleTime() const { return idleSeconds_; }

private:
    float smoothedActivity_ = 0.0f;
    float idleSeconds_ = 0.0f;
    uint64_t lastInputTick_ = 0;

    // Platform-specific input detection.
    uint64_t getLastInputTime();
};

} // namespace brain
