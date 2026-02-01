#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdint>
#include "reverb.h"
#include "ducking.h"
#include "crossfade.h"
#include "limiter.h"
#include "scheduler.h"
#include "stem_player.h"
#include "../brain/state_machine.h"
#include "../brain/app_heuristics.h"
#include "../voice/story_bank.h"
#include "../brain/story_generator.h"
#include "oscillator.h"
#include "filter.h"

namespace audio {

// DSP parameters per mood for audio processing.
struct MoodDspParams {
    float reverbWet = 0.3f;
    float reverbDecay = 0.5f;
    float reverbPreDelay = 20.0f;
    float masterLpHz = 18000.0f;
};

// Snapshot of state for UI/HTTP/SSE.
struct PublicState {
    std::string moodId;
    std::string targetMoodId;
    std::string activeProcess;
    float energy = 0.0f;
    float intensity = 0.0f;
    float activity = 0.0f;
    float idleSeconds = 0.0f;
    bool playing = false;
    uint64_t updatedAtMs = 0;
};

class Engine {
public:
    Engine(float sampleRate = 48000.0f, size_t blockSize = 256);

    void setMoodPack(brain::MoodPack pack);
    void setIntensity(float value);
    void setMood(const std::string& moodId);

    // Active process name feeds heuristics to bias target mood/energy.
    void tick(const std::string &activeProcess, float dtSeconds);

    // Render one block into interleaved stereo output buffer.
    // Returns RMS of mixed output.
    float renderBlock(float *out, size_t frames);

    // Get current state for UI feedback.
    float currentEnergy() const { return intensity_; }
    const std::string& currentMoodId() const;
    bool isPlaying() const { return isPlaying_; }
    void setPlaying(bool playing) { isPlaying_ = playing; }
    PublicState snapshot() const;

private:
    float sampleRate_;
    size_t blockSize_;
    float intensity_;
    bool isPlaying_ = true;
    float timeSinceLastStory_ = 0.0f; 

    brain::MoodPack pack_;
    brain::MoodStateMachine machine_;
    brain::AppHeuristics heuristics_;
    brain::ActivityMonitor activityMonitor_;
    
    // Voice system
    voice::StoryBank storyBank_;
    brain::StoryGenerator storyGen_;
    
    // Thread safety for next story
    std::mutex voiceMutex_;
    std::shared_ptr<voice::Story> nextStory_ = nullptr;
    std::shared_ptr<voice::Story> currentStory_ = nullptr;

    Scheduler scheduler_;
    DuckingCompressor duck_;
    SimplePlateReverb reverb_;
    SoftLimiter limiter_;
    
    // Audio Intelligence (Phase 3.5)
    Oscillator binauralLeft_;
    Oscillator binauralRight_;
    BiquadFilter breathingLp_;
    BiquadFilter melatoninShelf_;
    
    float binLeftFreq_ = 200.0f;
    float binRightFreq_ = 240.0f; // 40Hz offset (Gamma)

    // Stem banks for current and target moods
    StemBank currentStems_;
    StemBank targetStems_;
    size_t currentMoodIndex_ = 0;
    size_t targetMoodIndex_ = 0;

    // Fallback procedural generation
    float musicPhase_;
    
    // Buffers reused per render
    std::vector<float> musicA_;
    std::vector<float> musicB_;
    std::vector<float> voice_;
    std::vector<float> mixed_;

    // DSP params per mood
    MoodDspParams getDspParams(const brain::MoodRecipe& recipe);

    void loadStemsForMood(size_t moodIndex, StemBank& bank);
    void generateMusic(const brain::MoodRecipe &recipe, float density, std::vector<float> &out, float &phase);
    
    // Renders active voice player or silence
    void renderVoice(std::vector<float> &out, size_t frames);
    
    // Check if we should trigger a story
    void updateNarrativeLogic(const brain::MoodRecipe& recipe, float dt);
    
    // Update binaural frequencies and filter settings
    void updateBioReactiveDsp(float dt);

    // Public state for UI/SSE.
    mutable std::mutex publicStateMutex_;
    PublicState publicState_;
};

} // namespace audio
