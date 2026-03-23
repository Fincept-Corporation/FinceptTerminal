#include "python/PythonRunner.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace fincept::python {

// Scripts that require NumPy 1.x environment (same list as Tauri's python.rs)
static const QStringList kNumpy1Scripts = {
    "vectorbt", "backtesting", "gluonts", "functime",
    "pyportfolioopt", "financepy", "ffn", "ffn_wrapper",
};

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

QString PythonRunner::python_path() const {
    return python_path_;
}
QString PythonRunner::scripts_dir() const {
    return scripts_dir_;
}

// ── Venv routing ─────────────────────────────────────────────────────────────
// Routes scripts to the correct venv based on their name.
// Scripts matching kNumpy1Scripts go to venv-numpy1, all others to venv-numpy2.

static QString select_venv_for_script(const QString& script) {
    QString lower = script.toLower();
    for (const auto& keyword : kNumpy1Scripts) {
        if (lower.contains(keyword)) {
            return "venv-numpy1";
        }
    }
    return "venv-numpy2";
}

// ── Find Python ──────────────────────────────────────────────────────────────

QString PythonRunner::find_python() const {
    // 1. Use UV-managed venvs from PythonSetupManager (preferred)
    auto& setup = PythonSetupManager::instance();
    QString venv2_py = setup.python_path("venv-numpy2");
    if (QFileInfo::exists(venv2_py)) {
        LOG_INFO("Python", "Using UV-managed venv-numpy2: " + venv2_py);
        return venv2_py;
    }

    QString venv1_py = setup.python_path("venv-numpy1");
    if (QFileInfo::exists(venv1_py)) {
        LOG_INFO("Python", "Using UV-managed venv-numpy1: " + venv1_py);
        return venv1_py;
    }

    // 2. Check venv next to executable (dev / portable)
    QString exe_dir = QCoreApplication::applicationDirPath();
    for (const auto& venv : {"venv-numpy2", "venv-numpy1", "venv", ".venv"}) {
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
        exe_dir + "/../../../../scripts", // deep dev: build/Release/../../scripts
        exe_dir + "/resources/scripts",
        "C:/windowsdisk/finceptTerminal/fincept-qt/scripts", // absolute fallback for dev
    };

    // Also check project root (for dev: fincept-qt/scripts)
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        if (QFileInfo::exists(dir.filePath("scripts/yfinance_data.py"))) {
            return dir.filePath("scripts");
        }
        if (dir.isRoot())
            break;
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
        cb({false, {}, "Python not available — run first-time setup from the app", -1});
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

/// Run arbitrary Python code (for notebook/colab cells).
/// Creates a temp file, executes it, returns output.
void PythonRunner::run_code(const QString& code, Callback cb) {
    if (python_path_.isEmpty()) {
        cb({false, {}, "Python not available — run first-time setup from the app", -1});
        return;
    }

    // Write code to a temp file
    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString temp_path = temp_dir + "/fincept_cell_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".py";

    QFile file(temp_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cb({false, {}, "Failed to create temp script file", -1});
        return;
    }
    file.write(code.toUtf8());
    file.close();

    // Queue as a special request — use the temp file path directly
    queue_.enqueue({"__code__:" + temp_path, {}, std::move(cb)});
    start_next();
}

void PythonRunner::start_next() {
    while (active_count_ < max_concurrent_ && !queue_.isEmpty()) {
        auto req = queue_.dequeue();
        ++active_count_;

        // Determine if this is inline code or a script file
        bool is_code = req.script.startsWith("__code__:");
        QString script_path;
        QString python_exe = python_path_;
        QString temp_file;

        if (is_code) {
            // Inline code execution (notebook cells)
            temp_file = req.script.mid(9); // strip "__code__:" prefix
            script_path = temp_file;
            // Always use venv-numpy2 for notebook code
            QString venv2 = PythonSetupManager::instance().python_path("venv-numpy2");
            if (QFileInfo::exists(venv2)) python_exe = venv2;
        } else {
            script_path = scripts_dir_ + "/" + req.script;
            // Route to correct venv based on script name
            QString venv_name = select_venv_for_script(req.script);
            QString venv_py = PythonSetupManager::instance().python_path(venv_name);
            if (QFileInfo::exists(venv_py)) python_exe = venv_py;
        }

        auto* proc = new QProcess(this);
        QStringList full_args;
        full_args << script_path;
        if (!is_code) full_args.append(req.args);

        // Set environment for clean Python execution
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");
        env.insert("PYTHONDONTWRITEBYTECODE", "1");
        env.insert("PYTHONUNBUFFERED", "1");
        env.insert("FINCEPT_DATA_DIR", PythonSetupManager::instance().install_dir());
        proc->setProcessEnvironment(env);

#ifdef _WIN32
        proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        auto cb = std::move(req.cb);
        auto script = req.script;

        connect(
            proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, cb, script, is_code, temp_file](int exit_code, QProcess::ExitStatus status) {
                Q_UNUSED(status);
                QString stdout_str = QString::fromUtf8(proc->readAllStandardOutput());
                QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();

                // Clean up temp file for inline code
                if (is_code && !temp_file.isEmpty()) {
                    QFile::remove(temp_file);
                }

                PythonResult result;
                result.exit_code = exit_code;

                if (is_code) {
                    // For notebook cells: return raw stdout, not JSON-extracted
                    result.success = (exit_code == 0);
                    result.output = stdout_str;
                    result.error = stderr_str;
                } else {
                    // For scripts: extract JSON from output
                    QString json_out = extract_json(stdout_str);
                    result.success = (exit_code == 0 && !json_out.isEmpty());
                    result.output = json_out.isEmpty() ? stdout_str : json_out;
                    result.error = stderr_str;
                }

                if (!result.success && !is_code) {
                    LOG_ERROR(
                        "Python",
                        QString("Script %1 failed (exit=%2): %3").arg(script).arg(exit_code).arg(stderr_str.left(200)));
                }

                cb(result);

                --active_count_;
                start_next(); // drain queue
            });

        connect(proc, &QProcess::errorOccurred, this, [this, proc, cb, is_code, temp_file](QProcess::ProcessError err) {
            Q_UNUSED(err);
            QString error_msg = proc->errorString();
            proc->deleteLater();
            if (is_code && !temp_file.isEmpty()) QFile::remove(temp_file);
            cb({false, {}, "Process error: " + error_msg, -1});

            --active_count_;
            start_next(); // drain queue
        });

        LOG_INFO("Python", QString("Running (%1/%2 active, %3 queued): %4 %5")
                               .arg(active_count_)
                               .arg(max_concurrent_)
                               .arg(queue_.size())
                               .arg(python_exe)
                               .arg(script_path));
        proc->start(python_exe, full_args);
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
