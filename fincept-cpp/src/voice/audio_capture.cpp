#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "audio_capture.h"
#include "core/logger.h"
#include <cstring>
#include <cmath>

namespace fincept::voice {

// ============================================================================
// miniaudio callback — routes to AudioCapture instance
// ============================================================================
static void capture_callback(ma_device* device, void* /*output*/, const void* input,
                              ma_uint32 frame_count) {
    auto* capture = static_cast<AudioCapture*>(device->pUserData);
    if (capture && capture->is_recording()) {
        capture->on_audio_data(input, frame_count);
    }
}

// ============================================================================
// Singleton
// ============================================================================
AudioCapture& AudioCapture::instance() {
    static AudioCapture inst;
    return inst;
}

AudioCapture::~AudioCapture() {
    shutdown();
}

// ============================================================================
// Init — create miniaudio capture device
// ============================================================================
bool AudioCapture::init() {
    if (initialized_) return true;

    auto* device = new ma_device;

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_s16;
    config.capture.channels = CHANNELS;
    config.sampleRate       = SAMPLE_RATE;
    config.dataCallback     = capture_callback;
    config.pUserData        = this;

    ma_result result = ma_device_init(nullptr, &config, device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("AudioCapture", "Failed to init capture device: %d", (int)result);
        delete device;
        return false;
    }

    device_ = device;
    initialized_ = true;
    LOG_INFO("AudioCapture", "Initialized: %dHz, %d channel, 16-bit", SAMPLE_RATE, CHANNELS);
    return true;
}

void AudioCapture::shutdown() {
    if (!initialized_) return;
    stop_recording();

    if (device_) {
        auto* device = static_cast<ma_device*>(device_);
        ma_device_uninit(device);
        delete device;
        device_ = nullptr;
    }

    initialized_ = false;
    LOG_INFO("AudioCapture", "Shutdown");
}

// ============================================================================
// Recording control
// ============================================================================
bool AudioCapture::start_recording() {
    if (!initialized_ || recording_) return false;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        samples_.clear();
    }

    auto* device = static_cast<ma_device*>(device_);
    ma_result result = ma_device_start(device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("AudioCapture", "Failed to start recording: %d", (int)result);
        return false;
    }

    recording_ = true;
    LOG_INFO("AudioCapture", "Recording started");
    return true;
}

void AudioCapture::stop_recording() {
    if (!recording_) return;
    recording_ = false;

    if (device_) {
        auto* device = static_cast<ma_device*>(device_);
        ma_device_stop(device);
    }

    LOG_INFO("AudioCapture", "Recording stopped, %zu samples captured", samples_.size());
}

// ============================================================================
// Audio data callback (called from audio thread)
// ============================================================================
void AudioCapture::on_audio_data(const void* input, uint32_t frame_count) {
    if (!input || frame_count == 0) return;

    const auto* pcm = static_cast<const int16_t*>(input);

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        samples_.insert(samples_.end(), pcm, pcm + frame_count * CHANNELS);
    }

    // Calculate RMS level for UI
    if (level_callback_) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < frame_count; i++) {
            float s = pcm[i] / 32768.0f;
            sum += s * s;
        }
        float rms = std::sqrt(sum / frame_count);
        level_callback_(rms);
    }
}

// ============================================================================
// Get recorded data
// ============================================================================
std::vector<int16_t> AudioCapture::get_recorded_samples() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return samples_;
}

float AudioCapture::get_duration() const {
    return static_cast<float>(samples_.size()) / SAMPLE_RATE;
}

void AudioCapture::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    samples_.clear();
}

// ============================================================================
// Generate WAV file bytes from recorded PCM (for OpenAI Whisper API upload)
// ============================================================================
std::vector<uint8_t> AudioCapture::get_recorded_wav() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (samples_.empty()) return {};

    uint32_t data_size = static_cast<uint32_t>(samples_.size() * sizeof(int16_t));
    uint32_t file_size = 44 + data_size;

    std::vector<uint8_t> wav;
    wav.resize(file_size);

    auto write_u32 = [&](size_t offset, uint32_t val) {
        wav[offset + 0] = (uint8_t)(val & 0xFF);
        wav[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
        wav[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
        wav[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
    };
    auto write_u16 = [&](size_t offset, uint16_t val) {
        wav[offset + 0] = (uint8_t)(val & 0xFF);
        wav[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    };

    wav[0] = 'R'; wav[1] = 'I'; wav[2] = 'F'; wav[3] = 'F';
    write_u32(4, file_size - 8);
    wav[8] = 'W'; wav[9] = 'A'; wav[10] = 'V'; wav[11] = 'E';
    wav[12] = 'f'; wav[13] = 'm'; wav[14] = 't'; wav[15] = ' ';
    write_u32(16, 16);
    write_u16(20, 1);
    write_u16(22, CHANNELS);
    write_u32(24, SAMPLE_RATE);
    write_u32(28, SAMPLE_RATE * CHANNELS * 2);
    write_u16(32, CHANNELS * 2);
    write_u16(34, BITS_PER_SAMPLE);
    wav[36] = 'd'; wav[37] = 'a'; wav[38] = 't'; wav[39] = 'a';
    write_u32(40, data_size);

    std::memcpy(wav.data() + 44, samples_.data(), data_size);

    return wav;
}

} // namespace fincept::voice
