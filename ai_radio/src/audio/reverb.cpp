#include "reverb.h"
#include <cmath>
#include <algorithm>

namespace audio {

namespace {
constexpr float kPi = 3.1415926535f;
}

SimplePlateReverb::SimplePlateReverb(float sampleRate)
    : sampleRate_(sampleRate),
      decay_(0.5f),
      damping_(0.25f),
      preDelaySamples_(static_cast<size_t>(0.02f * sampleRate)),
      preDelay_(preDelaySamples_, 0.0f),
      preDelayIdx_(0) {
    const std::array<size_t, 2> combSizes = {
        static_cast<size_t>(0.0297f * sampleRate_),
        static_cast<size_t>(0.0371f * sampleRate_)};
    const std::array<size_t, 2> allpassSizes = {
        static_cast<size_t>(0.005f * sampleRate_),
        static_cast<size_t>(0.0017f * sampleRate_)};

    for (size_t i = 0; i < combs_.size(); ++i) {
        combs_[i].data.assign(combSizes[i], 0.0f);
    }
    for (size_t i = 0; i < allpasses_.size(); ++i) {
        allpasses_[i].data.assign(allpassSizes[i], 0.0f);
    }
}

void SimplePlateReverb::setParams(float preDelayMs, float decay, float damping) {
    decay_ = std::clamp(decay, 0.05f, 0.95f);
    damping_ = std::clamp(damping, 0.0f, 0.9f);
    preDelaySamples_ = static_cast<size_t>((preDelayMs / 1000.0f) * sampleRate_);
    preDelay_.assign(std::max<size_t>(1, preDelaySamples_), 0.0f);
    preDelayIdx_ = 0;
}

void SimplePlateReverb::process(std::vector<float> &buffer, float wetMix) {
    if (buffer.empty()) return;
    
    // Clamp wetMix to valid range
    wetMix = std::clamp(wetMix, 0.0f, 1.0f);
    
    std::vector<float> wet(buffer.size(), 0.0f);

    for (size_t n = 0; n < buffer.size(); ++n) {
        // Predelay tap
        const float preOut = preDelay_.empty() ? buffer[n] : preDelay_[preDelayIdx_];
        if (!preDelay_.empty()) {
            preDelay_[preDelayIdx_] = buffer[n];
            preDelayIdx_ = (preDelayIdx_ + 1) % preDelay_.size();
        }

        // Comb filters in parallel
        float combSum = 0.0f;
        for (auto &comb : combs_) {
            float delayed = comb.read();
            float feedback = preOut + delayed * decay_;
            comb.write(feedback);
            comb.advance();
            // simple one-pole damping
            delayed = delayed * (1.0f - damping_) + feedback * damping_;
            combSum += delayed;
        }
        combSum *= 0.5f; // average

        // Allpasses in series for diffusion
        float apOut = combSum;
        for (auto &ap : allpasses_) {
            float bufOut = ap.read();
            float input = apOut + (-0.5f * bufOut);
            ap.write(input);
            ap.advance();
            apOut = bufOut + (0.5f * input);
        }

        wet[n] = apOut;
    }

    // Mix dry and wet signals
    const float dryMix = 1.0f - wetMix;
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = buffer[i] * dryMix + wet[i] * wetMix;
    }
}

} // namespace audio
