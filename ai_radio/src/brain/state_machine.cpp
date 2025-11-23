#include "state_machine.h"
#include <algorithm>

namespace brain {

namespace {

MoodRecipe makeMood(const std::string &id,
                    const std::string &display,
                    float energy,
                    float tension,
                    float warmth,
                    float color,
                    std::vector<float> density,
                    std::vector<std::string> transitions) {
    MoodRecipe m;
    m.id = id;
    m.displayName = display;
    m.energy = energy;
    m.tension = tension;
    m.warmth = warmth;
    m.color = color;
    m.densityCurve = std::move(density);
    m.allowedTransitions = std::move(transitions);
    m.narrativeFrequency = 0.05f;
    m.synth = {"default", 0, 0.3f};
    return m;
}

} // namespace

MoodPack defaultMoodPack() {
    MoodPack pack;
    pack.moods.push_back(makeMood("focus_room", "Focus Room", 0.55f, 0.35f, 0.55f, 0.6f,
                                  {0.35f, 0.55f}, {"rain_cave", "arcade_night"}));
    pack.moods.push_back(makeMood("rain_cave", "Rain Cave", 0.35f, 0.25f, 0.45f, 0.3f,
                                  {0.25f, 0.4f, 0.25f}, {"focus_room", "sleep_ship"}));
    pack.moods.push_back(makeMood("arcade_night", "Arcade Night", 0.7f, 0.5f, 0.35f, 0.8f,
                                  {0.4f, 0.75f}, {"focus_room", "rain_cave"}));
    pack.moods.push_back(makeMood("sleep_ship", "Sleep Ship", 0.2f, 0.2f, 0.6f, 0.1f,
                                  {0.15f, 0.25f, 0.35f, 0.2f}, {"rain_cave"}));
    return pack;
}

MoodStateMachine::MoodStateMachine(MoodPack pack)
    : pack_(std::move(pack)),
      currentIndex_(0),
      targetIndex_(0),
      fadeProgress_(1.0f),
      fadeDuration_(8.0f) {}

std::optional<size_t> MoodStateMachine::findIndex(const std::string &id) const {
    for (size_t i = 0; i < pack_.moods.size(); ++i) {
        if (pack_.moods[i].id == id) return i;
    }
    return std::nullopt;
}

void MoodStateMachine::setTargetMood(const std::string &moodId) {
    auto idx = findIndex(moodId);
    if (!idx.has_value()) return;
    if (idx.value() == targetIndex_) return;
    // Only allow transition if permitted by current mood
    const auto &allowed = pack_.moods[currentIndex_].allowedTransitions;
    if (!allowed.empty() &&
        std::find(allowed.begin(), allowed.end(), moodId) == allowed.end()) {
        return;
    }
    targetIndex_ = idx.value();
    fadeProgress_ = 0.0f;
}

void MoodStateMachine::update(float dtSeconds) {
    if (currentIndex_ == targetIndex_) {
        fadeProgress_ = 1.0f;
        return;
    }
    fadeProgress_ += dtSeconds / fadeDuration_;
    if (fadeProgress_ >= 1.0f) {
        currentIndex_ = targetIndex_;
        fadeProgress_ = 1.0f;
    }
}

} // namespace brain
