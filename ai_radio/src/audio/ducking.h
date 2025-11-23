#pragma once

#include <vector>

namespace audio {

// Simple sidechain ducking compressor for mono buffers (RMS detector).
class DuckingCompressor {
public:
    DuckingCompressor(float attackMs = 15.0f,
                      float releaseMs = 350.0f,
                      float ratio = 2.5f,
                      float thresholdDb = -18.0f)
        : attackMs_(attackMs),
          releaseMs_(releaseMs),
          ratio_(ratio),
          thresholdDb_(thresholdDb),
          envelopeRms_(0.0f) {}

    void setParams(float attackMs, float releaseMs, float ratio, float thresholdDb);

    // sidechain = voice/TTS buffer, target = music buffer (in-place gain)
    void process(const std::vector<float> &sidechain,
                 std::vector<float> &target,
                 float sampleRate);

private:
    float attackMs_;
    float releaseMs_;
    float ratio_;
    float thresholdDb_;
    float envelopeRms_;
};

} // namespace audio
