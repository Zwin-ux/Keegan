#include "limiter.h"
#include <cmath>

namespace audio {

namespace {
inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}
}

void SoftLimiter::setParams(float ceilingDb, float softness) {
    ceilingDb_ = ceilingDb;
    softness_ = softness;
}

void SoftLimiter::process(std::vector<float> &buffer) {
    const float ceiling = dbToLinear(ceilingDb_);
    const float knee = softness_;
    for (auto &sample : buffer) {
        const float absSample = std::fabs(sample);
        if (absSample <= ceiling) continue;
        const float over = absSample - ceiling;
        const float t = over / (over + knee);
        const float limited = ceiling + t * knee;
        sample = (sample >= 0.0f) ? limited : -limited;
    }
}

} // namespace audio
