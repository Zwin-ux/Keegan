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

    void setActiveProcess(const std::string &processName);

    MoodBias currentBias() const { return currentBias_; }

private:
    std::vector<HeuristicRule> rules_;
    MoodBias currentBias_;
};

} // namespace brain
