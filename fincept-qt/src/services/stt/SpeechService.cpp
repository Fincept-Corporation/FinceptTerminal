// SpeechService.cpp — Python-based speech-to-text via Google Speech Recognition.
//
// Process lifecycle:
//   start_listening() → spawn QProcess running speech_to_text.py
//   stdout JSON lines → parse_line() → emit transcription_ready / error_occurred
//   stop_listening()  → kill QProcess
//
// The Python script handles mic capture, ambient noise calibration, and
// Google API calls. This service just manages the process and parses output.

#include "services/stt/SpeechService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace fincept::services {

static constexpr auto TAG = "SpeechService";
static constexpr int kShutdownTimeoutMs = 2000;

// ── Singleton ────────────────────────────────────────────────────────────────

SpeechService& SpeechService::instance() {
    static SpeechService s_instance;
    return s_instance;
}

SpeechService::SpeechService(QObject* parent) : QObject(parent) {
    LOG_INFO(TAG, "SpeechService initialised");
}

SpeechService::~SpeechService() {
    kill_process();
}

// ── Public API ───────────────────────────────────────────────────────────────

void SpeechService::start_listening() {
    if (listening_.load(std::memory_order_relaxed))
        return;
    spawn_process();
}

void SpeechService::stop_listening() {
    if (!listening_.load(std::memory_order_relaxed))
        return;
    kill_process();
}

bool SpeechService::is_listening() const noexcept {
    return listening_.load(std::memory_order_relaxed);
}

// ── Process management ───────────────────────────────────────────────────────

void SpeechService::spawn_process() {
    if (process_) {
        LOG_WARN(TAG, "Process already running — ignoring spawn request");
        return;
    }

    // Resolve the Python executable from the numpy2 venv
    QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
    if (python_exe.isEmpty() || !QFileInfo::exists(python_exe)) {
        // Fallback to PythonRunner's resolved path
        python_exe = python::PythonRunner::instance().python_path();
    }
    if (python_exe.isEmpty()) {
        const QString msg = QStringLiteral("Python not available — cannot start voice input");
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return;
    }

    // Resolve script path
    const QString scripts_dir = python::PythonRunner::instance().scripts_dir();
    const QString script_path = scripts_dir + QStringLiteral("/voice/speech_to_text.py");
    if (!QFileInfo::exists(script_path)) {
        const QString msg = QStringLiteral("Voice script not found: ") + script_path;
        LOG_ERROR(TAG, msg);
        emit error_occurred(msg);
        return;
    }

    process_ = new QProcess(this);

    // Environment setup — match PythonRunner's env for consistency
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONDONTWRITEBYTECODE", "1");
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("FINCEPT_DATA_DIR", python::PythonSetupManager::instance().install_dir());

    const QString existing_pypath = env.value("PYTHONPATH");
#ifdef _WIN32
    const QChar kPathSep = ';';
#else
    const QChar kPathSep = ':';
#endif
    const QString new_pypath =
        existing_pypath.isEmpty() ? scripts_dir : (scripts_dir + kPathSep + existing_pypath);
    env.insert("PYTHONPATH", new_pypath);

    process_->setProcessEnvironment(env);
    process_->setWorkingDirectory(scripts_dir);

#ifdef _WIN32
    process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(process_, &QProcess::readyReadStandardOutput, this, &SpeechService::on_stdout_ready);
    connect(process_, &QProcess::finished, this, &SpeechService::on_process_finished);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            const QString msg = QStringLiteral("Failed to start voice recognition process");
            LOG_ERROR(TAG, msg);
            emit error_occurred(msg);
            kill_process();
        }
    });

    stdout_buffer_.clear();
    process_->start(python_exe, {script_path});

    listening_.store(true, std::memory_order_release);
    LOG_INFO(TAG, "Voice recognition process started");
    emit listening_changed(true);
}

void SpeechService::kill_process() {
    if (!process_)
        return;

    // Disconnect signals before killing to avoid spurious callbacks
    disconnect(process_, nullptr, this, nullptr);

    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(kShutdownTimeoutMs);
    }
    process_->deleteLater();
    process_ = nullptr;
    stdout_buffer_.clear();

    const bool was_listening = listening_.exchange(false, std::memory_order_acq_rel);
    if (was_listening) {
        LOG_INFO(TAG, "Voice recognition process stopped");
        emit listening_changed(false);
    }
}

// ── stdout parsing ───────────────────────────────────────────────────────────

void SpeechService::on_stdout_ready() {
    if (!process_)
        return;

    stdout_buffer_.append(process_->readAllStandardOutput());

    // Process complete lines (JSON-lines protocol)
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

void SpeechService::parse_line(const QByteArray& line) {
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
            emit transcription_ready(text);
        }
    } else if (obj.contains("error")) {
        const QString msg = obj["error"].toString();
        LOG_WARN(TAG, QString("STT error: %1").arg(msg));
        emit error_occurred(msg);
    } else if (obj.contains("fatal")) {
        const QString msg = obj["fatal"].toString();
        LOG_ERROR(TAG, QString("STT fatal: %1").arg(msg));
        emit error_occurred(msg);
        kill_process();
    } else if (obj.contains("status")) {
        const QString status = obj["status"].toString();
        LOG_DEBUG(TAG, QString("STT status: %1").arg(status));
    }
}

void SpeechService::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    LOG_INFO(TAG, QString("Voice process exited (code=%1, status=%2)")
                      .arg(exit_code)
                      .arg(status == QProcess::CrashExit ? "crash" : "normal"));

    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }
    stdout_buffer_.clear();

    const bool was_listening = listening_.exchange(false, std::memory_order_acq_rel);
    if (was_listening) {
        emit listening_changed(false);
        if (status == QProcess::CrashExit || exit_code != 0) {
            emit error_occurred(QStringLiteral("Voice recognition stopped unexpectedly"));
        }
    }
}

} // namespace fincept::services
