#include "engine.h"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace audio {

namespace {
constexpr float kPi = 3.1415926535f;

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

float rms(const std::vector<float> &buf) {
    if (buf.empty()) return 0.0f;
    float sum = 0.0f;
    for (float v : buf) sum += v * v;
    return std::sqrt(sum / static_cast<float>(buf.size()));
}
} // namespace

Engine::Engine(float sampleRate, size_t blockSize)
    : sampleRate_(sampleRate),
      blockSize_(blockSize),
      intensity_(0.7f),
      pack_(brain::defaultMoodPack()),
      machine_(pack_),
      heuristics_(brain::AppHeuristics::WithDefaults()),
      scheduler_(sampleRate),
      reverb_(sampleRate),
      limiter_(-1.0f, 0.05f),
      musicPhase_(0.0f),
      voicePhase_(0.0f),
      narrativeTimer_(0.0f),
      narrativeInterval_(20.0f) {
    musicA_.resize(blockSize_);
    musicB_.resize(blockSize_);
    voice_.resize(blockSize_);
    mixed_.resize(blockSize_);
}

void Engine::setMoodPack(brain::MoodPack pack) {
    pack_ = std::move(pack);
    machine_ = brain::MoodStateMachine(pack_);
}

void Engine::setIntensity(float value) {
    intensity_ = clamp01(value);
}

void Engine::tick(const std::string &activeProcess, float dtSeconds) {
    heuristics_.setActiveProcess(activeProcess);
    const auto bias = heuristics_.currentBias();
    // If heuristic suggests a different mood, try to transition.
    machine_.setTargetMood(bias.moodId);
    machine_.update(dtSeconds);
}

void Engine::generateMusic(const brain::MoodRecipe &recipe, float density, std::vector<float> &out, float &phase) {
    const float freq = 110.0f + 220.0f * recipe.energy * intensity_;
    const float amp = 0.2f + 0.3f * density;
    for (size_t i = 0; i < blockSize_; ++i) {
        float v = std::sin(phase) * amp;
        // subtle tension adds a second harmonic
        v += std::sin(phase * 2.0f) * recipe.tension * 0.1f;
        out[i] = v;
        phase += 2.0f * kPi * freq / sampleRate_;
        if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
    }
}

void Engine::generateVoice(const brain::MoodRecipe &recipe, std::vector<float> &out, float &phase, float dt) {
    std::fill(out.begin(), out.end(), 0.0f);
    narrativeTimer_ += dt;
    if (narrativeTimer_ < narrativeInterval_) return;
    narrativeTimer_ = 0.0f;
    // simple blip envelope
    const float freq = 440.0f;
    for (size_t i = 0; i < blockSize_; ++i) {
        float env = 1.0f - static_cast<float>(i) / static_cast<float>(blockSize_);
        out[i] = std::sin(phase) * 0.3f * env;
        phase += 2.0f * kPi * freq / sampleRate_;
        if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
    }
}

float Engine::renderBlock(float *out, size_t frames) {
    if (frames == 0 || out == nullptr) return 0.0f;
    blockSize_ = frames;
    musicA_.resize(frames);
    musicB_.resize(frames);
    voice_.resize(frames);
    mixed_.resize(frames);

    const auto &cur = machine_.currentRecipe();
    const auto &tgt = machine_.targetRecipe();

    scheduler_.setMood(cur);
    const float densityCur = scheduler_.nextDensity(blockSize_);
    scheduler_.setMood(tgt);
    const float densityTgt = scheduler_.nextDensity(blockSize_);

    generateMusic(cur, densityCur, musicA_, musicPhase_);
    generateMusic(tgt, densityTgt, musicB_, musicPhase_); // reuse phase for continuity

    equalPowerCrossfade(musicA_, musicB_, machine_.crossfade(), mixed_);

    generateVoice(cur, voice_, voicePhase_, static_cast<float>(blockSize_) / sampleRate_);

    duck_.process(voice_, mixed_, sampleRate_);
    reverb_.process(mixed_);
    limiter_.process(mixed_);

    // Write to stereo interleaved
    for (size_t i = 0; i < frames; ++i) {
        const float v = mixed_[i];
        out[2 * i] = v;
        out[2 * i + 1] = v;
    }

    // Advance state machine fade based on block time
    machine_.update(static_cast<float>(blockSize_) / sampleRate_);

    return rms(mixed_);
}

} // namespace audio
