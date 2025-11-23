#pragma once

#include <array>
#include <vector>

namespace audio {

// Lightweight plate-inspired reverb: two combs + two allpasses + predelay.
class SimplePlateReverb {
public:
    explicit SimplePlateReverb(float sampleRate = 48000.0f);

    void setParams(float preDelayMs, float decay, float damping);

    void process(std::vector<float> &buffer);

private:
    float sampleRate_;
    float decay_;
    float damping_;
    size_t preDelaySamples_;
    std::vector<float> preDelay_;
    size_t preDelayIdx_;

    struct DelayLine {
        std::vector<float> data;
        size_t idx{0};
        inline float read() const { return data.empty() ? 0.0f : data[idx]; }
        inline void write(float v) { if (!data.empty()) data[idx] = v; }
        inline void advance() { if (!data.empty()) idx = (idx + 1) % data.size(); }
    };

    std::array<DelayLine, 2> combs_;
    std::array<DelayLine, 2> allpasses_;
};

} // namespace audio
