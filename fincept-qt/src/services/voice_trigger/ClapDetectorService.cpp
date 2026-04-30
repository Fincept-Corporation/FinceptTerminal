// ClapDetectorService.cpp — Drives scripts/voice/clap_detector.py and emits
// `clap_detected()` when the Python side reports an event.
//
// Logging mirrors SpeechService / TtsService so failures (mic missing, script
// crash, etc.) are diagnosable from the central log file without re-running.

#include "services/voice_trigger/ClapDetectorService.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace fincept::services {

namespace {
// Anon-namespaced + uniquely-prefixed to avoid the unity-build symbol
// collision trap that bit SpeechService / TtsService earlier.
constexpr auto CLAP_TAG = "ClapDetector";
constexpr int  kClapShutdownTimeoutMs = 1500;
} // namespace

ClapDetectorService& ClapDetectorService::instance() {
    static ClapDetectorService s_instance;
    return s_instance;
}

ClapDetectorService::ClapDetectorService(QObject* parent) : QObject(parent) {
    LOG_INFO(CLAP_TAG, "ClapDetectorService initialised");
}

ClapDetectorService::~ClapDetectorService() {
    stop();
}

bool ClapDetectorService::is_active() const noexcept {
    return active_.load(std::memory_order_relaxed);
}

bool ClapDetectorService::is_enabled_in_config() {
    return AppConfig::instance().get("voice/clap_to_start/enabled", false).toBool();
}

void ClapDetectorService::start() {
    if (process_) {
        LOG_INFO(CLAP_TAG, "start: already running — no-op");
        return;
    }

    QString python_exe = python::PythonSetupManager::instance().python_path("venv-numpy2");
    if (python_exe.isEmpty() || !QFileInfo::exists(python_exe))
        python_exe = python::PythonRunner::instance().python_path();
    if (python_exe.isEmpty()) {
        LOG_ERROR(CLAP_TAG, "No Python interpreter — cannot start clap detector");
        emit error_occurred(QStringLiteral("Python not available — clap-to-start unavailable"));
        return;
    }

    const QString scripts_dir = python::PythonRunner::instance().scripts_dir();
    const QString script = scripts_dir + QStringLiteral("/voice/clap_detector.py");
    if (!QFileInfo::exists(script)) {
        LOG_ERROR(CLAP_TAG, QString("Clap detector script missing at '%1'").arg(script));
        emit error_occurred(QStringLiteral("clap_detector.py not found: ") + script);
        return;
    }

    process_ = new QProcess(this);

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
    env.insert("PYTHONPATH", existing_pypath.isEmpty()
                                 ? scripts_dir
                                 : (scripts_dir + kPathSep + existing_pypath));

    auto& cfg = AppConfig::instance();
    const QString mode    = cfg.get("voice/clap_to_start/mode", "double").toString();
    const QString peak    = cfg.get("voice/clap_to_start/peak_min", "12000").toString();
    const QString ratio   = cfg.get("voice/clap_to_start/pr_ratio", "4.0").toString();
    const QString gap     = cfg.get("voice/clap_to_start/max_gap_ms", "1500").toString();
    const QString debounce = cfg.get("voice/clap_to_start/debounce_ms", "1500").toString();
    const QString device  = cfg.get("voice/deepgram/device", "").toString();

    LOG_INFO(CLAP_TAG, QString("env: mode=%1 peak=%2 ratio=%3 gap=%4 debounce=%5 device='%6'")
                           .arg(mode, peak, ratio, gap, debounce, device));

    env.insert("FINCEPT_CLAP_MODE", mode);
    env.insert("FINCEPT_CLAP_PEAK_MIN", peak);
    env.insert("FINCEPT_CLAP_PR_RATIO", ratio);
    env.insert("FINCEPT_CLAP_MAX_GAP_MS", gap);
    env.insert("FINCEPT_CLAP_DEBOUNCE_MS", debounce);
    env.insert("FINCEPT_STT_DEVICE", device);

    process_->setProcessEnvironment(env);
    process_->setWorkingDirectory(scripts_dir);
    process_->setProcessChannelMode(QProcess::SeparateChannels);

#ifdef _WIN32
    process_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(process_, &QProcess::readyReadStandardOutput, this, &ClapDetectorService::on_stdout_ready);
    connect(process_, &QProcess::readyReadStandardError,  this, &ClapDetectorService::on_stderr_ready);
    connect(process_, &QProcess::started, this, [this]() {
        LOG_INFO(CLAP_TAG, QString("QProcess::started — pid=%1")
                               .arg(process_ ? process_->processId() : 0));
    });
    connect(process_, &QProcess::finished, this, &ClapDetectorService::on_process_finished);
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        LOG_ERROR(CLAP_TAG, QString("QProcess::errorOccurred err=%1").arg(static_cast<int>(err)));
        if (err == QProcess::FailedToStart) {
            emit error_occurred(QStringLiteral("Failed to start clap detector"));
            stop();
        }
    });

    stdout_buffer_.clear();
    LOG_INFO(CLAP_TAG, QString("Launching '%1' '%2'").arg(python_exe, script));
    process_->start(python_exe, {script});

    active_.store(true, std::memory_order_release);
    emit listening_changed(true);
}

void ClapDetectorService::stop() {
    if (!process_)
        return;

    LOG_INFO(CLAP_TAG, "stop — terminating clap detector");
    disconnect(process_, nullptr, this, nullptr);
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(kClapShutdownTimeoutMs);
    }
    process_->deleteLater();
    process_ = nullptr;
    stdout_buffer_.clear();

    const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
    if (was_active)
        emit listening_changed(false);
}

void ClapDetectorService::on_stdout_ready() {
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
            LOG_DEBUG(CLAP_TAG, QString("stdout: %1").arg(QString::fromUtf8(line.left(200))));
            parse_line(line);
        }
    }
}

void ClapDetectorService::on_stderr_ready() {
    if (!process_)
        return;
    const QByteArray chunk = process_->readAllStandardError();
    if (chunk.isEmpty())
        return;
    const QString text = QString::fromUtf8(chunk).trimmed();
    if (!text.isEmpty()) {
        // Stderr is verbose by design (per-candidate logs) — keep at WARN only
        // for human-friendly visibility. Drop to DEBUG if it gets noisy.
        LOG_WARN(CLAP_TAG, QString("stderr: %1").arg(text));
    }
}

void ClapDetectorService::parse_line(const QByteArray& line) {
    QJsonParseError parse_err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_err);
    if (parse_err.error != QJsonParseError::NoError) {
        LOG_WARN(CLAP_TAG, QString("non-JSON stdout: %1").arg(QString::fromUtf8(line)));
        return;
    }
    const QJsonObject obj = doc.object();

    if (obj.contains("event") && obj["event"].toString() == "clap") {
        LOG_INFO(CLAP_TAG, "Clap event detected — emitting clap_detected()");
        emit clap_detected();
    } else if (obj.contains("status")) {
        LOG_INFO(CLAP_TAG, QString("status: %1").arg(obj["status"].toString()));
    } else if (obj.contains("error")) {
        const QString msg = obj["error"].toString();
        LOG_WARN(CLAP_TAG, QString("script error: %1").arg(msg));
        emit error_occurred(msg);
    } else if (obj.contains("fatal")) {
        const QString msg = obj["fatal"].toString();
        LOG_ERROR(CLAP_TAG, QString("script fatal: %1").arg(msg));
        emit error_occurred(msg);
    }
}

void ClapDetectorService::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    if (process_) {
        const QByteArray remaining_err = process_->readAllStandardError();
        if (!remaining_err.isEmpty())
            LOG_WARN(CLAP_TAG, QString("final stderr: %1").arg(QString::fromUtf8(remaining_err).trimmed()));
    }

    LOG_INFO(CLAP_TAG, QString("process exited (code=%1, status=%2)")
                           .arg(exit_code)
                           .arg(status == QProcess::CrashExit ? "crash" : "normal"));

    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }
    stdout_buffer_.clear();

    const bool was_active = active_.exchange(false, std::memory_order_acq_rel);
    if (was_active)
        emit listening_changed(false);
}

} // namespace fincept::services
