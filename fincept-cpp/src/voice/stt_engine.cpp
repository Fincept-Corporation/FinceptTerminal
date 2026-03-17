#include "stt_engine.h"
#include "audio_capture.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <chrono>

namespace fincept::voice {

using json = nlohmann::json;

STTEngine& STTEngine::instance() {
    static STTEngine inst;
    return inst;
}

bool STTEngine::init() {
    if (initialized_) return true;
    initialized_ = true;
    LOG_INFO("STT", "Engine initialized");
    return true;
}

void STTEngine::shutdown() {
    initialized_ = false;
}

std::future<TranscriptionResult> STTEngine::transcribe(
    const std::vector<uint8_t>& wav_data, const STTConfig& config) {
    return std::async(std::launch::async, [this, wav_data, config]() {
        switch (config.provider) {
            case STTProvider::OpenAIAPI:
                return transcribe_openai_api(wav_data, config);
            case STTProvider::Whisper:
                LOG_WARN("STT", "Local Whisper not yet available, using OpenAI API");
                return transcribe_openai_api(wav_data, config);
        }
        TranscriptionResult r;
        r.error = "Unknown STT provider";
        return r;
    });
}

std::future<TranscriptionResult> STTEngine::transcribe_pcm(
    const std::vector<int16_t>& samples, int sample_rate,
    const STTConfig& config) {
    uint32_t data_size = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    uint32_t file_size = 44 + data_size;
    std::vector<uint8_t> wav(file_size);

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

    wav[0]='R'; wav[1]='I'; wav[2]='F'; wav[3]='F';
    write_u32(4, file_size - 8);
    wav[8]='W'; wav[9]='A'; wav[10]='V'; wav[11]='E';
    wav[12]='f'; wav[13]='m'; wav[14]='t'; wav[15]=' ';
    write_u32(16, 16);
    write_u16(20, 1);
    write_u16(22, 1);
    write_u32(24, sample_rate);
    write_u32(28, sample_rate * 2);
    write_u16(32, 2);
    write_u16(34, 16);
    wav[36]='d'; wav[37]='a'; wav[38]='t'; wav[39]='a';
    write_u32(40, data_size);
    std::memcpy(wav.data() + 44, samples.data(), data_size);

    return transcribe(wav, config);
}

TranscriptionResult STTEngine::transcribe_openai_api(
    const std::vector<uint8_t>& wav_data, const STTConfig& config) {

    TranscriptionResult result;

    if (config.openai_key.empty()) {
        result.error = "OpenAI API key not set";
        return result;
    }

    if (wav_data.empty()) {
        result.error = "No audio data";
        return result;
    }

    std::string boundary = "----FinceptVoice" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    body += "whisper-1\r\n";

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
    body += config.language + "\r\n";

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n";
    body += "json\r\n";

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";
    body.append(reinterpret_cast<const char*>(wav_data.data()), wav_data.size());
    body += "\r\n";

    body += "--" + boundary + "--\r\n";

    http::Headers headers = {
        {"Authorization", "Bearer " + config.openai_key},
        {"Content-Type", "multipart/form-data; boundary=" + boundary},
    };

    auto& client = http::HttpClient::instance();
    auto resp = client.post("https://api.openai.com/v1/audio/transcriptions",
                             body, headers);

    if (!resp.success || resp.status_code != 200) {
        result.error = "Whisper API error: " + std::to_string(resp.status_code);
        LOG_ERROR("STT", "API error: %d %s", resp.status_code, resp.error.c_str());
        return result;
    }

    try {
        auto j = json::parse(resp.body);
        result.text = j.value("text", "");
        result.language = j.value("language", config.language);
        result.duration = j.value("duration", 0.0f);
        result.success = !result.text.empty();
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        LOG_ERROR("STT", "Parse error: %s", e.what());
    }

    // Remove trailing period (speech recognition artifact)
    if (!result.text.empty() && result.text.back() == '.') {
        result.text.pop_back();
        while (!result.text.empty() && result.text.back() == ' ') {
            result.text.pop_back();
        }
    }

    LOG_INFO("STT", "Transcribed: \"%s\" (%.1fs)", result.text.c_str(), result.duration);
    return result;
}

} // namespace fincept::voice
