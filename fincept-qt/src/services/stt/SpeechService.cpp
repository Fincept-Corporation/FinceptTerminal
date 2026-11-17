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
#include "core/process/ProcessTree.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace fincept::services {

static constexpr auto TAG = "SpeechService";

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
        LOG_DEBUG(TAG, QString("PythonSttProvider[%1]::start() — entry").arg(name()));
        if (process_) {
            LOG_WARN(
                TAG,
                QString("PythonSttProvider[%1]: start() called but process_ already exists — ignoring").arg(name()));
            return;
        }

        QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
        LOG_DEBUG(TAG, QString("Resolving python_exe — venv-numpy2 path: '%1' (exists=%2)")
                           .arg(log_path(python_exe))
                           .arg(QFileInfo::exists(python_exe)));
        if (python_exe.isEmpty() || !QFileInfo::exists(python_exe)) {
            const QString fallback = python::PythonRunner::instance().python_path();
            LOG_WARN(TAG,
                     QString("venv-numpy2 python missing — falling back to PythonRunner default: '%1'").arg(fallback));
            python_exe = fallback;
        }

        if (python_exe.isEmpty()) {
            LOG_ERROR(TAG, "No Python interpreter available — aborting STT start");
            emit fatal_error(QStringLiteral("Python not available — cannot start voice input"));
            return;
        }

        const QString script = script_path();
        LOG_DEBUG(TAG,
                  QString("Resolving script — path: '%1' (exists=%2)").arg(log_path(script)).arg(QFileInfo::exists(script)));
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
        const QString new_pypath = existing_pypath.isEmpty() ? scripts_dir : (scripts_dir + kPathSep + existing_pypath);
        env.insert("PYTHONPATH", new_pypath);

        // Give subclass a chance to inject API keys / model / language
        const QString env_problem = extend_environment(env);
        if (!env_problem.isEmpty()) {
            delete process_; // never started, nothing connected yet
            process_ = nullptr;
            emit fatal_error(env_problem);
            return;
        }

        process_->setProcessEnvironment(env);
        process_->setWorkingDirectory(scripts_dir);
        // Capture stderr so Python tracebacks / diagnostic prints surface in
        // the C++ log instead of being silently dropped.
        process_->setProcessChannelMode(QProcess::SeparateChannels);

        // The recogniser and anything it starts are one unit: stop() kills the whole
        // tree, leftovers of a recogniser that ended are reaped when its QProcess is
        // deleted, and on Windows the OS kills the tree if the terminal itself dies
        // (kill-on-close job). Never released: nothing it starts should outlive it.
        // Also hides the console window (was the CREATE_NO_WINDOW modifier here).
        // Before start() and before the started handler below (adoption runs first).
        process::own_tree(process_);

        connect(process_, &QProcess::readyReadStandardOutput, this, &PythonSttProvider::on_stdout_ready);
        connect(process_, &QProcess::readyReadStandardError, this, &PythonSttProvider::on_stderr_ready);
        connect(process_, &QProcess::started, this, [this]() {
            LOG_DEBUG(TAG, QString("Provider[%1]: QProcess::started — pid=%2")
                               .arg(name())
                               .arg(process_ ? process_->processId() : 0));
        });
        connect(process_, &QProcess::finished, this, &PythonSttProvider::on_process_finished);
        connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            // Only a failed start is its own event; a crash is reported by on_process_finished.
            if (err == QProcess::FailedToStart)
                LOG_ERROR(TAG, logf("Voice process could not start",
                                    {{"provider", name()}, {"error", process_ ? process_->errorString() : QString()}}));
            else
                LOG_DEBUG(TAG,
                          QString("Provider[%1]: QProcess::errorOccurred err=%2").arg(name()).arg(static_cast<int>(err)));
            if (err == QProcess::FailedToStart) {
                emit fatal_error(QStringLiteral("Failed to start voice recognition process"));
                stop();
            }
        });

        stdout_buffer_.clear();
        last_stderr_.clear();
        LOG_DEBUG(TAG, QString("Provider[%1]: launching '%2' '%3' (cwd='%4')")
                           .arg(name(), log_path(python_exe), log_path(script), log_path(scripts_dir)));
        process_->start(python_exe, {script});

        active_.store(true, std::memory_order_release);
        LOG_INFO(TAG, logf("Voice input started", {{"provider", name()}}));
        emit active_changed(true);
    }

    void stop() override {
        if (!process_)
            return;

        // Fire-and-forget teardown (§P1) — stop() is UI-reachable (mic toggle,
        // fatal-event handler, reload_config), so no waitForFinished here.
        // Detaching FIRST means the eventual exit cannot re-enter
        // on_process_finished and emit a second active_changed(false); letting
        // the child reap itself on `finished` avoids ~QProcess's own blocking
        // waitForFinished() on a still-Running handle.
        QProcess* dying = process_;
        process_ = nullptr;
        stdout_buffer_.clear();
        disconnect(dying, nullptr, this, nullptr);
        if (dying->state() == QProcess::NotRunning) {
            dying->deleteLater();
        } else {
            connect(dying, &QProcess::finished, dying, &QObject::deleteLater);
            connect(dying, &QProcess::errorOccurred, dying, &QObject::deleteLater);
            process::kill_tree(dying);
        }

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            LOG_INFO(TAG, QString("Voice provider '%1' stopped").arg(name()));
            emit active_changed(false);
        }
    }

    [[nodiscard]] bool is_active() const noexcept override { return active_.load(std::memory_order_relaxed); }

  protected:
    /// Absolute path to the Python script that implements the JSON-lines protocol.
    virtual QString script_path() const = 0;

    /// Hook for subclasses to add API keys / model / language etc. to the
    /// child process environment. Default: no extra vars. A non-empty return
    /// is a plain sentence: the process is not started (fatal_error instead).
    virtual QString extend_environment(QProcessEnvironment& /*env*/) const { return {}; }

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
                LOG_DEBUG(TAG, QString("Provider[%1] stdout: %2").arg(name(), QString::fromUtf8(line.left(400))));
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
        // Policy B.9: stderr at DEBUG only; its last line goes into the exit line if the process fails.
        const QString text = QString::fromUtf8(chunk).trimmed();
        if (!text.isEmpty()) {
            last_stderr_ = text.section(QLatin1Char('\n'), -1).trimmed();
            LOG_DEBUG(TAG, QString("Provider[%1] stderr: %2").arg(name(), text));
        }
    }

    void parse_line(const QByteArray& line) {
        QJsonParseError parse_err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_err);
        if (parse_err.error != QJsonParseError::NoError) {
            LOG_DEBUG(TAG,
                      QString("Provider[%1]: non-JSON stdout line ignored: %2").arg(name(), QString::fromUtf8(line)));
            return;
        }

        const QJsonObject obj = doc.object();

        if (obj.contains("text")) {
            const QString text = obj["text"].toString().trimmed();
            if (!text.isEmpty()) {
                LOG_DEBUG(TAG, logf("Transcript received", {{"provider", name()}, {"chars", text.length()}}));
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
            LOG_ERROR(TAG, QString("Provider[%1] STT FATAL event: %2 — tearing down provider").arg(name(), msg));
            emit fatal_error(msg);
            stop();
        } else if (obj.contains("status")) {
            const QString status = obj["status"].toString();
            LOG_DEBUG(TAG, QString("Provider[%1] STT status: %2").arg(name(), status));
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
                const QString tail = QString::fromUtf8(remaining_err).trimmed();
                if (!tail.isEmpty())
                    last_stderr_ = tail.section(QLatin1Char('\n'), -1).trimmed();
                LOG_DEBUG(TAG, QString("Provider[%1] final stderr: %2").arg(name(), tail));
            }
            const QByteArray remaining_out = process_->readAllStandardOutput();
            if (!remaining_out.isEmpty()) {
                // Never the text itself: it can be a transcript.
                LOG_DEBUG(TAG, logf("Provider left unread stdout", {{"provider", name()}, {"bytes", remaining_out.size()}}));
            }
        }

        // One line per exit: ERROR when a running session died (the user sees the error),
        // WARN for an abnormal exit outside a session, INFO otherwise.
        {
            const bool abnormal = status == QProcess::CrashExit || exit_code != 0;
            const bool in_session = active_.load(std::memory_order_relaxed);
            if (abnormal && in_session)
                LOG_ERROR(TAG, logf("Voice process exited abnormally",
                                    {{"provider", name()},
                                     {"exit", exit_code},
                                     {"status", status == QProcess::CrashExit ? "crash" : "normal"},
                                     {"error", last_stderr_}}));
            else if (abnormal)
                LOG_WARN(TAG, logf("Voice process exited abnormally",
                                   {{"provider", name()},
                                    {"exit", exit_code},
                                    {"status", status == QProcess::CrashExit ? "crash" : "normal"},
                                    {"error", last_stderr_}}));
            else
                LOG_INFO(TAG, logf("Voice process exited", {{"provider", name()}, {"exit", exit_code}}));
        }

        if (process_) {
            process_->deleteLater();
            process_ = nullptr;
        }
        stdout_buffer_.clear();

        const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
        if (was_active) {
            emit active_changed(false);
            if (status == QProcess::CrashExit || exit_code != 0) {
                // Logged once above ("Voice process exited abnormally").
                emit error(QStringLiteral("Voice recognition stopped unexpectedly"));
            }
        }
    }

    QProcess* process_ = nullptr;
    QByteArray stdout_buffer_;
    QString last_stderr_; // last stderr line, for the exit line (policy B.9)
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
        return python::PythonRunner::instance().scripts_dir() + QStringLiteral("/voice/speech_to_text.py");
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
        return python::PythonRunner::instance().scripts_dir() + QStringLiteral("/voice/deepgram_stt.py");
    }

    QString extend_environment(QProcessEnvironment& env) const override {
        const SecretRead key_res = SecureStorage::instance().read(QStringLiteral("voice.deepgram.api_key"));
        const bool unreadable = key_res.failed();
        LOG_TRANSITION(TAG, QStringLiteral("deepgram_key_read"), unreadable,
                       unreadable ? LogLevel::Warn : LogLevel::Info,
                       unreadable ? logf("Deepgram API key could not be read; voice input not started",
                                         {{"key", "voice.deepgram.api_key"}, {"kind", key_res.error_name()}})
                                  : QStringLiteral("Deepgram API key readable again"));
        if (unreadable) {
            // Stored but unreadable — not "no key": starting the script with an empty
            // key would report a missing key the user already entered.
            return tr("Could not read the stored Deepgram API key: %1. Voice input was not started — try again "
                      "in a moment.")
                .arg(key_res.reason());
        }
        const QString api_key = key_res.found() ? key_res.value : QString();

        auto& cfg = AppConfig::instance();
        const QString model = cfg.get("voice/deepgram/model", "nova-3").toString();
        const QString lang = cfg.get("voice/deepgram/language", "en").toString();
        const QString keyterms = cfg.get("voice/deepgram/keyterms", "").toString();
        // Audio capture knobs — laptop mics often need 3-5x gain; far-field
        // setups may need a specific device by name. Defaults match script.
        const QString gain = cfg.get("voice/deepgram/gain", "3.0").toString();
        const QString device = cfg.get("voice/deepgram/device", "").toString();

        // Never the key, not even its length (policy B.6): only whether one is set.
        LOG_DEBUG(TAG, QString("Deepgram env: api_key_set=%1 model='%2' language='%3' "
                               "keyterms_len=%4 gain=%5 device='%6'")
                           .arg(api_key.isEmpty() ? QStringLiteral("false") : QStringLiteral("true"))
                           .arg(model, lang)
                           .arg(keyterms.length())
                           .arg(gain, device));

        env.insert("DEEPGRAM_API_KEY", api_key);
        env.insert("FINCEPT_STT_MODEL", model);
        env.insert("FINCEPT_STT_LANGUAGE", lang);
        env.insert("FINCEPT_STT_KEYTERMS", keyterms);
        env.insert("FINCEPT_STT_GAIN", gain);
        env.insert("FINCEPT_STT_DEVICE", device);
        return {};
    }
};

} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

SpeechService& SpeechService::instance() {
    static SpeechService s_instance;
    return s_instance;
}

SpeechService::SpeechService(QObject* parent) : QObject(parent) {
    LOG_DEBUG(TAG, "SpeechService initialised");
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
    LOG_DEBUG(TAG, QString("SpeechService::start_listening() — listening_=%1 provider=%2")
                       .arg(listening_.load(std::memory_order_relaxed))
                       .arg(provider_ ? provider_->name() : QStringLiteral("<none>")));
    if (listening_.load(std::memory_order_relaxed)) {
        // A second click while listening: expected, nothing degraded.
        LOG_DEBUG(TAG, "Voice input already listening, start ignored");
        return;
    }

    install_provider();
    if (!provider_) {
        LOG_ERROR(TAG, logf("Voice input could not start, no provider installed", {{"provider", configured_provider()}}));
        emit error_occurred(QStringLiteral("Voice provider could not be installed"));
        return;
    }

    LOG_DEBUG(TAG, QString("start_listening: delegating to provider '%1'").arg(provider_->name()));
    provider_->start();
}

void SpeechService::stop_listening() {
    LOG_DEBUG(TAG, QString("SpeechService::stop_listening() — listening_=%1 provider=%2")
                       .arg(listening_.load(std::memory_order_relaxed))
                       .arg(provider_ ? provider_->name() : QStringLiteral("<none>")));
    if (!listening_.load(std::memory_order_relaxed)) {
        LOG_DEBUG(TAG, "stop_listening: not active — no-op");
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
        LOG_DEBUG(TAG, "Voice config reloaded — restarting active session");
        provider_->start();
    } else {
        LOG_DEBUG(TAG, "Voice config reloaded (idle)");
    }
}

bool SpeechService::is_listening() const noexcept {
    return listening_.load(std::memory_order_relaxed);
}

// ── Provider lifecycle ───────────────────────────────────────────────────────

void SpeechService::install_provider() {
    const QString id = configured_provider();
    LOG_DEBUG(TAG, QString("install_provider() — configured='%1' current='%2'")
                       .arg(id, provider_ ? provider_->name() : QStringLiteral("<none>")));

    // If an existing provider matches the selection, reuse it.
    if (provider_ && provider_->name() == id) {
        LOG_DEBUG(TAG, "install_provider: reusing existing provider instance");
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
        LOG_DEBUG(TAG, QString("Provider active_changed — active=%1 (forwarding listening_changed)").arg(active));
        listening_.store(active, std::memory_order_release);
        emit listening_changed(active);
    });

    LOG_DEBUG(TAG, QString("Voice provider installed: %1").arg(id));
}

void SpeechService::teardown_provider() {
    if (!provider_)
        return;
    provider_->stop();
    disconnect(provider_.get(), nullptr, this, nullptr);
    provider_.reset();
}

} // namespace fincept::services
