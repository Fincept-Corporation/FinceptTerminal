// WhisperService.cpp — Offline speech-to-text via whisper.cpp
//
// Threading model:
//   Audio thread  : QAudioSource callback → append_samples() → ring_mutex_ try-lock
//   UI thread     : drain_timer_ fires → drain_and_infer() → QtConcurrent::run()
//   Worker thread : run_inference() → whisper_full() → invokeMethod back to UI
//
// All whisper_* calls happen exclusively on the worker thread.
// All Qt widget / signal interactions happen exclusively on the UI thread.

#include "services/stt/WhisperService.h"
#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"

// whisper.cpp public C API
#include "whisper.h"

#include <QAudioFormat>
#include <QAudioSource>
#include <QDir>
#include <QFile>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::services {

static constexpr auto TAG = "WhisperService";

// ── Singleton ─────────────────────────────────────────────────────────────────

WhisperService& WhisperService::instance() {
    static WhisperService s_instance;
    return s_instance;
}

WhisperService::WhisperService(QObject* parent) : QObject(parent) {
    ring_buffer_.reserve(kMaxWindowSamples);
    window_buffer_.reserve(kMaxWindowSamples);

    nam_ = new QNetworkAccessManager(this);

    drain_timer_ = new QTimer(this);
    drain_timer_->setInterval(kDrainIntervalMs);
    connect(drain_timer_, &QTimer::timeout, this, &WhisperService::drain_and_infer);

    retry_timer_ = new QTimer(this);
    retry_timer_->setSingleShot(true);
    connect(retry_timer_, &QTimer::timeout, this, &WhisperService::download_model);

    LOG_INFO(TAG, "WhisperService initialised");
}

WhisperService::~WhisperService() {
    stop_listening();
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void WhisperService::start_listening() {
    if (listening_.load(std::memory_order_relaxed)) return;

    if (!model_loaded_.load(std::memory_order_relaxed)) {
        ensure_model();   // async — will call start_listening() again via model_ready
        return;
    }

    open_audio_device();
}

void WhisperService::stop_listening() {
    if (!listening_.load(std::memory_order_relaxed)) return;
    close_audio_device();
}

bool WhisperService::is_listening() const noexcept {
    return listening_.load(std::memory_order_relaxed);
}

bool WhisperService::is_model_ready() const noexcept {
    return model_loaded_.load(std::memory_order_relaxed);
}

QString WhisperService::model_path() const {
    // Co-locate with all other Fincept data under com.fincept.terminal/models/
    // to keep the install footprint in one place.
    const QString dir = python::PythonSetupManager::instance().install_dir()
                        + QStringLiteral("/models");
    return dir + u'/' + kModelFilename;
}

// static
bool WhisperService::is_model_downloaded() {
    return QFile::exists(instance().model_path());
}

void WhisperService::download_for_setup() {
    if (is_model_downloaded()) {
        // Already present — just report 100% so the setup step goes green.
        emit model_download_progress(100);
        emit model_ready();
        return;
    }
    download_attempt_ = 0;
    download_model();
}

// ── Model management ──────────────────────────────────────────────────────────

void WhisperService::ensure_model() {
    if (model_loaded_.load(std::memory_order_relaxed)) return;

    const QString path = model_path();
    if (QFile::exists(path)) {
        if (load_model()) {
            emit model_ready();
            start_listening();
        }
        return;
    }

    if (!download_active_.exchange(true, std::memory_order_acq_rel)) {
        LOG_INFO(TAG, QString("Model not found at %1 — downloading").arg(path));
        download_model();
    }
}

void WhisperService::download_model() {
    const QString dir = python::PythonSetupManager::instance().install_dir()
                        + QStringLiteral("/models");
    QDir().mkpath(dir);

    QNetworkRequest req{QUrl{QString::fromUtf8(kModelUrl)}};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = nam_->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0) {
                    const int pct = static_cast<int>(received * 100 / total);
                    emit model_download_progress(pct);
                }
            });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { on_download_finished(reply); });
}

void WhisperService::on_download_finished(QNetworkReply* reply) {
    reply->deleteLater();
    download_active_.store(false, std::memory_order_release);

    if (reply->error() != QNetworkReply::NoError) {
        const QString net_err = reply->errorString();
        ++download_attempt_;
        if (download_attempt_ <= kMaxDownloadRetries) {
            // Exponential backoff: 2s, 4s, 8s
            const int delay_ms = kRetryBaseMs * (1 << (download_attempt_ - 1));
            LOG_WARN(TAG, QString("Model download failed (attempt %1/%2): %3 — retrying in %4 ms")
                             .arg(download_attempt_).arg(kMaxDownloadRetries).arg(net_err).arg(delay_ms));
            emit model_download_progress(0); // reset progress bar
            retry_timer_->start(delay_ms);
        } else {
            const QString msg = QString("Model download failed after %1 attempts: %2")
                                    .arg(kMaxDownloadRetries).arg(net_err);
            LOG_ERROR(TAG, msg);
            download_attempt_ = 0;
            download_active_.store(false, std::memory_order_release);
            emit error_occurred(msg);
        }
        return;
    }

    // Validate download size before writing — ggml-base.en-q5_0.bin is ~57 MB.
    // A truncated response (e.g. from a proxy cut-off) would load but produce
    // garbage transcripts or crash whisper_init_from_file_with_params().
    static constexpr qint64 kMinModelBytes = 50LL * 1024 * 1024; // 50 MB floor
    const QByteArray model_data = reply->readAll();
    if (model_data.size() < kMinModelBytes) {
        const QString msg = QString("Model download incomplete: received %1 bytes (expected ≥ %2 MB)")
                                .arg(model_data.size())
                                .arg(kMinModelBytes / (1024 * 1024));
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return;
    }

    // Write atomically via QSaveFile — avoids a truncated file on crash.
    QSaveFile f{model_path()};
    if (!f.open(QIODevice::WriteOnly)) {
        const QString msg = QStringLiteral("Cannot write model to ") + model_path();
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return;
    }
    f.write(model_data);
    if (!f.commit()) {
        LOG_ERROR(TAG, "QSaveFile::commit() failed for model");
        emit error_occurred(QStringLiteral("Failed to save model file"));
        return;
    }

    download_attempt_ = 0; // reset for any future re-download
    LOG_INFO(TAG, QString("Model downloaded and saved (%1 MB)").arg(model_data.size() / (1024 * 1024)));
    emit model_download_progress(100);

    if (load_model()) {
        emit model_ready();
        start_listening();
    }
}

bool WhisperService::load_model() {
    const QString path = model_path();
    LOG_INFO(TAG, QString("Loading whisper model: %1").arg(path));

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;   // CPU-only — Vulkan opt-in left for future work

    ctx_ = whisper_init_from_file_with_params(path.toUtf8().constData(), cparams);
    if (!ctx_) {
        const QString msg = QStringLiteral("whisper_init_from_file_with_params() failed — ")
                            + QStringLiteral("model file may be corrupt: ") + path;
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return false;
    }

    model_loaded_.store(true, std::memory_order_release);
    LOG_INFO(TAG, "Whisper model loaded");
    return true;
}

// ── Audio capture ─────────────────────────────────────────────────────────────

void WhisperService::open_audio_device() {
    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Float);

    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) {
        const QString msg = QStringLiteral("No audio input device available");
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return;
    }

    // If the device doesn't natively support Float, fall back to Int16.
    // We'll convert in the callback.
    if (!dev.isFormatSupported(fmt)) {
        fmt.setSampleFormat(QAudioFormat::Int16);
        if (!dev.isFormatSupported(fmt)) {
            const QString msg = QStringLiteral("Audio device does not support 16kHz mono");
            LOG_ERROR(TAG, msg);
            emit error_occurred(msg);
            return;
        }
    }

    audio_source_ = new QAudioSource(dev, fmt, this);
    audio_source_->setBufferSize(kSampleRate / 10 * sizeof(float)); // 100 ms buffer

    // Use the IO-device pull model (compatible with all Qt6 versions including
    // those without the callback-based start() overload added in Qt 6.7).
    QIODevice* io = audio_source_->start();
    if (!io) {
        const QString msg = QStringLiteral("Failed to open audio input device");
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        audio_source_->deleteLater();
        audio_source_ = nullptr;
        return;
    }

    // Read available audio every ~50 ms via a secondary timer.
    // This keeps the audio read off the critical path while being frequent enough
    // to avoid buffer overruns (audio_source_ buffer holds 100 ms).
    auto* read_timer = new QTimer(this);
    read_timer->setObjectName(QStringLiteral("whisper_read_timer"));
    read_timer->setInterval(50);

    const bool is_float = (fmt.sampleFormat() == QAudioFormat::Float);
    connect(read_timer, &QTimer::timeout, this, [this, io, is_float]() {
        if (!io || !audio_source_) return;
        const QByteArray data = io->readAll();
        if (data.isEmpty()) return;

        if (is_float) {
            const auto* samples = reinterpret_cast<const float*>(data.constData());
            const qsizetype count = data.size() / static_cast<qsizetype>(sizeof(float));
            append_samples(samples, count);
        } else {
            // Int16 → Float32 conversion
            const auto* samples16 = reinterpret_cast<const qint16*>(data.constData());
            const qsizetype count = data.size() / static_cast<qsizetype>(sizeof(qint16));
            std::vector<float> converted(static_cast<std::size_t>(count));
            constexpr float kScale = 1.0f / 32768.0f;
            for (qsizetype i = 0; i < count; ++i)
                converted[static_cast<std::size_t>(i)] = samples16[i] * kScale;
            append_samples(converted.data(), count);
        }
    });
    read_timer->start();

    listening_.store(true, std::memory_order_release);
    drain_timer_->start();

    LOG_INFO(TAG, "Audio capture started");
    emit listening_changed(true);
}

void WhisperService::close_audio_device() {
    drain_timer_->stop();

    // Stop and destroy the read timer (child of this, found by object name).
    if (auto* t = findChild<QTimer*>(QStringLiteral("whisper_read_timer")))
        t->deleteLater();

    if (audio_source_) {
        audio_source_->stop();
        audio_source_->deleteLater();
        audio_source_ = nullptr;
    }

    // Clear buffers so the next session starts clean.
    {
        QMutexLocker lock{&ring_mutex_};
        ring_buffer_.clear();
    }
    window_buffer_.clear();

    listening_.store(false, std::memory_order_release);
    LOG_INFO(TAG, "Audio capture stopped");
    emit listening_changed(false);
}

// ── Ring buffer ───────────────────────────────────────────────────────────────

void WhisperService::append_samples(const float* data, qsizetype frames) {
    // Best-effort lock — never block the timer callback.
    if (!ring_mutex_.tryLock()) return;
    const std::size_t n = static_cast<std::size_t>(frames);
    // Clamp ring buffer to avoid unbounded growth if inference falls behind.
    if (ring_buffer_.size() + n > static_cast<std::size_t>(kMaxWindowSamples * 2))
        ring_buffer_.erase(ring_buffer_.begin(),
                           ring_buffer_.begin() +
                           static_cast<std::ptrdiff_t>(n));
    ring_buffer_.insert(ring_buffer_.end(), data, data + n);
    ring_mutex_.unlock();
}

std::vector<float> WhisperService::drain_samples() {
    QMutexLocker lock{&ring_mutex_};
    std::vector<float> out;
    out.swap(ring_buffer_);
    return out;
}

// ── VAD helper ────────────────────────────────────────────────────────────────
// Returns true if the tail of the buffer contains speech above the RMS threshold.
static bool has_speech(const std::vector<float>& buf, int tail_samples, float threshold) {
    if (buf.empty()) return false;
    const auto n = static_cast<std::ptrdiff_t>(
        std::min(static_cast<std::size_t>(tail_samples), buf.size()));
    const float* begin = buf.data() + static_cast<std::ptrdiff_t>(buf.size()) - n;
    const float rms = std::sqrt(
        std::inner_product(begin, begin + n, begin, 0.0f) / static_cast<float>(n));
    return rms > threshold;
}

// ── Inference dispatch ────────────────────────────────────────────────────────

void WhisperService::drain_and_infer() {
    if (!model_loaded_.load(std::memory_order_relaxed)) return;
    if (inference_busy_.load(std::memory_order_relaxed)) return;

    auto new_samples = drain_samples();
    if (new_samples.empty()) return;

    // Append new audio to the sliding window.
    window_buffer_.insert(window_buffer_.end(),
                          new_samples.begin(), new_samples.end());

    // Trim window to kMaxWindowSamples (keep most recent).
    if (window_buffer_.size() > static_cast<std::size_t>(kMaxWindowSamples)) {
        const std::size_t excess = window_buffer_.size()
                                   - static_cast<std::size_t>(kMaxWindowSamples);
        window_buffer_.erase(window_buffer_.begin(),
                             window_buffer_.begin() +
                             static_cast<std::ptrdiff_t>(excess));
    }

    // VAD gate — only infer if there is actual speech in the window tail.
    if (!has_speech(window_buffer_, kVadTailSamples, kVadThreshold)) return;

    // Take a snapshot for the worker thread — do NOT pass pointers to members.
    std::vector<float> snapshot = window_buffer_;

    inference_busy_.store(true, std::memory_order_release);

    QPointer<WhisperService> self = this;
    QtConcurrent::run([self, snapshot = std::move(snapshot)]() mutable {
        if (!self) return;
        self->run_inference(std::move(snapshot));
    });
}

void WhisperService::run_inference(std::vector<float> samples) {
    // This method runs on a QtConcurrent worker thread.
    // MUST NOT access any QWidget or emit signals directly —
    // all UI interaction goes via QMetaObject::invokeMethod.

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.n_threads        = kInferenceThreads;
    wparams.single_segment   = false;
    wparams.print_realtime   = false;
    wparams.print_progress   = false;
    wparams.print_timestamps = false;
    wparams.print_special    = false;
    wparams.translate        = false;
    wparams.language         = "en";
    // Reduce audio context from 1500 (30 s) to 256 (~5 s) for lower latency.
    wparams.audio_ctx        = 256;
    wparams.no_context       = false;  // keep prior context for continuity

    const int rc = whisper_full(
        ctx_,
        wparams,
        samples.data(),
        static_cast<int>(samples.size()));

    inference_busy_.store(false, std::memory_order_release);

    if (rc != 0) {
        LOG_WARN(TAG, QString("whisper_full() returned %1").arg(rc));
        return;
    }

    // Collect all segments into a single string.
    QString text;
    const int n_seg = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_seg; ++i) {
        const char* seg_text = whisper_full_get_segment_text(ctx_, i);
        if (seg_text) text += QString::fromUtf8(seg_text);
    }

    text = text.trimmed();

    // Strip whisper hallucination tokens (background noise artifacts).
    static const QStringList kHallucinationTokens{
        QStringLiteral("[BLANK_AUDIO]"),
        QStringLiteral("[ Silence ]"),
        QStringLiteral("(silence)"),
        QStringLiteral("[silence]"),
        QStringLiteral("(Music)"),
        QStringLiteral("[Music]"),
        QStringLiteral("[ Music ]"),
        QStringLiteral("(Applause)"),
        QStringLiteral("Thanks for watching!"),
        QStringLiteral("Thank you for watching."),
    };
    for (const QString& token : kHallucinationTokens) {
        if (text.contains(token, Qt::CaseInsensitive)) {
            text.clear();
            break;
        }
    }

    if (text.isEmpty()) return;

    LOG_DEBUG(TAG, QString("Transcript: \"%1\"").arg(text));

    QPointer<WhisperService> self = this;
    QMetaObject::invokeMethod(self, [self, text]() {
        if (self) emit self->transcription_ready(text);
    }, Qt::QueuedConnection);
}

} // namespace fincept::services
