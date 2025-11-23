#pragma once

#include <cstddef>
#include <vector>
#include "../brain/state_machine.h"

namespace audio {

// Minimal lookahead scheduler that outputs a density scalar per block.
class Scheduler {
public:
    Scheduler(float sampleRate, float lookaheadMs = 50.0f)
        : sampleRate_(sampleRate),
          lookaheadSamples_(static_cast<size_t>(lookaheadMs * 0.001f * sampleRate)),
          phase_(0.0f) {}

    void setMood(const brain::MoodRecipe &mood);

    // Advance time and return a density multiplier [0..1] for the next block.
    float nextDensity(size_t blockSize);

private:
    float sampleRate_;
    size_t lookaheadSamples_;
    float phase_;
    float tempoHz_{1.0f};
    float baseDensity_{0.5f};
};

} // namespace audio
