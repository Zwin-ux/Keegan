#pragma once

#include <string>
#include <vector>
#include <optional>

namespace brain {

struct StemConfig {
    std::string file;
    std::string role;
    float gainDb{0.0f};
    bool loop{true};
    float probability{1.0f};
};

struct SynthPreset {
    std::string presetFile;
    int seed{0};
    float patternDensity{0.5f};
};

struct MoodRecipe {
    std::string id;
    std::string displayName;
    std::vector<StemConfig> stems;
    SynthPreset synth;
    std::vector<float> densityCurve;
    float narrativeFrequency{0.05f};
    std::vector<std::string> allowedTransitions;
    float color{0.0f};
    float warmth{0.0f};
    float tension{0.0f};
    float energy{0.0f};
};

struct MoodPack {
    std::vector<MoodRecipe> moods;
};

MoodPack defaultMoodPack();

class MoodStateMachine {
public:
    explicit MoodStateMachine(MoodPack pack);

    void setTargetMood(const std::string &moodId);
    void update(float dtSeconds);

    const MoodRecipe &currentRecipe() const { return pack_.moods[currentIndex_]; }
    const MoodRecipe &targetRecipe() const { return pack_.moods[targetIndex_]; }
    float crossfade() const { return fadeProgress_; }

private:
    MoodPack pack_;
    size_t currentIndex_;
    size_t targetIndex_;
    float fadeProgress_;
    float fadeDuration_;

    std::optional<size_t> findIndex(const std::string &id) const;
};

} // namespace brain
