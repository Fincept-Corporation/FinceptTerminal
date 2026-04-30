// TtsService.cpp — Pluggable text-to-speech (pyttsx3 free + Deepgram Aura).
//
// Two providers share a common Python-QProcess + JSON-stdout plumbing.
// Subclass overrides only `script_path()` and (optionally)
// `extend_environment()` to inject API keys / voice models.
//
// Logging mirrors SpeechService — every state transition + every line of
// stdin/stdout/stderr is captured so failures are diagnosable from the log.

#include "services/tts/TtsService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::services {

// Use uniquely-prefixed names — unity build merges multiple .cpp files into a
// single TU, so generic names like TAG / kShutdownTimeoutMs would clash with
// SpeechService's identically-named constants.
namespace {
constexpr auto TTS_TAG = "TtsService";
constexpr int  kTtsShutdownTimeoutMs = 1500;
} // namespace

// ── PythonTtsProvider (base) ─────────────────────────────────────────────────
//
// Shared plumbing for any TTS provider implemented as a Python QProcess that
// reads text from stdin and emits JSON lines on stdout. Subclasses contribute
// `script_path()` and any extra env vars via `extend_environment()`.

namespace {

class PythonTtsProvider : public TtsProvider {
  public:
    using TtsProvider::TtsProvider;

    void speak(const QString& text) override {
        LOG_INFO(TTS_TAG, QString("Provider[%1]::speak() — len=%2 active=%3")
                              .arg(name()).arg(text.length()).arg(active_.load()));

        if (text.trimmed().isEmpty()) {
            LOG_INFO(TTS_TAG, "speak: empty text — emitting finished immediately");
            emit speaking_finished();
            return;
        }

        // Cancel any in-flight utterance — matches QTextToSpeech::say().
        if (process_) {
            LOG_INFO(TTS_TAG, "speak: cancelling previous utterance");
            stop();
        }

        QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
        LOG_INFO(TTS_TAG, QString("Resolving python_exe — venv-numpy2: '%1' (exists=%2)")
                              .arg(python_exe).arg(QFileInfo::exists(python_exe)));
        if (python_exe.isEmpty() || !QFileInfo::exists(python_exe))
            python_exe = python::PythonRunner::instance().python_path();

        if (python_exe.isEmpty()) {
            LOG_ERROR(TTS_TAG, "No Python interpreter available — aborting TTS");
            emit error_occurred(QStringLiteral("Python not available — cannot speak response"));
            return;
        }

        const QString script = script_path();
        LOG_INFO(TTS_TAG, QString("Resolving script — '%1' (exists=%2)")
                              .arg(script).arg(QFileInfo::exists(script)));
        if (!QFileInfo::exists(script)) {
            LOG_ERROR(TTS_TAG, QString("TTS script not found at '%1'").arg(script));
            emit error_occurred(QStringLiteral("TTS script not found: ") + script);
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
        env.insert("PYTHONPATH", existing_pypath.isEmpty()
                                     ? scripts_dir
                                     : (scripts_dir + kPathSep + existing_pypath));

        // Provider-specific env (api keys, voice models, rates).
        extend_environment(env);

        process_->setProcessEnvironment(env);
        process_->setWorkingDirectory(scripts_dir);
        process_->setProcessChannelMode(QProcess::SeparateChannels);

#ifdef _WIN32
        process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        connect(process_, &QProcess::readyReadStandardOutput, this, &PythonTtsProvider::on_stdout_ready);
        connect(process_, &QProcess::readyReadStandardError,  this, &PythonTtsProvider::on_stderr_ready);
        connect(process_, &QProcess::started, this, [this]() {
            LOG_INFO(TTS_TAG, QString("Provider[%1]: QProcess::started — pid=%2")
                                  .arg(name()).arg(process_ ? process_->processId() : 0));
        });
        connect(process_, &QProcess::finished, this, &PythonTtsProvider::on_process_finished);
        connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            LOG_ERROR(TTS_TAG, QString("Provider[%1]: QProcess::errorOccurred err=%2")
                                   .arg(name()).arg(static_cast<int>(err)));
            if (err == QProcess::FailedToStart) {
                emit error_occurred(QStringLiteral("Failed to launch TTS process"));
                stop();
            }
        });

        stdout_buffer_.clear();
        LOG_INFO(TTS_TAG, QString("Provider[%1]: launching '%2' '%3' (cwd='%4')")
                              .arg(name(), python_exe, script, scripts_dir));
        process_->start(python_exe, {script});

        if (!process_->waitForStarted(2000)) {
            LOG_ERROR(TTS_TAG, "TTS process did not start within 2 s");
            emit error_occurred(QStringLiteral("TTS process failed to start"));
            stop();
            return;
        }

        // Pipe text on stdin and close so the child sees EOF.
        const QByteArray payload = text.toUtf8();
        const qint64 written = process_->write(payload);
        process_->closeWriteChannel();
        LOG_INFO(TTS_TAG, QString("Provider[%1]: wrote %2/%3 bytes to stdin")
                              .arg(name()).arg(written).arg(payload.size()));
    }

    void stop() override {
        if (!process_)
            return;

        disconnect(process_, nullptr, this, nullptr);
        if (process_->state() != QProcess::NotRunning) {
            process_->kill();
            process_->waitForFinished(kTtsShutdownTimeoutMs);
        }
        process_->deleteLater();
        process_ = nullptr;
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            LOG_INFO(TTS_TAG, QString("Provider[%1]: stop — was_active=true → speaking_finished")
                                  .arg(name()));
            emit speaking_finished();
        }
    }

    [[nodiscard]] bool is_active() const noexcept override {
        return active_.load(std::memory_order_relaxed);
    }

  protected:
    /// Absolute path to the Python script that implements the JSON-lines protocol.
    virtual QString script_path() const = 0;

    /// Hook for subclasses to add API keys / voice / model env vars.
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
                LOG_DEBUG(TTS_TAG, QString("Provider[%1] stdout: %2")
                                       .arg(name(), QString::fromUtf8(line.left(400))));
                parse_line(line);
            }
        }
    }

    void on_stderr_ready() {
        if (!process_)
            return;
        const QByteArray chunk = process_->readAllStandardError();
        if (chunk.isEmpty())
            return;
        const QString text = QString::fromUtf8(chunk).trimmed();
        if (!text.isEmpty()) {
            LOG_WARN(TTS_TAG, QString("Provider[%1] stderr: %2").arg(name(), text));
        }
    }

    void parse_line(const QByteArray& line) {
        QJsonParseError parse_err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_err);
        if (parse_err.error != QJsonParseError::NoError) {
            LOG_WARN(TTS_TAG, QString("Provider[%1] non-JSON stdout: %2")
                                  .arg(name(), QString::fromUtf8(line)));
            return;
        }
        const QJsonObject obj = doc.object();

        if (obj.contains("status")) {
            const QString status = obj["status"].toString();
            LOG_INFO(TTS_TAG, QString("Provider[%1] status: %2").arg(name(), status));
            if (status == "speaking") {
                active_.store(true, std::memory_order_release);
                emit speaking_started();
            } else if (status == "done") {
                const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
                if (was_active)
                    emit speaking_finished();
            }
        } else if (obj.contains("error")) {
            const QString msg = obj["error"].toString();
            LOG_WARN(TTS_TAG, QString("Provider[%1] error: %2").arg(name(), msg));
            emit error_occurred(msg);
        } else if (obj.contains("fatal")) {
            const QString msg = obj["fatal"].toString();
            LOG_ERROR(TTS_TAG, QString("Provider[%1] fatal: %2").arg(name(), msg));
            emit error_occurred(msg);
        }
    }

    void on_process_finished(int exit_code, QProcess::ExitStatus status) {
        if (process_) {
            const QByteArray remaining_err = process_->readAllStandardError();
            if (!remaining_err.isEmpty())
                LOG_WARN(TTS_TAG, QString("Provider[%1] final stderr: %2")
                                       .arg(name(), QString::fromUtf8(remaining_err).trimmed()));
            const QByteArray remaining_out = process_->readAllStandardOutput();
            if (!remaining_out.isEmpty())
                LOG_INFO(TTS_TAG, QString("Provider[%1] final stdout: %2")
                                       .arg(name(), QString::fromUtf8(remaining_out).trimmed()));
        }

        LOG_INFO(TTS_TAG, QString("Provider[%1] process exited (code=%2, status=%3, was_active=%4)")
                              .arg(name())
                              .arg(exit_code)
                              .arg(status == QProcess::CrashExit ? "crash" : "normal")
                              .arg(active_.load()));

        if (process_) {
            process_->deleteLater();
            process_ = nullptr;
        }
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            emit speaking_finished();
            if (status == QProcess::CrashExit || exit_code != 0) {
                LOG_ERROR(TTS_TAG, QString("Provider[%1] exited abnormally (code=%2)")
                                       .arg(name()).arg(exit_code));
                emit error_occurred(QStringLiteral("Voice response stopped unexpectedly"));
            }
        }
    }

    QProcess*         process_ = nullptr;
    QByteArray        stdout_buffer_;
    std::atomic<bool> active_{false};
};

// ── Pyttsx3TtsProvider ───────────────────────────────────────────────────────
//
// Free, offline. Wraps SAPI/NSSpeech/espeak via pyttsx3.

class Pyttsx3TtsProvider final : public PythonTtsProvider {
  public:
    using PythonTtsProvider::PythonTtsProvider;
    QString name() const override { return QStringLiteral("pyttsx3"); }

  protected:
    QString script_path() const override {
        return python::PythonRunner::instance().scripts_dir() +
               QStringLiteral("/voice/tts.py");
    }
};

// ── DeepgramTtsProvider ──────────────────────────────────────────────────────
//
// POSTs text to Deepgram /v1/speak (Aura-2 voices) and pipes returned PCM
// straight into pyaudio. Reads API key from SecureStorage; voice model from
// AppConfig key "voice/deepgram/tts_model".

class DeepgramTtsProvider final : public PythonTtsProvider {
  public:
    using PythonTtsProvider::PythonTtsProvider;
    QString name() const override { return QStringLiteral("deepgram"); }

  protected:
    QString script_path() const override {
        return python::PythonRunner::instance().scripts_dir() +
               QStringLiteral("/voice/deepgram_tts.py");
    }

    void extend_environment(QProcessEnvironment& env) const override {
        auto key_res = SecureStorage::instance().retrieve(QStringLiteral("voice.deepgram.api_key"));
        const QString api_key = key_res.is_ok() ? key_res.value() : QString();
        if (key_res.is_err()) {
            LOG_ERROR(TTS_TAG, QString("Deepgram TTS: SecureStorage::retrieve failed — %1")
                                   .arg(QString::fromStdString(key_res.error())));
        }

        const QString model = AppConfig::instance()
                                  .get("voice/deepgram/tts_model", "aura-2-thalia-en")
                                  .toString();

        LOG_INFO(TTS_TAG, QString("Deepgram TTS env: api_key_len=%1 model='%2'")
                              .arg(api_key.length()).arg(model));

        env.insert("DEEPGRAM_API_KEY", api_key);
        env.insert("FINCEPT_TTS_DG_MODEL", model);
    }
};

} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

TtsService& TtsService::instance() {
    static TtsService s_instance;
    return s_instance;
}

TtsService::TtsService(QObject* parent) : QObject(parent) {
    LOG_INFO(TTS_TAG, "TtsService initialised");
}

TtsService::~TtsService() {
    teardown_provider();
}

bool TtsService::is_speaking() const noexcept {
    return speaking_.load(std::memory_order_relaxed);
}

QString TtsService::configured_provider() {
    const QString v = AppConfig::instance()
                          .get("voice/tts/provider", "pyttsx3")
                          .toString().toLower();
    return (v == "deepgram") ? QStringLiteral("deepgram") : QStringLiteral("pyttsx3");
}

void TtsService::speak(const QString& text) {
    LOG_INFO(TTS_TAG, QString("speak() — text_len=%1 currently_speaking=%2 provider=%3")
                          .arg(text.length()).arg(is_speaking())
                          .arg(provider_ ? provider_->name() : QStringLiteral("<none>")));

    install_provider();
    if (!provider_) {
        LOG_ERROR(TTS_TAG, "speak: install_provider() left provider_ null — cannot speak");
        emit error_occurred(QStringLiteral("TTS provider could not be installed"));
        return;
    }
    provider_->speak(text);
}

void TtsService::stop() {
    LOG_INFO(TTS_TAG, QString("stop() — speaking=%1").arg(is_speaking()));
    if (provider_)
        provider_->stop();
}

void TtsService::reload_config() {
    const bool was_speaking = is_speaking();
    if (was_speaking && provider_)
        provider_->stop();

    install_provider();
    LOG_INFO(TTS_TAG, "reload_config: provider re-evaluated");
}

void TtsService::install_provider() {
    const QString id = configured_provider();
    if (provider_ && provider_->name() == id) {
        // Reuse existing instance — env vars are rebuilt every speak() call,
        // so any AppConfig changes (api key, model) take effect on next speak.
        return;
    }

    LOG_INFO(TTS_TAG, QString("install_provider — switching from '%1' to '%2'")
                          .arg(provider_ ? provider_->name() : QStringLiteral("<none>"), id));
    teardown_provider();

    if (id == QStringLiteral("deepgram"))
        provider_ = std::make_unique<DeepgramTtsProvider>(this);
    else
        provider_ = std::make_unique<Pyttsx3TtsProvider>(this);

    connect(provider_.get(), &TtsProvider::speaking_started, this, [this]() {
        speaking_.store(true, std::memory_order_release);
        emit speaking_started();
    });
    connect(provider_.get(), &TtsProvider::speaking_finished, this, [this]() {
        speaking_.store(false, std::memory_order_release);
        emit speaking_finished();
    });
    connect(provider_.get(), &TtsProvider::error_occurred, this, &TtsService::error_occurred);
}

void TtsService::teardown_provider() {
    if (!provider_)
        return;
    provider_->stop();
    disconnect(provider_.get(), nullptr, this, nullptr);
    provider_.reset();
}

} // namespace fincept::services
