#pragma once

#include <algorithm>
#include <vector>
#include <cmath>

namespace audio {

// Equal-power crossfade helper for mono buffers.
inline void equalPowerCrossfade(const std::vector<float> &a,
                                const std::vector<float> &b,
                                float t,
                                std::vector<float> &out) {
    constexpr float kPi = 3.1415926535f;
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float gainA = std::cos(0.5f * kPi * clamped);
    const float gainB = std::sin(0.5f * kPi * clamped);
    const size_t frames = std::min(a.size(), b.size());
    out.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        out[i] = a[i] * gainA + b[i] * gainB;
    }
}

} // namespace audio
