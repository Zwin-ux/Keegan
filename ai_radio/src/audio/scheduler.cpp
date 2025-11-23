#include "scheduler.h"
#include <cmath>
#include <algorithm>

namespace audio {

namespace {
constexpr float kPi = 3.1415926535f;
}

void Scheduler::setMood(const brain::MoodRecipe &mood) {
    // Derive tempo from energy if not specified; clamp to sensible range.
    float bpm = 40.0f + mood.energy * 80.0f;
    tempoHz_ = std::clamp(bpm / 60.0f, 0.5f, 4.0f);
    // Use last density point as base; fallback to 0.4.
    if (!mood.densityCurve.empty()) {
        baseDensity_ = std::clamp(mood.densityCurve.back(), 0.05f, 1.0f);
    } else {
        baseDensity_ = 0.4f;
    }
}

float Scheduler::nextDensity(size_t blockSize) {
    // Simple LFO wobble around base density, respecting lookahead.
    float dt = static_cast<float>(blockSize) / sampleRate_;
    phase_ += dt * tempoHz_;
    if (phase_ > 1.0f) phase_ -= 1.0f;
    float wobble = 0.05f * std::sin(2.0f * kPi * phase_);
    float density = std::clamp(baseDensity_ + wobble, 0.05f, 1.0f);
    (void)lookaheadSamples_; // reserved for future event emission
    return density;
}

} // namespace audio
