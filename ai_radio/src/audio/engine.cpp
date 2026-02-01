#include "engine.h"
#include "../util/logger.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <cstdlib>
#include <chrono>

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
      activityMonitor_(),
      storyBank_(),
      storyGen_(storyBank_),
      scheduler_(sampleRate),
      reverb_(sampleRate),
      limiter_(-1.0f, 0.05f),
      musicPhase_(0.0f),
      binauralLeft_(sampleRate),
      binauralRight_(sampleRate),
      breathingLp_(sampleRate),
      melatoninShelf_(sampleRate) {
    musicA_.resize(blockSize_);
    musicB_.resize(blockSize_);
    voice_.resize(blockSize_);
    mixed_.resize(blockSize_);

    // Initial filter settings
    breathingLp_.setParams(BiquadFilter::LowPass, 20000.0f, 0.707f);
    melatoninShelf_.setParams(BiquadFilter::HighShelf, 8000.0f, 0.707f, 0.0f);

    // Load stems for initial mood
    loadStemsForMood(0, currentStems_);

    if (storyBank_.loadFromFile("config/stories.json")) {
        util::logInfo("Engine: Voice stories loaded.");
    }

    // Initialize public state snapshot.
    {
        std::lock_guard<std::mutex> lock(publicStateMutex_);
        publicState_.moodId = machine_.currentRecipe().id;
        publicState_.targetMoodId = machine_.targetRecipe().id;
        publicState_.energy = intensity_;
        publicState_.intensity = intensity_;
        publicState_.activity = activityMonitor_.activity();
        publicState_.idleSeconds = activityMonitor_.idleTime();
        publicState_.playing = isPlaying_;
        publicState_.updatedAtMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }
}

void Engine::setMoodPack(brain::MoodPack pack) {
    pack_ = std::move(pack);
    machine_ = brain::MoodStateMachine(pack_);
    
    if (!pack_.moods.empty()) {
        loadStemsForMood(0, currentStems_);
    }
}

void Engine::setIntensity(float value) {
    intensity_ = clamp01(value);
}

void Engine::setMood(const std::string& moodId) {
    machine_.setTargetMood(moodId);
}

const std::string& Engine::currentMoodId() const {
    return machine_.currentRecipe().id;
}

void Engine::tick(const std::string &activeProcess, float dtSeconds) {
    heuristics_.setActiveProcess(activeProcess);
    activityMonitor_.update(dtSeconds);
    
    float activityBoost = activityMonitor_.activity() * 0.3f;
    float effectiveIntensity = clamp01(intensity_ + activityBoost);
    
    const auto bias = heuristics_.currentBias();
    machine_.setTargetMood(bias.moodId);
    machine_.update(dtSeconds);

    size_t newTargetIndex = 0;
    for (size_t i = 0; i < pack_.moods.size(); ++i) {
        if (pack_.moods[i].id == machine_.targetRecipe().id) {
            newTargetIndex = i;
            break;
        }
    }
    
    if (newTargetIndex != targetMoodIndex_) {
        targetMoodIndex_ = newTargetIndex;
        loadStemsForMood(newTargetIndex, targetStems_);
    }
    
    if (machine_.crossfade() >= 1.0f && currentMoodIndex_ != targetMoodIndex_) {
        currentMoodIndex_ = targetMoodIndex_;
        std::swap(currentStems_, targetStems_);
    }

    if (storyBank_.countForMood(machine_.currentRecipe().id) < 5) { 
         std::string context = "User is in " + activeProcess + ". Energy: " + std::to_string(effectiveIntensity);
         storyGen_.requestStory(machine_.currentRecipe().id, context);
    }
    storyGen_.update();

    updateNarrativeLogic(machine_.currentRecipe(), dtSeconds);
    
    // Update DSP targets logic (running at tick rate is fine for smooth changes)
    updateBioReactiveDsp(dtSeconds);

    // Publish snapshot for UI/SSE.
    {
        std::lock_guard<std::mutex> lock(publicStateMutex_);
        publicState_.moodId = machine_.currentRecipe().id;
        publicState_.targetMoodId = machine_.targetRecipe().id;
        publicState_.activeProcess = activeProcess;
        publicState_.energy = effectiveIntensity;
        publicState_.intensity = intensity_;
        publicState_.activity = activityMonitor_.activity();
        publicState_.idleSeconds = activityMonitor_.idleTime();
        publicState_.playing = isPlaying_;
        publicState_.updatedAtMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
    }
}

void Engine::updateBioReactiveDsp(float dt) {
    const auto& mood = machine_.currentRecipe().id;
    
    // 1. Binaural Beats Targeting
    float targetLeft = 200.0f;
    float targetRight = 240.0f; // Default Gamma (40Hz offset)

    if (mood == "rain_cave") { // Theta (6Hz)
        targetLeft = 120.0f;
        targetRight = 126.0f;
    } else if (mood == "sleep_ship") { // Delta (2Hz)
        targetLeft = 80.0f;
        targetRight = 82.0f;
    } else if (mood == "arcade_night") { // Beta (25Hz)
        targetLeft = 150.0f;
        targetRight = 175.0f;
    }

    // Smooth transition for frequencies could be added, but sudden shift is okay for < 1Hz diff usually.
    // We'll set them directly for now.
    binauralLeft_.setFrequency(targetLeft);
    binauralRight_.setFrequency(targetRight);

    // 2. Breathing Filter (Activity -> Cutoff)
    // Low energy = 500Hz, High energy = 20kHz
    float activity = activityMonitor_.activity(); // 0..1
    float targetCutoff = 500.0f + (19500.0f * activity * activity); // Exponential curve
    breathingLp_.setParams(BiquadFilter::LowPass, targetCutoff, 0.707f);

    // 3. Melatonin Mode (Time -> High Shelf Gain)
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm = *localtime(&tt);
    
    float shelfGain = 0.0f;
    if (local_tm.tm_hour >= 23 || local_tm.tm_hour < 6) {
        // Night mode: Cut highs
        shelfGain = -12.0f; 
    } else if (local_tm.tm_hour >= 21) {
        // Evening wind-down
        shelfGain = -6.0f;
    }
    melatoninShelf_.setParams(BiquadFilter::HighShelf, 6000.0f, 0.707f, shelfGain);
}

void Engine::updateNarrativeLogic(const brain::MoodRecipe& recipe, float dt) {
    timeSinceLastStory_ += dt;
    {
        std::lock_guard<std::mutex> lock(voiceMutex_);
        if (nextStory_ != nullptr) return;
    }
    if (timeSinceLastStory_ < 60.0f) return;

    float prob = recipe.narrativeFrequency * dt * 0.1f; 
    if (static_cast<float>(rand()) / RAND_MAX < prob) {
        auto story = storyBank_.pickStory(recipe.id, timeSinceLastStory_, 60.0f);
        if (story) {
            util::logInfo("Engine: Triggering story: " + story->id);
            storyBank_.markPlayed(story, timeSinceLastStory_);
            timeSinceLastStory_ = 0.0f;
            std::lock_guard<std::mutex> lock(voiceMutex_);
            nextStory_ = story;
        }
    }
}

void Engine::loadStemsForMood(size_t moodIndex, StemBank& bank) {
    if (moodIndex >= pack_.moods.size()) return;
    const auto& recipe = pack_.moods[moodIndex];
    if (!recipe.stems.empty()) {
        bank.loadFromConfig(recipe.stems);
    }
}

MoodDspParams Engine::getDspParams(const brain::MoodRecipe& recipe) {
    MoodDspParams params;
    if (recipe.id == "focus_room") {
        params.reverbWet = 0.2f; params.reverbDecay = 0.4f; params.reverbPreDelay = 15.0f;
        params.masterLpHz = 12000.0f;  
    } else if (recipe.id == "rain_cave") {
        params.reverbWet = 0.5f; params.reverbDecay = 0.7f; params.reverbPreDelay = 40.0f;
        params.masterLpHz = 16000.0f;
    } else if (recipe.id == "arcade_night") {
        params.reverbWet = 0.25f; params.reverbDecay = 0.3f; params.reverbPreDelay = 10.0f;
        params.masterLpHz = 18000.0f; 
    } else if (recipe.id == "sleep_ship") {
        params.reverbWet = 0.35f; params.reverbDecay = 0.6f; params.reverbPreDelay = 30.0f;
        params.masterLpHz = 6000.0f;  
    }
    return params;
}

void Engine::generateMusic(const brain::MoodRecipe &recipe, float density, std::vector<float> &out, float &phase) {
    const float freq = 110.0f + 220.0f * recipe.energy * intensity_;
    const float amp = 0.2f + 0.3f * density;
    for (size_t i = 0; i < blockSize_; ++i) {
        float v = std::sin(phase) * amp;
        v += std::sin(phase * 2.0f) * recipe.tension * 0.1f;
        out[i] = v;
        phase += 2.0f * kPi * freq / sampleRate_;
        if (phase > 2.0f * kPi) phase -= 2.0f * kPi;
    }
}

void Engine::renderVoice(std::vector<float> &out, size_t frames) {
    std::fill(out.begin(), out.end(), 0.0f);
    {
        std::lock_guard<std::mutex> lock(voiceMutex_);
        if (nextStory_) {
            currentStory_ = nextStory_;
            nextStory_ = nullptr;
            currentStory_->player.reset(); 
        }
    }
    if (currentStory_) {
        currentStory_->player.render(out.data(), frames, 1.0f); 
        if (currentStory_->player.isFinished()) {
            currentStory_ = nullptr;
        }
    }
}

float Engine::renderBlock(float *out, size_t frames) {
    if (frames == 0 || out == nullptr) return 0.0f;
    if (!isPlaying_) {
        std::fill(out, out + frames * 2, 0.0f);
        return 0.0f;
    }
    
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

    // Stems / Procedural
    if (currentStems_.count() > 0) {
        currentStems_.renderMixed(musicA_.data(), frames, densityCur);
    } else {
        generateMusic(cur, densityCur, musicA_, musicPhase_);
    }
    
    if (targetStems_.count() > 0 && machine_.crossfade() < 1.0f) {
        targetStems_.renderMixed(musicB_.data(), frames, densityTgt);
    } else {
        generateMusic(tgt, densityTgt, musicB_, musicPhase_);
    }

    equalPowerCrossfade(musicA_, musicB_, machine_.crossfade(), mixed_);

    // Voice
    renderVoice(voice_, frames);
    duck_.process(voice_, mixed_, sampleRate_);
    
    // Mix Voice & Binaural Beats
    constexpr float kBinauralGain = 0.03f; // Subtle background hum (-30dB)
    
    // Process Binaural directly into mixed
    // Note: mixed is mono currently? No, mixed is vector<float> representing mono mix before stereo split? 
    // Wait, renderMixed and equalPowerCrossfade seem to work on mono buffers.
    // But binaural REQUIRES stereo separation.
    // My architecture merges to mono 'mixed_' then applies Reverb (which outputs mono? SimplePlateReverb is mono-in-stereo-out or mono-in-mono-out?)
    // SimplePlateReverb::process(std::vector<float>& buf, ...) - in-place mono?
    
    // Engine::renderBlock outputs INTERLEAVED stereo at the end.
    // So 'mixed_' is mono up until the loop at the end.
    // If I mix binaural into 'mixed_', I lose the stereo separation (the Beat!).
    // I must apply binaural modulation AT THE END when creating the stereo buffer.
    
    // Mix Voice
    for (size_t i = 0; i < frames; ++i) mixed_[i] += voice_[i];
    
    // Apply Mono DSP (Limiter, Reverb, Breathing Filter, Melatonin Shelf)
    // Reverb
    MoodDspParams dsp = getDspParams(cur);
    reverb_.setParams(dsp.reverbPreDelay, dsp.reverbDecay, 0.25f);
    reverb_.process(mixed_, dsp.reverbWet);
    
    // Breathing Filter
    breathingLp_.processBlock(mixed_);
    
    // Melatonin Shelf
    melatoninShelf_.processBlock(mixed_);
    
    limiter_.process(mixed_);

    // Final Stereo Mix + Binaural Injection
    for (size_t i = 0; i < frames; ++i) {
        float mono = mixed_[i];
        
        // Generate binaural samples
        float binL = binauralLeft_.process() * kBinauralGain;
        float binR = binauralRight_.process() * kBinauralGain;
        
        out[2 * i]     = mono + binL;
        out[2 * i + 1] = mono + binR;
    }

    machine_.update(static_cast<float>(blockSize_) / sampleRate_);
    return rms(mixed_);
}

PublicState Engine::snapshot() const {
    std::lock_guard<std::mutex> lock(publicStateMutex_);
    return publicState_;
}

} // namespace audio
