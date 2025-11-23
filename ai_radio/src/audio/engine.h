#pragma once

#include <string>
#include <vector>
#include "reverb.h"
#include "ducking.h"
#include "crossfade.h"
#include "limiter.h"
#include "scheduler.h"
#include "../brain/state_machine.h"
#include "../brain/app_heuristics.h"

namespace audio {

class Engine {
public:
    Engine(float sampleRate = 48000.0f, size_t blockSize = 256);

    void setMoodPack(brain::MoodPack pack);
    void setIntensity(float value);

    // Active process name feeds heuristics to bias target mood/energy.
    void tick(const std::string &activeProcess, float dtSeconds);

    // Render one block into interleaved stereo output buffer.
    // Returns RMS of mixed output for telemetry/testing.
    float renderBlock(float *out, size_t frames);

private:
    float sampleRate_;
    size_t blockSize_;
    float intensity_;

    brain::MoodPack pack_;
    brain::MoodStateMachine machine_;
    brain::AppHeuristics heuristics_;

    Scheduler scheduler_;
    DuckingCompressor duck_;
    SimplePlateReverb reverb_;
    SoftLimiter limiter_;

    float musicPhase_;
    float voicePhase_;
    float narrativeTimer_;
    float narrativeInterval_;

    // Buffers reused per render
    std::vector<float> musicA_;
    std::vector<float> musicB_;
    std::vector<float> voice_;
    std::vector<float> mixed_;

    void generateMusic(const brain::MoodRecipe &recipe, float density, std::vector<float> &out, float &phase);
    void generateVoice(const brain::MoodRecipe &recipe, std::vector<float> &out, float &phase, float dt);
};

} // namespace audio
