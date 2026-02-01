#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include "../brain/state_machine.h"

namespace audio {

// Decodes and plays WAV audio files with seamless looping support.
// Audio is decoded upfront into memory for low-latency playback.
class StemPlayer {
public:
    StemPlayer() = default;
    ~StemPlayer() = default;

    // Load and decode a WAV file into memory.
    // Returns true on success. Logs error and returns false on failure.
    bool load(const std::string& path);

    // Check if audio data is loaded and ready for playback.
    bool isLoaded() const { return !buffer_.empty(); }

    // Get the sample rate of the loaded audio.
    uint32_t sampleRate() const { return sampleRate_; }

    // Get number of channels (1 = mono, 2 = stereo).
    uint16_t channels() const { return channels_; }

    // Get total number of samples (per channel).
    size_t totalSamples() const { return buffer_.size() / channels_; }

    // Render audio into output buffer with specified gain.
    // Output should be sized for (frames) samples.
    // Automatically loops when reaching end of audio.
    void render(float* out, size_t frames, float gain = 1.0f);

    // Render and mix (add) into existing buffer rather than overwrite.
    void renderMix(float* out, size_t frames, float gain = 1.0f);

    // Seek to a specific sample position.
    void seek(size_t sampleOffset);

    // Reset playback to beginning.
    void reset() { readPos_ = 0; }

    // Set whether this stem should loop (default: true).
    void setLooping(bool loop) { looping_ = loop; }
    bool isLooping() const { return looping_; }

    // Check if playback has finished (only relevant if not looping).
    bool isFinished() const { return !looping_ && readPos_ >= buffer_.size(); }

private:
    std::vector<float> buffer_;     // Interleaved audio samples
    size_t readPos_ = 0;            // Current read position in samples
    uint32_t sampleRate_ = 48000;
    uint16_t channels_ = 1;
    bool looping_ = true;

    // Internal WAV parsing helpers
    bool parseWavHeader(const std::vector<uint8_t>& data, size_t& dataOffset, size_t& dataSize);
    void convertToFloat(const uint8_t* data, size_t dataSize, uint16_t bitsPerSample);
};

// Collection of stems for a mood, manages loading and mixing.
class StemBank {
public:
    struct StemEntry {
        StemPlayer player;
        std::string role;       // "base", "rhythm", "env", "melodic"
        float gainDb = 0.0f;
        float probability = 1.0f;
        bool active = true;
    };

    // Load all stems for a mood from config.
    bool loadFromConfig(const std::vector<brain::StemConfig>& configs);

    // Clear all loaded stems.
    void clear();

    // Render all active stems mixed together.
    // Output is mono, sized for (frames) samples.
    void renderMixed(float* out, size_t frames, float densityThreshold);

    // Get number of loaded stems.
    size_t count() const { return stems_.size(); }

    // Access individual stems.
    StemEntry& at(size_t index) { return stems_[index]; }
    const StemEntry& at(size_t index) const { return stems_[index]; }

private:
    std::vector<StemEntry> stems_;
    std::vector<float> tempBuffer_;
};

// Convert decibels to linear gain.
inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

} // namespace audio
