// SpeechService.cpp — Pluggable speech-to-text (Google or Deepgram).
//
// Architecture:
//   SpeechService (public facade)
//     └── SttProvider (abstract)
//           ├── PythonSttProvider (shared QProcess + JSON-lines plumbing)
//           │     ├── GoogleSttProvider   → scripts/voice/speech_to_text.py
//           │     └── DeepgramSttProvider → scripts/voice/deepgram_stt.py
//
// Both providers share the same JSON-lines stdout protocol:
//   {"status": "calibrating"|"listening"|"stopped"}
//   {"text": "..."}        → transcription
//   {"error": "..."}       → recoverable error
//   {"fatal": "..."}       → unrecoverable error (process exits)

#include "services/stt/SpeechService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace fincept::services {

static constexpr auto TAG = "SpeechService";
static constexpr int kShutdownTimeoutMs = 2000;

// ── PythonSttProvider (base) ─────────────────────────────────────────────────
//
// Shared plumbing for any STT provider implemented as a Python QProcess that
// emits JSON lines on stdout. Subclasses contribute script_path() and any
// extra environment variables via extend_environment().

namespace {

class PythonSttProvider : public SttProvider {
  public:
    using SttProvider::SttProvider;

    void start() override {
        LOG_INFO(TAG, QString("PythonSttProvider[%1]::start() — entry").arg(name()));
        if (process_) {
            LOG_WARN(TAG, QString("PythonSttProvider[%1]: start() called but process_ already exists — ignoring")
                              .arg(name()));
            return;
        }

        QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
        LOG_INFO(TAG, QString("Resolving python_exe — venv-numpy2 path: '%1' (exists=%2)")
                          .arg(python_exe).arg(QFileInfo::exists(python_exe)));
        if (python_exe.isEmpty() || !QFileInfo::exists(python_exe)) {
            const QString fallback = python::PythonRunner::instance().python_path();
            LOG_WARN(TAG, QString("venv-numpy2 python missing — falling back to PythonRunner default: '%1'")
                              .arg(fallback));
            python_exe = fallback;
        }

        if (python_exe.isEmpty()) {
            LOG_ERROR(TAG, "No Python interpreter available — aborting STT start");
            emit fatal_error(QStringLiteral("Python not available — cannot start voice input"));
            return;
        }

        const QString script = script_path();
        LOG_INFO(TAG, QString("Resolving script — path: '%1' (exists=%2)")
                          .arg(script).arg(QFileInfo::exists(script)));
        if (!QFileInfo::exists(script)) {
            LOG_ERROR(TAG, QString("STT script not found at '%1' — aborting").arg(script));
            emit fatal_error(QStringLiteral("Voice script not found: ") + script);
            return;
        }

        process_ = new QProcess(this);

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");
        env.insert("PYTHONDONTWRITEBYTECODE", "1");
        env.insert("PYTHONUNBUFFERED", "1");
        env.insert("FINCEPT_DATA_DIR", python::PythonSetupManager::instance().install_dir());

        const QString scripts_dir = python::PythonRunner::instance().scripts_dir();
        const QString existing_pypath = env.value("PYTHONPATH");
#ifdef _WIN32
        const QChar kPathSep = ';';
#else
        const QChar kPathSep = ':';
#endif
        const QString new_pypath =
            existing_pypath.isEmpty() ? scripts_dir : (scripts_dir + kPathSep + existing_pypath);
        env.insert("PYTHONPATH", new_pypath);

        // Give subclass a chance to inject API keys / model / language
        extend_environment(env);

        process_->setProcessEnvironment(env);
        process_->setWorkingDirectory(scripts_dir);
        // Capture stderr so Python tracebacks / diagnostic prints surface in
        // the C++ log instead of being silently dropped.
        process_->setProcessChannelMode(QProcess::SeparateChannels);

#ifdef _WIN32
        process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        connect(process_, &QProcess::readyReadStandardOutput, this, &PythonSttProvider::on_stdout_ready);
        connect(process_, &QProcess::readyReadStandardError, this, &PythonSttProvider::on_stderr_ready);
        connect(process_, &QProcess::started, this, [this]() {
            LOG_INFO(TAG, QString("Provider[%1]: QProcess::started — pid=%2")
                              .arg(name()).arg(process_ ? process_->processId() : 0));
        });
        connect(process_, &QProcess::finished, this, &PythonSttProvider::on_process_finished);
        connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            LOG_ERROR(TAG, QString("Provider[%1]: QProcess::errorOccurred err=%2")
                               .arg(name()).arg(static_cast<int>(err)));
            if (err == QProcess::FailedToStart) {
                emit fatal_error(QStringLiteral("Failed to start voice recognition process"));
                stop();
            }
        });

        stdout_buffer_.clear();
        LOG_INFO(TAG, QString("Provider[%1]: launching '%2' '%3' (cwd='%4')")
                          .arg(name(), python_exe, script, scripts_dir));
        process_->start(python_exe, {script});

        active_.store(true, std::memory_order_release);
        LOG_INFO(TAG, QString("Voice provider '%1' marked active — emitting active_changed(true)").arg(name()));
        emit active_changed(true);
    }

    void stop() override {
        if (!process_)
            return;

        disconnect(process_, nullptr, this, nullptr);

        if (process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(kShutdownTimeoutMs);
        }
        process_->deleteLater();
        process_ = nullptr;
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            LOG_INFO(TAG, QString("Voice provider '%1' stopped").arg(name()));
            emit active_changed(false);
        }
    }

    [[nodiscard]] bool is_active() const noexcept override {
        return active_.load(std::memory_order_relaxed);
    }

  protected:
    /// Absolute path to the Python script that implements the JSON-lines protocol.
    virtual QString script_path() const = 0;

    /// Hook for subclasses to add API keys / model / language etc. to the
    /// child process environment. Default: no extra vars.
    virtual void extend_environment(QProcessEnvironment& /*env*/) const {}

  private:
    void on_stdout_ready() {
        if (!process_)
            return;

        stdout_buffer_.append(process_->readAllStandardOutput());

        while (true) {
            const int newline_pos = stdout_buffer_.indexOf('\n');
            if (newline_pos < 0)
                break;

            const QByteArray line = stdout_buffer_.left(newline_pos).trimmed();
            stdout_buffer_.remove(0, newline_pos + 1);

            if (!line.isEmpty()) {
                LOG_DEBUG(TAG, QString("Provider[%1] stdout: %2")
                                   .arg(name(), QString::fromUtf8(line.left(400))));
                parse_line(line);
            }
        }
    }

    /// Stderr from the Python child — Python tracebacks, deepgram-sdk warnings,
    /// and any `print(..., file=sys.stderr)` lines land here. Logged so a
    /// failing voice session is diagnosable from Logger output.
    void on_stderr_ready() {
        if (!process_)
            return;
        const QByteArray chunk = process_->readAllStandardError();
        if (chunk.isEmpty())
            return;
        // Strip trailing newlines for cleaner log lines but preserve embedded
        // ones — multi-line tracebacks read better in the log file than a
        // single mashed-together line.
        const QString text = QString::fromUtf8(chunk).trimmed();
        if (!text.isEmpty()) {
            LOG_WARN(TAG, QString("Provider[%1] stderr: %2").arg(name(), text));
        }
    }

    void parse_line(const QByteArray& line) {
        QJsonParseError parse_err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_err);
        if (parse_err.error != QJsonParseError::NoError) {
            LOG_WARN(TAG, QString("Provider[%1]: non-JSON stdout line ignored: %2")
                              .arg(name(), QString::fromUtf8(line)));
            return;
        }

        const QJsonObject obj = doc.object();

        if (obj.contains("text")) {
            const QString text = obj["text"].toString().trimmed();
            if (!text.isEmpty()) {
                LOG_INFO(TAG, QString("Provider[%1]: transcript len=%2 \"%3\"")
                                  .arg(name()).arg(text.length()).arg(text.left(120)));
                emit transcription(text);
            } else {
                LOG_WARN(TAG, QString("Provider[%1]: empty transcript object").arg(name()));
            }
        } else if (obj.contains("error")) {
            const QString msg = obj["error"].toString();
            LOG_WARN(TAG, QString("Provider[%1] STT error event: %2").arg(name(), msg));
            emit error(msg);
        } else if (obj.contains("fatal")) {
            const QString msg = obj["fatal"].toString();
            LOG_ERROR(TAG, QString("Provider[%1] STT FATAL event: %2 — tearing down provider")
                               .arg(name(), msg));
            emit fatal_error(msg);
            stop();
        } else if (obj.contains("status")) {
            const QString status = obj["status"].toString();
            LOG_INFO(TAG, QString("Provider[%1] STT status: %2").arg(name(), status));
        } else {
            LOG_WARN(TAG, QString("Provider[%1]: JSON line had no recognised key — %2")
                              .arg(name(), QString::fromUtf8(line.left(200))));
        }
    }

    void on_process_finished(int exit_code, QProcess::ExitStatus status) {
        // Drain any final stderr content before the QProcess is deleted —
        // last-gasp Python tracebacks otherwise vanish.
        if (process_) {
            const QByteArray remaining_err = process_->readAllStandardError();
            if (!remaining_err.isEmpty()) {
                LOG_WARN(TAG, QString("Provider[%1] final stderr: %2")
                                  .arg(name(), QString::fromUtf8(remaining_err).trimmed()));
            }
            const QByteArray remaining_out = process_->readAllStandardOutput();
            if (!remaining_out.isEmpty()) {
                LOG_INFO(TAG, QString("Provider[%1] final stdout: %2")
                                  .arg(name(), QString::fromUtf8(remaining_out).trimmed()));
            }
        }

        LOG_INFO(TAG, QString("Voice provider '%1' process exited (code=%2, status=%3, was_active=%4)")
                          .arg(name())
                          .arg(exit_code)
                          .arg(status == QProcess::CrashExit ? "crash" : "normal")
                          .arg(active_.load(std::memory_order_relaxed)));

        if (process_) {
            process_->deleteLater();
            process_ = nullptr;
        }
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            emit active_changed(false);
            if (status == QProcess::CrashExit || exit_code != 0) {
                LOG_ERROR(TAG, QString("Provider[%1] exited abnormally (code=%2) — surfacing error to UI")
                                   .arg(name()).arg(exit_code));
                emit error(QStringLiteral("Voice recognition stopped unexpectedly"));
            }
        }
    }

    QProcess* process_ = nullptr;
    QByteArray stdout_buffer_;
    std::atomic<bool> active_{false};
};

// ── GoogleSttProvider ────────────────────────────────────────────────────────
//
// Unauth'd Google Web Speech via Python `speech_recognition`. No API key.

class GoogleSttProvider final : public PythonSttProvider {
  public:
    using PythonSttProvider::PythonSttProvider;

    QString name() const override { return QStringLiteral("google"); }

  protected:
    QString script_path() const override {
        return python::PythonRunner::instance().scripts_dir() +
               QStringLiteral("/voice/speech_to_text.py");
    }
};

// ── DeepgramSttProvider ──────────────────────────────────────────────────────
//
// Deepgram live WebSocket STT via Python `deepgram-sdk`. Reads API key from
// SecureStorage (key "voice.deepgram.api_key"). Model/language/keyterms come
// from AppConfig.

class DeepgramSttProvider final : public PythonSttProvider {
  public:
    using PythonSttProvider::PythonSttProvider;

    QString name() const override { return QStringLiteral("deepgram"); }

  protected:
    QString script_path() const override {
        return python::PythonRunner::instance().scripts_dir() +
               QStringLiteral("/voice/deepgram_stt.py");
    }

    void extend_environment(QProcessEnvironment& env) const override {
        auto key_res = SecureStorage::instance().retrieve(QStringLiteral("voice.deepgram.api_key"));
        const QString api_key = key_res.is_ok() ? key_res.value() : QString();

        if (key_res.is_err()) {
            LOG_ERROR(TAG, QString("Deepgram: SecureStorage::retrieve failed — %1")
                               .arg(QString::fromStdString(key_res.error())));
        }

        auto& cfg = AppConfig::instance();
        const QString model = cfg.get("voice/deepgram/model", "nova-3").toString();
        const QString lang = cfg.get("voice/deepgram/language", "en").toString();
        const QString keyterms = cfg.get("voice/deepgram/keyterms", "").toString();
        // Audio capture knobs — laptop mics often need 3-5x gain; far-field
        // setups may need a specific device by name. Defaults match script.
        const QString gain = cfg.get("voice/deepgram/gain", "3.0").toString();
        const QString device = cfg.get("voice/deepgram/device", "").toString();

        // Never log the key itself — only its length, so we can tell at a
        // glance whether SecureStorage returned anything.
        LOG_INFO(TAG, QString("Deepgram env: api_key_len=%1 model='%2' language='%3' "
                              "keyterms_len=%4 gain=%5 device='%6'")
                          .arg(api_key.length()).arg(model, lang)
                          .arg(keyterms.length()).arg(gain, device));

        env.insert("DEEPGRAM_API_KEY", api_key);
        env.insert("FINCEPT_STT_MODEL", model);
        env.insert("FINCEPT_STT_LANGUAGE", lang);
        env.insert("FINCEPT_STT_KEYTERMS", keyterms);
        env.insert("FINCEPT_STT_GAIN", gain);
        env.insert("FINCEPT_STT_DEVICE", device);
    }
};

} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

SpeechService& SpeechService::instance() {
    static SpeechService s_instance;
    return s_instance;
}

SpeechService::SpeechService(QObject* parent) : QObject(parent) {
    LOG_INFO(TAG, "SpeechService initialised");
}

SpeechService::~SpeechService() {
    teardown_provider();
}

// ── Public API ───────────────────────────────────────────────────────────────

QString SpeechService::configured_provider() {
    auto& cfg = AppConfig::instance();
    // Canonical key is "voice/stt/provider"; fall back to legacy "voice/provider"
    // for users who saved settings before TTS provider was split out.
    QString v = cfg.get("voice/stt/provider", "").toString().toLower();
    if (v.isEmpty())
        v = cfg.get("voice/provider", "google").toString().toLower();
    return (v == "deepgram") ? QStringLiteral("deepgram") : QStringLiteral("google");
}

void SpeechService::start_listening() {
    LOG_INFO(TAG, QString("SpeechService::start_listening() — listening_=%1 provider=%2")
                      .arg(listening_.load(std::memory_order_relaxed))
                      .arg(provider_ ? provider_->name() : QStringLiteral("<none>")));
    if (listening_.load(std::memory_order_relaxed)) {
        LOG_WARN(TAG, "start_listening: already listening — ignoring");
        return;
    }

    install_provider();
    if (!provider_) {
        LOG_ERROR(TAG, "start_listening: install_provider() left provider_ null — cannot start");
        emit error_occurred(QStringLiteral("Voice provider could not be installed"));
        return;
    }

    LOG_INFO(TAG, QString("start_listening: delegating to provider '%1'").arg(provider_->name()));
    provider_->start();
}

void SpeechService::stop_listening() {
    LOG_INFO(TAG, QString("SpeechService::stop_listening() — listening_=%1 provider=%2")
                      .arg(listening_.load(std::memory_order_relaxed))
                      .arg(provider_ ? provider_->name() : QStringLiteral("<none>")));
    if (!listening_.load(std::memory_order_relaxed)) {
        LOG_INFO(TAG, "stop_listening: not active — no-op");
        return;
    }
    if (provider_)
        provider_->stop();
}

void SpeechService::reload_config() {
    const bool was_listening = listening_.load(std::memory_order_relaxed);

    if (was_listening && provider_)
        provider_->stop();

    // install_provider() compares names — if the provider id didn't change,
    // the same instance is kept (and will re-read env on next start(), which
    // picks up any Deepgram API-key / model / language / keyterms changes).
    install_provider();

    if (was_listening && provider_) {
        LOG_INFO(TAG, "Voice config reloaded — restarting active session");
        provider_->start();
    } else {
        LOG_INFO(TAG, "Voice config reloaded (idle)");
    }
}

bool SpeechService::is_listening() const noexcept {
    return listening_.load(std::memory_order_relaxed);
}

// ── Provider lifecycle ───────────────────────────────────────────────────────

void SpeechService::install_provider() {
    const QString id = configured_provider();
    LOG_INFO(TAG, QString("install_provider() — configured='%1' current='%2'")
                      .arg(id, provider_ ? provider_->name() : QStringLiteral("<none>")));

    // If an existing provider matches the selection, reuse it.
    if (provider_ && provider_->name() == id) {
        LOG_INFO(TAG, "install_provider: reusing existing provider instance");
        return;
    }

    teardown_provider();

    if (id == QStringLiteral("deepgram"))
        provider_ = std::make_unique<DeepgramSttProvider>(this);
    else
        provider_ = std::make_unique<GoogleSttProvider>(this);

    connect(provider_.get(), &SttProvider::transcription, this, &SpeechService::transcription_ready);
    connect(provider_.get(), &SttProvider::error, this, &SpeechService::error_occurred);
    connect(provider_.get(), &SttProvider::fatal_error, this, &SpeechService::error_occurred);
    connect(provider_.get(), &SttProvider::active_changed, this, [this](bool active) {
        LOG_INFO(TAG, QString("Provider active_changed — active=%1 (forwarding listening_changed)")
                          .arg(active));
        listening_.store(active, std::memory_order_release);
        emit listening_changed(active);
    });

    LOG_INFO(TAG, QString("Voice provider installed: %1").arg(id));
}

void SpeechService::teardown_provider() {
    if (!provider_)
        return;
    provider_->stop();
    disconnect(provider_.get(), nullptr, this, nullptr);
    provider_.reset();
}

} // namespace fincept::services
