#include "python/PythonRunner.h"
#include "core/logging/Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace fincept::python {

PythonRunner& PythonRunner::instance() {
    static PythonRunner s;
    return s;
}

PythonRunner::PythonRunner() {
    python_path_ = find_python();
    scripts_dir_ = find_scripts_dir();

    if (python_path_.isEmpty()) {
        LOG_WARN("Python", "No Python interpreter found");
    } else {
        LOG_INFO("Python", "Python: " + python_path_);
    }
    LOG_INFO("Python", "Scripts: " + scripts_dir_);
}

bool PythonRunner::is_available() const {
    return !python_path_.isEmpty();
}

QString PythonRunner::python_path() const { return python_path_; }
QString PythonRunner::scripts_dir() const { return scripts_dir_; }

// ── Find Python ──────────────────────────────────────────────────────────────

QString PythonRunner::find_python() const {
    // 1. Check venv in APPDATA (Windows install dir)
#ifdef _WIN32
    QString appdata = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // Try com.fincept.terminal venvs
    QDir appdata_dir(appdata);
    appdata_dir.cdUp(); // Go from "Fincept/FinceptTerminal" to "Fincept"
    appdata_dir.cdUp(); // Go to AppData/Roaming or AppData/Local

    for (const auto& venv : {"venv-numpy2", "venv-numpy1", "venv"}) {
        QString candidate = appdata_dir.filePath(
            QString("com.fincept.terminal/%1/Scripts/python.exe").arg(venv));
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
#else
    QString home = QDir::homePath();
    for (const auto& venv : {"venv-numpy2", "venv-numpy1", "venv"}) {
        QString candidate = home + "/.local/share/com.fincept.terminal/" +
                            QString(venv) + "/bin/python3";
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
#endif

    // 2. Check venv next to executable
    QString exe_dir = QCoreApplication::applicationDirPath();
    for (const auto& venv : {"venv", "venv-numpy2", ".venv"}) {
#ifdef _WIN32
        QString candidate = exe_dir + "/" + venv + "/Scripts/python.exe";
#else
        QString candidate = exe_dir + "/" + venv + "/bin/python3";
#endif
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    // 3. Fallback to system python
#ifdef _WIN32
    QString sys_python = "python";
#else
    QString sys_python = "python3";
#endif

    QProcess test;
    test.start(sys_python, {"--version"});
    if (test.waitForFinished(5000) && test.exitCode() == 0) {
        return sys_python;
    }

    return {};
}

// ── Find Scripts Directory ───────────────────────────────────────────────────

QString PythonRunner::find_scripts_dir() const {
    QString exe_dir = QCoreApplication::applicationDirPath();

    // Check relative to exe: scripts/, ../scripts/, etc.
    QStringList candidates = {
        exe_dir + "/scripts",
        exe_dir + "/../scripts",
        exe_dir + "/../../scripts",
        exe_dir + "/../../../scripts",
        exe_dir + "/../../../../scripts",    // deep dev: build/Release/../../scripts
        exe_dir + "/resources/scripts",
        "C:/windowsdisk/finceptTerminal/fincept-qt/scripts",  // absolute fallback for dev
    };

    // Also check project root (for dev: fincept-qt/scripts)
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        if (QFileInfo::exists(dir.filePath("scripts/yfinance_data.py"))) {
            return dir.filePath("scripts");
        }
        if (dir.isRoot()) break;
    }

    for (const auto& c : candidates) {
        if (QFileInfo::exists(c + "/yfinance_data.py")) {
            return QDir::cleanPath(c);
        }
    }

    // Last resort: current directory
    if (QFileInfo::exists("scripts/yfinance_data.py")) {
        return QDir::cleanPath("scripts");
    }

    LOG_WARN("Python", "Scripts directory not found");
    return exe_dir + "/scripts";
}

// ── Run Script ───────────────────────────────────────────────────────────────

void PythonRunner::run(const QString& script, const QStringList& args, Callback cb) {
    if (python_path_.isEmpty()) {
        cb({false, {}, "Python not available", -1});
        return;
    }

    QString script_path = scripts_dir_ + "/" + script;
    if (!QFileInfo::exists(script_path)) {
        cb({false, {}, "Script not found: " + script_path, -1});
        return;
    }

    // Queue the request and start if under concurrency limit
    queue_.enqueue({script, args, std::move(cb)});
    start_next();
}

void PythonRunner::start_next() {
    while (active_count_ < max_concurrent_ && !queue_.isEmpty()) {
        auto req = queue_.dequeue();
        ++active_count_;

        QString script_path = scripts_dir_ + "/" + req.script;
        auto* proc = new QProcess(this);
        QStringList full_args;
        full_args << script_path;
        full_args.append(req.args);

        auto cb = std::move(req.cb);
        auto script = req.script;

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, cb, script](int exit_code, QProcess::ExitStatus status) {
            Q_UNUSED(status);
            QString stdout_str = QString::fromUtf8(proc->readAllStandardOutput());
            QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
            proc->deleteLater();

            QString json_out = extract_json(stdout_str);

            PythonResult result;
            result.exit_code = exit_code;
            result.success = (exit_code == 0 && !json_out.isEmpty());
            result.output = json_out.isEmpty() ? stdout_str : json_out;
            result.error = stderr_str;

            if (!result.success) {
                LOG_ERROR("Python", QString("Script %1 failed (exit=%2): %3")
                    .arg(script).arg(exit_code).arg(stderr_str.left(200)));
            }

            cb(result);

            --active_count_;
            start_next();  // drain queue
        });

        connect(proc, &QProcess::errorOccurred, this, [this, proc, cb](QProcess::ProcessError err) {
            Q_UNUSED(err);
            QString error_msg = proc->errorString();
            proc->deleteLater();
            cb({false, {}, "Process error: " + error_msg, -1});

            --active_count_;
            start_next();  // drain queue
        });

        LOG_INFO("Python", QString("Running (%1/%2 active, %3 queued): %4 %5")
            .arg(active_count_).arg(max_concurrent_).arg(queue_.size())
            .arg(python_path_).arg(script_path));
        proc->start(python_path_, full_args);
    }
}

// ── Extract JSON ─────────────────────────────────────────────────────────────

QString extract_json(const QString& output) {
    // Find the last line that starts with '{' or '['
    // This matches the Rust python_runner's extract_json logic
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (int i = lines.size() - 1; i >= 0; --i) {
        QString trimmed = lines[i].trimmed();
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            return trimmed;
        }
    }
    return {};
}

} // namespace fincept::python
