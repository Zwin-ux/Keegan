#pragma once

#include <cmath>
#include <numbers>

namespace audio {

class Oscillator {
public:
    Oscillator(float sampleRate) : sampleRate_(sampleRate), phase_(0.0f), freq_(440.0f) {}

    void setFrequency(float freq) {
        freq_ = freq;
    }

    // Process one sample
    float process() {
        // Basic sine wave
        float val = std::sin(phase_);
        
        // Advance phase
        float delta = 2.0f * std::numbers::pi_v<float> * freq_ / sampleRate_;
        phase_ += delta;
        if (phase_ > 2.0f * std::numbers::pi_v<float>) {
            phase_ -= 2.0f * std::numbers::pi_v<float>;
        }
        
        return val;
    }
    
    // Process block
    void processBlock(float* out, size_t frames, float gain) {
        float delta = 2.0f * std::numbers::pi_v<float> * freq_ / sampleRate_;
        for (size_t i = 0; i < frames; ++i) {
            out[i] += std::sin(phase_) * gain;
            phase_ += delta;
            if (phase_ > 2.0f * std::numbers::pi_v<float>) {
                phase_ -= 2.0f * std::numbers::pi_v<float>;
            }
        }
    }

private:
    float sampleRate_;
    float phase_;
    float freq_;
};

} // namespace audio
