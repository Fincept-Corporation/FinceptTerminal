#pragma once
// WhisperService.h — Offline speech-to-text via whisper.cpp
//
// Architecture:
//   • QAudioSource (callback mode) → lock-free ring buffer  (audio thread)
//   • QTimer drain → QtConcurrent::run whisper_full()       (worker thread)
//   • QMetaObject::invokeMethod → emit transcription_ready  (UI thread)
//
// Complies with P1 (never block UI thread), P6 (service layer), P8 (QPointer guards).
//
// Model lifecycle:
//   1. On first start_listening(), check AppDataLocation for ggml-base.en-q5_0.bin
//   2. If missing, download from HuggingFace — emit model_download_progress(int %)
//   3. On download complete, emit model_ready() and begin capture
//   4. On all subsequent calls the model is already loaded — zero overhead

#include <QAudioFormat>
#include <QAudioSource>
#include <QMediaDevices>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <vector>

// Forward-declare whisper context to avoid pulling the full header into every TU
// that includes WhisperService.h.
struct whisper_context;

namespace fincept::services {

// ── WhisperService ────────────────────────────────────────────────────────────
//
// Singleton.  Typical usage from AiChatBubble:
//
//   auto& ws = WhisperService::instance();
//   connect(&ws, &WhisperService::transcription_ready,
//           this, &AiChatBubble::on_transcription);
//   connect(&ws, &WhisperService::model_download_progress,
//           this, [this](int pct){ show_progress(pct); });
//   connect(&ws, &WhisperService::model_ready,
//           this, [this]{ start_listening_ui(); });
//   ws.start_listening();   // triggers download if model absent
//
class WhisperService : public QObject {
    Q_OBJECT

  public:
    static WhisperService& instance();

    // Delete copy/move — singleton.
    WhisperService(const WhisperService&)            = delete;
    WhisperService& operator=(const WhisperService&) = delete;

    // ── Public API ────────────────────────────────────────────────────────────

    /// Begin microphone capture.  If the model isn't downloaded yet, starts the
    /// download first and emits model_ready() when safe to proceed.
    void start_listening();

    /// Stop microphone capture immediately.  Any in-flight inference is
    /// discarded — its result will not be emitted.
    void stop_listening();

    [[nodiscard]] bool is_listening()  const noexcept;
    [[nodiscard]] bool is_model_ready() const noexcept;

    /// Absolute path to the model file (may not exist yet).
    [[nodiscard]] QString model_path() const;

  signals:
    /// Fired on the UI thread when whisper produces a non-empty transcript.
    void transcription_ready(const QString& text);

    /// Fired 0-100 during model download.
    void model_download_progress(int percent);

    /// Fired once after the model is successfully loaded and ready.
    void model_ready();

    /// Fired if something goes wrong (download failure, mic unavailable, etc.).
    void error_occurred(const QString& message);

    /// Fired when listening state changes — UI can update mic button.
    void listening_changed(bool active);

  private:
    explicit WhisperService(QObject* parent = nullptr);
    ~WhisperService() override;

    // ── Model management ──────────────────────────────────────────────────────
    void ensure_model();
    void download_model();
    bool load_model();
    void on_download_finished(QNetworkReply* reply);

    // ── Audio capture ─────────────────────────────────────────────────────────
    void open_audio_device();
    void close_audio_device();

    // ── Inference ─────────────────────────────────────────────────────────────
    // Called by drain_timer_ on the UI thread; dispatches work to thread pool.
    void drain_and_infer();

    // Runs on a worker thread — must NOT touch any QWidget or member directly.
    // Posts result back via invokeMethod.
    void run_inference(std::vector<float> samples);

    // ── Ring buffer helpers ───────────────────────────────────────────────────
    // append_samples() is called from the audio-thread callback — must be
    // lock-free (or at worst a single try-lock that drops frames on contention).
    void append_samples(const float* data, qsizetype frames);
    std::vector<float> drain_samples();

    // ── Constants ─────────────────────────────────────────────────────────────
    static constexpr int    kSampleRate      = 16'000;   // Hz — whisper requirement
    static constexpr int    kDrainIntervalMs = 1'200;    // how often to kick inference
    static constexpr int    kWindowSeconds   = 6;        // sliding audio window
    static constexpr int    kMaxWindowSamples = kSampleRate * kWindowSeconds;
    static constexpr float  kVadThreshold   = 0.012f;   // RMS silence gate
    static constexpr int    kVadTailSamples = kSampleRate / 2; // 500 ms of tail required
    static constexpr int    kInferenceThreads = 4;       // whisper n_threads

    static constexpr const char* kModelFilename =
        "ggml-base.en-q5_0.bin";
    static constexpr const char* kModelUrl =
        "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en-q5_0.bin";

    // ── State ─────────────────────────────────────────────────────────────────
    whisper_context*       ctx_          = nullptr;
    QAudioSource*          audio_source_ = nullptr;
    QNetworkAccessManager* nam_          = nullptr;
    QTimer*                drain_timer_  = nullptr;

    std::atomic<bool> listening_      {false};
    std::atomic<bool> model_loaded_   {false};
    std::atomic<bool> inference_busy_ {false};  // prevent overlapping inferences
    std::atomic<bool> download_active_{false};

    // Ring buffer — written by audio thread, read by UI thread drain_timer_.
    // Protected by ring_mutex_ (audio callback does a non-blocking try-lock;
    // on contention it simply drops the frame rather than blocking realtime thread).
    mutable QMutex      ring_mutex_;
    std::vector<float>  ring_buffer_;

    // Sliding window — accumulated across drain cycles for context continuity.
    std::vector<float>  window_buffer_;
};

} // namespace fincept::services
