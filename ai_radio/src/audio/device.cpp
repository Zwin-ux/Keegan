#define MINIAUDIO_IMPLEMENTATION
#include "../../vendor/miniaudio.h"
#include "device.h"
#include "../util/logger.h"
#include <cstring>

namespace audio {

struct AudioDevice::Impl {
    ma_context context{};
    ma_device device{};
};

static void dataCallback(ma_device *pDevice, void *pOutput, const void *, ma_uint32 frameCount) {
    if (pDevice == nullptr || pOutput == nullptr) return;
    auto *engine = reinterpret_cast<Engine *>(pDevice->pUserData);
    if (!engine) {
        std::memset(pOutput, 0, sizeof(float) * frameCount * 2);
        return;
    }
    float *out = static_cast<float *>(pOutput);
    engine->renderBlock(out, frameCount);
}

AudioDevice::AudioDevice(Engine &engine, uint32_t sampleRate, uint32_t framesPerBuffer)
    : engine_(engine),
      sampleRate_(sampleRate),
      framesPerBuffer_(framesPerBuffer),
      ready_(false),
      impl_(nullptr) {}

AudioDevice::~AudioDevice() {
    shutdown();
}

bool AudioDevice::init() {
    impl_ = new Impl();
    if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS) {
        util::logError("Audio context init failed");
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = sampleRate_;
    config.dataCallback = dataCallback;
    config.pUserData = &engine_;
    config.periodSizeInFrames = framesPerBuffer_;

    if (ma_device_init(&impl_->context, &config, &impl_->device) != MA_SUCCESS) {
        util::logError("Audio device init failed");
        ma_context_uninit(&impl_->context);
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    ready_ = true;
    return true;
}

bool AudioDevice::start() {
    if (!ready_ || !impl_) return false;
    util::logInfo("Starting audio device");
    return ma_device_start(&impl_->device) == MA_SUCCESS;
}

void AudioDevice::stop() {
    if (!impl_) return;
    util::logInfo("Stopping audio device");
    ma_device_stop(&impl_->device);
}

void AudioDevice::shutdown() {
    if (!impl_) return;
    util::logInfo("Shutting down audio device");
    ma_device_uninit(&impl_->device);
    ma_context_uninit(&impl_->context);
    delete impl_;
    impl_ = nullptr;
    ready_ = false;
}

} // namespace audio
