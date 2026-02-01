#include "stem_player.h"
#include "../util/logger.h"
#include "../brain/state_machine.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace audio {

namespace {
// WAV file format constants
constexpr uint32_t RIFF_ID = 0x46464952;  // "RIFF"
constexpr uint32_t WAVE_ID = 0x45564157;  // "WAVE"
constexpr uint32_t FMT_ID  = 0x20746D66;  // "fmt "
constexpr uint32_t DATA_ID = 0x61746164;  // "data"

template<typename T>
T readLE(const uint8_t* data) {
    T result = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        result |= static_cast<T>(data[i]) << (8 * i);
    }
    return result;
}
} // namespace

bool StemPlayer::load(const std::string& path) {
    buffer_.clear();
    readPos_ = 0;

    // Read entire file into memory
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        util::logError("StemPlayer: Failed to open file: " + path);
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize < 44) {
        util::logError("StemPlayer: File too small to be valid WAV: " + path);
        return false;
    }

    file.seekg(0);
    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    // Parse WAV header
    size_t dataOffset = 0;
    size_t dataSize = 0;
    if (!parseWavHeader(fileData, dataOffset, dataSize)) {
        util::logError("StemPlayer: Invalid WAV header: " + path);
        return false;
    }

    // Get bits per sample from header (offset 34 in standard WAV)
    uint16_t bitsPerSample = readLE<uint16_t>(&fileData[34]);

    // Convert audio data to float
    convertToFloat(fileData.data() + dataOffset, dataSize, bitsPerSample);

    util::logInfo("StemPlayer: Loaded " + path + " (" + 
                  std::to_string(totalSamples()) + " samples, " +
                  std::to_string(channels_) + " ch, " +
                  std::to_string(sampleRate_) + " Hz)");

    return true;
}

bool StemPlayer::parseWavHeader(const std::vector<uint8_t>& data, size_t& dataOffset, size_t& dataSize) {
    if (data.size() < 44) return false;

    // Check RIFF header
    if (readLE<uint32_t>(&data[0]) != RIFF_ID) return false;
    if (readLE<uint32_t>(&data[8]) != WAVE_ID) return false;

    // Find fmt chunk
    size_t pos = 12;
    bool foundFmt = false;
    while (pos + 8 <= data.size()) {
        uint32_t chunkId = readLE<uint32_t>(&data[pos]);
        uint32_t chunkSize = readLE<uint32_t>(&data[pos + 4]);

        if (chunkId == FMT_ID) {
            if (pos + 8 + chunkSize > data.size()) return false;
            
            uint16_t audioFormat = readLE<uint16_t>(&data[pos + 8]);
            if (audioFormat != 1 && audioFormat != 3) {
                util::logError("StemPlayer: Unsupported audio format (only PCM/float supported)");
                return false;
            }

            channels_ = readLE<uint16_t>(&data[pos + 10]);
            sampleRate_ = readLE<uint32_t>(&data[pos + 12]);
            foundFmt = true;
        } else if (chunkId == DATA_ID) {
            if (!foundFmt) return false;
            dataOffset = pos + 8;
            dataSize = chunkSize;
            return true;
        }

        pos += 8 + chunkSize;
        if (chunkSize % 2 == 1) pos++; // Padding byte
    }

    return false;
}

void StemPlayer::convertToFloat(const uint8_t* data, size_t dataSize, uint16_t bitsPerSample) {
    size_t bytesPerSample = bitsPerSample / 8;
    size_t totalSamples = dataSize / bytesPerSample;
    buffer_.resize(totalSamples);

    for (size_t i = 0; i < totalSamples; ++i) {
        const uint8_t* samplePtr = data + i * bytesPerSample;

        if (bitsPerSample == 8) {
            // 8-bit unsigned
            buffer_[i] = (static_cast<float>(samplePtr[0]) - 128.0f) / 128.0f;
        } else if (bitsPerSample == 16) {
            // 16-bit signed little-endian
            int16_t sample = static_cast<int16_t>(samplePtr[0] | (samplePtr[1] << 8));
            buffer_[i] = static_cast<float>(sample) / 32768.0f;
        } else if (bitsPerSample == 24) {
            // 24-bit signed little-endian
            int32_t sample = samplePtr[0] | (samplePtr[1] << 8) | (samplePtr[2] << 16);
            if (sample & 0x800000) sample |= 0xFF000000; // Sign extend
            buffer_[i] = static_cast<float>(sample) / 8388608.0f;
        } else if (bitsPerSample == 32) {
            // 32-bit float
            float sample;
            std::memcpy(&sample, samplePtr, sizeof(float));
            buffer_[i] = sample;
        }
    }
}

void StemPlayer::render(float* out, size_t frames, float gain) {
    if (buffer_.empty() || frames == 0) {
        std::fill(out, out + frames, 0.0f);
        return;
    }

    for (size_t i = 0; i < frames; ++i) {
        if (readPos_ >= buffer_.size()) {
            if (looping_) {
                readPos_ = 0;
            } else {
                out[i] = 0.0f;
                continue;
            }
        }

        // For stereo files, mix down to mono
        if (channels_ == 2 && readPos_ + 1 < buffer_.size()) {
            out[i] = (buffer_[readPos_] + buffer_[readPos_ + 1]) * 0.5f * gain;
            readPos_ += 2;
        } else {
            out[i] = buffer_[readPos_] * gain;
            readPos_++;
        }
    }
}

void StemPlayer::renderMix(float* out, size_t frames, float gain) {
    if (buffer_.empty() || frames == 0) return;

    for (size_t i = 0; i < frames; ++i) {
        if (readPos_ >= buffer_.size()) {
            if (looping_) {
                readPos_ = 0;
            } else {
                continue;
            }
        }

        // For stereo files, mix down to mono
        if (channels_ == 2 && readPos_ + 1 < buffer_.size()) {
            out[i] += (buffer_[readPos_] + buffer_[readPos_ + 1]) * 0.5f * gain;
            readPos_ += 2;
        } else {
            out[i] += buffer_[readPos_] * gain;
            readPos_++;
        }
    }
}

void StemPlayer::seek(size_t sampleOffset) {
    size_t maxPos = buffer_.size() / channels_;
    readPos_ = std::min(sampleOffset * channels_, buffer_.size());
}

// --- StemBank implementation ---

bool StemBank::loadFromConfig(const std::vector<brain::StemConfig>& configs) {
    clear();
    
    for (const auto& cfg : configs) {
        StemEntry entry;
        entry.role = cfg.role;
        entry.gainDb = cfg.gainDb;
        entry.probability = cfg.probability;
        entry.active = true;

        if (!entry.player.load(cfg.file)) {
            util::logError("StemBank: Failed to load stem: " + cfg.file);
            // Continue loading other stems, this one just won't play
            continue;
        }

        stems_.push_back(std::move(entry));
    }

    util::logInfo("StemBank: Loaded " + std::to_string(stems_.size()) + " stems");
    return !stems_.empty();
}

void StemBank::clear() {
    stems_.clear();
    tempBuffer_.clear();
}

void StemBank::renderMixed(float* out, size_t frames, float densityThreshold) {
    // Clear output buffer
    std::fill(out, out + frames, 0.0f);

    if (stems_.empty()) return;

    // Ensure temp buffer is sized
    if (tempBuffer_.size() < frames) {
        tempBuffer_.resize(frames);
    }

    // Determine how many stems to activate based on density
    size_t maxActive = static_cast<size_t>(std::ceil(stems_.size() * densityThreshold));
    maxActive = std::max<size_t>(1, maxActive); // At least one stem

    size_t activeCount = 0;
    for (auto& stem : stems_) {
        if (!stem.player.isLoaded()) continue;
        if (activeCount >= maxActive) break;

        // Apply probability check
        if (stem.probability < 1.0f) {
            float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            if (roll > stem.probability) continue;
        }

        float gain = dbToLinear(stem.gainDb);
        stem.player.renderMix(out, frames, gain);
        activeCount++;
    }
}

} // namespace audio
