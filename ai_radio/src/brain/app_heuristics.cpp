#include "app_heuristics.h"

namespace brain {

AppHeuristics AppHeuristics::WithDefaults() {
    AppHeuristics h;
    h.addRule("code|devenv|idea", "focus_room", -0.05f);
    h.addRule("notepad|word|excel|chrome", "focus_room", 0.0f);
    h.addRule("unreal|unity|game|steam", "arcade_night", 0.15f);
    h.addRule("vlc|spotify|netflix|video", "sleep_ship", -0.1f);
    h.addRule("zoom|teams|meet", "rain_cave", -0.05f);
    return h;
}

void AppHeuristics::addRule(const std::string &regexPattern, const std::string &moodId, float energyBias) {
    rules_.push_back({std::regex(regexPattern, std::regex::icase), moodId, energyBias});
}

void AppHeuristics::setActiveProcess(const std::string &processName) {
    for (const auto &rule : rules_) {
        if (std::regex_search(processName, rule.pattern)) {
            currentBias_ = {rule.moodId, rule.energyBias};
            return;
        }
    }
    currentBias_ = {"focus_room", 0.0f}; // default bias
}

} // namespace brain
