#pragma once

#include <cmath>
#include <vector>
#include <numbers>

namespace audio {

class BiquadFilter {
public:
    enum Type {
        LowPass,
        HighPass,
        HighShelf
    };

    BiquadFilter(float sampleRate) 
        : sampleRate_(sampleRate), b0_(1), b1_(0), b2_(0), a1_(0), a2_(0),
          z1_(0), z2_(0) {}

    void setParams(Type type, float freq, float q, float gainDb = 0.0f) {
        float omega = 2.0f * std::numbers::pi_v<float> * freq / sampleRate_;
        float sn = std::sin(omega);
        float cs = std::cos(omega);
        float alpha = sn / (2.0f * q);
        float A = std::pow(10.0f, gainDb / 40.0f); // only for shelving
        float sqrtA = std::sqrt(A);

        float a0 = 1.0f;

        switch (type) {
            case LowPass:
                b0_ = (1.0f - cs) / 2.0f;
                b1_ = 1.0f - cs;
                b2_ = (1.0f - cs) / 2.0f;
                a0  = 1.0f + alpha;
                a1_ = -2.0f * cs;
                a2_ = 1.0f - alpha;
                break;
            case HighPass:
                b0_ = (1.0f + cs) / 2.0f;
                b1_ = -(1.0f + cs);
                b2_ = (1.0f + cs) / 2.0f;
                a0  = 1.0f + alpha;
                a1_ = -2.0f * cs;
                a2_ = 1.0f - alpha;
                break;
            case HighShelf:
                b0_ =    A * ( (A+1.0f) + (A-1.0f)*cs + 2.0f*sqrtA*alpha );
                b1_ = -2.0f * A * ( (A-1.0f) + (A+1.0f)*cs );
                b2_ =    A * ( (A+1.0f) + (A-1.0f)*cs - 2.0f*sqrtA*alpha );
                a0  =        (A+1.0f) - (A-1.0f)*cs + 2.0f*sqrtA*alpha;
                a1_ =  2.0f * ( (A-1.0f) - (A+1.0f)*cs );
                a2_ =        (A+1.0f) - (A-1.0f)*cs - 2.0f*sqrtA*alpha;
                break;
        }

        // Normalize
        b0_ /= a0;
        b1_ /= a0;
        b2_ /= a0;
        a1_ /= a0;
        a2_ /= a0;
    }

    void processBlock(std::vector<float>& buf) {
        for (float& s : buf) {
            float out = b0_ * s + b1_ * z1_ + b2_ * z2_ - a1_ * z1_ - a2_ * z2_;
            // Simple DF1? No, wait, this is not correct for IIR recurrence. 
            // Standard DF1: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
            // My variables z1/z2 store input history? Need to store x/y history properly or use DF2.
            // Let's use Transposed Direct Form II for numerical stability? 
            // Or just simple DF1 with 4 state vars.
            
            // Let's fix loop implementation to DF1
        }
        
        // Actually, let's rewrite properly for DF1
        for (float& s : buf) {
             float in = s;
             float out = b0_*in + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
             x2_ = x1_;
             x1_ = in;
             y2_ = y1_;
             y1_ = out;
             s = out;
        }
    }
    
private:
    float sampleRate_;
    // Coefficients
    float b0_, b1_, b2_, a1_, a2_;
    // State
    float x1_ = 0, x2_ = 0, y1_ = 0, y2_ = 0;
    float z1_ = 0, z2_ = 0; // Unused in DF1
};

} // namespace audio
