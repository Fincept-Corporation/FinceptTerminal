// NOTE: miniaudio implementation is in audio_capture.cpp (single compilation unit)
#include <miniaudio.h>
#include "audio_player.h"
#include "core/logger.h"
#include <cstring>
#include <algorithm>
#include <thread>

namespace fincept::voice {

static void playback_callback(ma_device* device, void* output, const void* /*input*/,
                                ma_uint32 frame_count) {
    auto* player = static_cast<AudioPlayer*>(device->pUserData);
    if (player && player->is_playing()) {
        player->on_playback_data(output, frame_count);
    } else {
        std::memset(output, 0, frame_count * sizeof(int16_t));
    }
}

AudioPlayer& AudioPlayer::instance() {
    static AudioPlayer inst;
    return inst;
}

AudioPlayer::~AudioPlayer() {
    shutdown();
}

bool AudioPlayer::init() {
    if (initialized_) return true;
    initialized_ = true;
    LOG_INFO("AudioPlayer", "Initialized");
    return true;
}

void AudioPlayer::shutdown() {
    stop();
    initialized_ = false;
}

bool AudioPlayer::start_device(int sample_rate) {
    stop_device();

    auto* device = new ma_device;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_s16;
    config.playback.channels = 1;
    config.sampleRate        = (ma_uint32)sample_rate;
    config.dataCallback      = playback_callback;
    config.pUserData         = this;

    ma_result result = ma_device_init(nullptr, &config, device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("AudioPlayer", "Failed to init playback device: %d", (int)result);
        delete device;
        return false;
    }

    device_ = device;

    result = ma_device_start(device);
    if (result != MA_SUCCESS) {
        LOG_ERROR("AudioPlayer", "Failed to start playback: %d", (int)result);
        ma_device_uninit(device);
        delete device;
        device_ = nullptr;
        return false;
    }

    return true;
}

void AudioPlayer::stop_device() {
    if (device_) {
        auto* device = static_cast<ma_device*>(device_);
        ma_device_stop(device);
        ma_device_uninit(device);
        delete device;
        device_ = nullptr;
    }
}

void AudioPlayer::on_playback_data(void* output, uint32_t frame_count) {
    auto* out = static_cast<int16_t*>(output);
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t available = playback_samples_.size() - playback_pos_;
    size_t to_copy = std::min<size_t>(frame_count, available);

    if (to_copy > 0) {
        std::memcpy(out, playback_samples_.data() + playback_pos_,
                     to_copy * sizeof(int16_t));
        playback_pos_ += to_copy;
    }

    if (to_copy < frame_count) {
        std::memset(out + to_copy, 0, (frame_count - to_copy) * sizeof(int16_t));
    }

    if (playback_pos_ >= playback_samples_.size() && playing_) {
        playing_ = false;
        if (finish_callback_) {
            auto cb = finish_callback_;
            std::thread([cb]() { cb(); }).detach();
        }
    }
}

bool AudioPlayer::play_pcm(const std::vector<int16_t>& samples, int sample_rate) {
    if (samples.empty()) return false;
    stop();

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        playback_samples_ = samples;
        playback_pos_ = 0;
        playback_rate_ = sample_rate;
    }

    playing_ = true;
    if (!start_device(sample_rate)) {
        playing_ = false;
        return false;
    }

    LOG_INFO("AudioPlayer", "Playing %zu samples at %dHz", samples.size(), sample_rate);
    return true;
}

bool AudioPlayer::play_wav(const std::vector<uint8_t>& wav_data) {
    if (wav_data.size() < 44) return false;

    int channels = wav_data[22] | (wav_data[23] << 8);
    int sample_rate = wav_data[24] | (wav_data[25] << 8) |
                      (wav_data[26] << 16) | (wav_data[27] << 24);
    int bits = wav_data[34] | (wav_data[35] << 8);

    if (bits != 16 || channels < 1) {
        LOG_ERROR("AudioPlayer", "Unsupported WAV: %dch %dbit", channels, bits);
        return false;
    }

    size_t data_offset = 44;
    uint32_t data_size = wav_data[40] | (wav_data[41] << 8) |
                         (wav_data[42] << 16) | (wav_data[43] << 24);

    if (data_offset + data_size > wav_data.size()) {
        data_size = static_cast<uint32_t>(wav_data.size() - data_offset);
    }

    size_t num_samples = data_size / sizeof(int16_t);
    std::vector<int16_t> samples(num_samples);
    std::memcpy(samples.data(), wav_data.data() + data_offset, num_samples * sizeof(int16_t));

    if (channels == 2) {
        std::vector<int16_t> mono(num_samples / 2);
        for (size_t i = 0; i < mono.size(); i++) {
            mono[i] = (int16_t)(((int32_t)samples[i * 2] + samples[i * 2 + 1]) / 2);
        }
        samples = std::move(mono);
    }

    return play_pcm(samples, sample_rate);
}

bool AudioPlayer::play_mp3(const std::vector<uint8_t>& mp3_data) {
    if (mp3_data.empty()) return false;

    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_s16, 1, 24000);
    ma_decoder decoder;

    ma_result result = ma_decoder_init_memory(mp3_data.data(), mp3_data.size(),
                                               &decoder_config, &decoder);
    if (result != MA_SUCCESS) {
        LOG_ERROR("AudioPlayer", "Failed to decode MP3: %d", (int)result);
        return false;
    }

    ma_uint64 total_frames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames);
    if (total_frames == 0) total_frames = 1024 * 1024;

    std::vector<int16_t> samples;
    samples.resize((size_t)total_frames);

    ma_uint64 frames_read = 0;
    ma_decoder_read_pcm_frames(&decoder, samples.data(), total_frames, &frames_read);
    samples.resize((size_t)frames_read);

    ma_uint32 sample_rate = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);

    if (samples.empty()) {
        LOG_ERROR("AudioPlayer", "MP3 decode produced no samples");
        return false;
    }

    LOG_INFO("AudioPlayer", "Decoded MP3: %zu frames at %uHz", samples.size(), sample_rate);
    return play_pcm(samples, (int)sample_rate);
}

void AudioPlayer::stop() {
    playing_ = false;
    stop_device();

    std::lock_guard<std::mutex> lock(buffer_mutex_);
    playback_samples_.clear();
    playback_pos_ = 0;
}

} // namespace fincept::voice
