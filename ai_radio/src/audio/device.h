#pragma once

#include <cstdint>
#include "engine.h"

namespace audio {

class AudioDevice {
public:
    AudioDevice(Engine &engine, uint32_t sampleRate = 48000, uint32_t framesPerBuffer = 512);
    ~AudioDevice();

    bool init();
    bool start();
    void stop();
    void shutdown();

    bool ready() const { return ready_; }

private:
    Engine &engine_;
    uint32_t sampleRate_;
    uint32_t framesPerBuffer_;
    bool ready_;

    struct Impl;
    Impl *impl_;
};

} // namespace audio
