#include "ducking.h"
#include <algorithm>
#include <cmath>

namespace audio {

namespace {
inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}
}

void DuckingCompressor::setParams(float attackMs, float releaseMs, float ratio, float thresholdDb) {
    attackMs_ = attackMs;
    releaseMs_ = releaseMs;
    ratio_ = ratio;
    thresholdDb_ = thresholdDb;
}

void DuckingCompressor::process(const std::vector<float> &sidechain,
                                std::vector<float> &target,
                                float sampleRate) {
    if (target.empty()) return;
    const float attackCoeff = std::exp(-1.0f / (0.001f * attackMs_ * sampleRate));
    const float releaseCoeff = std::exp(-1.0f / (0.001f * releaseMs_ * sampleRate));
    const float thresholdLin = dbToLinear(thresholdDb_);

    for (size_t i = 0; i < target.size(); ++i) {
        const float sc = i < sidechain.size() ? sidechain[i] : 0.0f;
        const float scSq = sc * sc;
        if (scSq > envelopeRms_) {
            envelopeRms_ = attackCoeff * (envelopeRms_ - scSq) + scSq;
        } else {
            envelopeRms_ = releaseCoeff * (envelopeRms_ - scSq) + scSq;
        }
        const float rms = std::sqrt(std::max(0.0f, envelopeRms_));

        float gain = 1.0f;
        if (rms > thresholdLin) {
            float over = rms / thresholdLin;
            float gainDb = - (over - 1.0f) * (ratio_ - 1.0f) * 6.0f; // gentle slope
            gain = dbToLinear(gainDb);
        }
        target[i] *= gain;
    }
}

} // namespace audio
