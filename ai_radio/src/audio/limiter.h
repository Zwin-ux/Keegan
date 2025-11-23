#pragma once

#include <vector>

namespace audio {

// Simple soft limiter with fixed ceiling.
class SoftLimiter {
public:
    explicit SoftLimiter(float ceilingDb = -1.0f, float softness = 0.1f)
        : ceilingDb_(ceilingDb), softness_(softness) {}

    void setParams(float ceilingDb, float softness);
    void process(std::vector<float> &buffer);

private:
    float ceilingDb_;
    float softness_;
};

} // namespace audio
