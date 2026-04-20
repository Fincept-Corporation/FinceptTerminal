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
        if (process_)
            return;

        QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
        if (python_exe.isEmpty() || !QFileInfo::exists(python_exe))
            python_exe = python::PythonRunner::instance().python_path();

        if (python_exe.isEmpty()) {
            emit fatal_error(QStringLiteral("Python not available — cannot start voice input"));
            return;
        }

        const QString script = script_path();
        if (!QFileInfo::exists(script)) {
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

#ifdef _WIN32
        process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        connect(process_, &QProcess::readyReadStandardOutput, this, &PythonSttProvider::on_stdout_ready);
        connect(process_, &QProcess::finished, this, &PythonSttProvider::on_process_finished);
        connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart) {
                emit fatal_error(QStringLiteral("Failed to start voice recognition process"));
                stop();
            }
        });

        stdout_buffer_.clear();
        process_->start(python_exe, {script});

        active_.store(true, std::memory_order_release);
        LOG_INFO(TAG, QString("Voice provider '%1' started").arg(name()));
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

            if (!line.isEmpty())
                parse_line(line);
        }
    }

    void parse_line(const QByteArray& line) {
        QJsonParseError parse_err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_err);
        if (parse_err.error != QJsonParseError::NoError) {
            LOG_WARN(TAG, QString("Non-JSON stdout line: %1").arg(QString::fromUtf8(line)));
            return;
        }

        const QJsonObject obj = doc.object();

        if (obj.contains("text")) {
            const QString text = obj["text"].toString().trimmed();
            if (!text.isEmpty()) {
                LOG_DEBUG(TAG, QString("Transcript: \"%1\"").arg(text));
                emit transcription(text);
            }
        } else if (obj.contains("error")) {
            const QString msg = obj["error"].toString();
            LOG_WARN(TAG, QString("STT error: %1").arg(msg));
            emit error(msg);
        } else if (obj.contains("fatal")) {
            const QString msg = obj["fatal"].toString();
            LOG_ERROR(TAG, QString("STT fatal: %1").arg(msg));
            emit fatal_error(msg);
            stop();
        } else if (obj.contains("status")) {
            const QString status = obj["status"].toString();
            LOG_DEBUG(TAG, QString("STT status: %1").arg(status));
        }
    }

    void on_process_finished(int exit_code, QProcess::ExitStatus status) {
        LOG_INFO(TAG, QString("Voice provider '%1' process exited (code=%2, status=%3)")
                          .arg(name())
                          .arg(exit_code)
                          .arg(status == QProcess::CrashExit ? "crash" : "normal"));

        if (process_) {
            process_->deleteLater();
            process_ = nullptr;
        }
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            emit active_changed(false);
            if (status == QProcess::CrashExit || exit_code != 0)
                emit error(QStringLiteral("Voice recognition stopped unexpectedly"));
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
        env.insert("DEEPGRAM_API_KEY", api_key);

        auto& cfg = AppConfig::instance();
        env.insert("FINCEPT_STT_MODEL", cfg.get("voice/deepgram/model", "nova-3").toString());
        env.insert("FINCEPT_STT_LANGUAGE", cfg.get("voice/deepgram/language", "en").toString());
        env.insert("FINCEPT_STT_KEYTERMS", cfg.get("voice/deepgram/keyterms", "").toString());
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
    const QString v = AppConfig::instance().get("voice/provider", "google").toString().toLower();
    return (v == "deepgram") ? QStringLiteral("deepgram") : QStringLiteral("google");
}

void SpeechService::start_listening() {
    if (listening_.load(std::memory_order_relaxed))
        return;

    install_provider();
    if (!provider_)
        return;

    provider_->start();
}

void SpeechService::stop_listening() {
    if (!listening_.load(std::memory_order_relaxed))
        return;
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

    // If an existing provider matches the selection, reuse it.
    if (provider_ && provider_->name() == id)
        return;

    teardown_provider();

    if (id == QStringLiteral("deepgram"))
        provider_ = std::make_unique<DeepgramSttProvider>(this);
    else
        provider_ = std::make_unique<GoogleSttProvider>(this);

    connect(provider_.get(), &SttProvider::transcription, this, &SpeechService::transcription_ready);
    connect(provider_.get(), &SttProvider::error, this, &SpeechService::error_occurred);
    connect(provider_.get(), &SttProvider::fatal_error, this, &SpeechService::error_occurred);
    connect(provider_.get(), &SttProvider::active_changed, this, [this](bool active) {
        listening_.store(active, std::memory_order_release);
        emit listening_changed(active);
    });

    LOG_INFO(TAG, QString("Voice provider selected: %1").arg(id));
}

void SpeechService::teardown_provider() {
    if (!provider_)
        return;
    provider_->stop();
    disconnect(provider_.get(), nullptr, this, nullptr);
    provider_.reset();
}

} // namespace fincept::services
