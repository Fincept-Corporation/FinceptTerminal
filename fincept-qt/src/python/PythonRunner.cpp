#include "python/PythonRunner.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUuid>

namespace fincept::python {

// Scripts that require NumPy 1.x environment
static const QStringList kNumpy1Scripts = {
    "vectorbt", "backtesting", "gluonts", "functime", "pyportfolioopt", "financepy", "ffn", "ffn_wrapper",
};

PythonRunner& PythonRunner::instance() {
    static PythonRunner s;
    return s;
}

PythonRunner::PythonRunner() {
    scripts_dir_ = find_scripts_dir();
    LOG_INFO("Python", "Scripts: " + scripts_dir_);

    // Try fast, non-blocking venv detection first
    python_path_ = find_python_sync();

    if (!python_path_.isEmpty()) {
        python_init_done_ = true;
        LOG_INFO("Python", "Python: " + python_path_);
    } else {
        // System python detection requires a process spawn — do it async
        find_python_async();
    }
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

// ── Standard Python environment ──────────────────────────────────────────────
// Shared by PythonRunner::run() and external direct-QProcess callers
// (e.g., AgentService's stdin/stream paths). Anything every Python spawn needs
// — encoding pins, FINCEPT_DATA_DIR, FINAGENT_DATA_DIR, base PYTHONPATH — lives
// here. Script-specific path additions (parent-of-pkg) stay in run().
QProcessEnvironment PythonRunner::build_python_env() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONDONTWRITEBYTECODE", "1");
    env.insert("PYTHONUNBUFFERED", "1");

    const QString install_dir = PythonSetupManager::instance().install_dir();
    env.insert("FINCEPT_DATA_DIR", install_dir);

    // FINAGENT_DATA_DIR: default to <FINCEPT_DATA_DIR>/finagent. Always overwrite
    // so we match what resources.py expects regardless of what the user shell
    // inherited — the app owns its own per-persona storage root.
    env.insert("FINAGENT_DATA_DIR", install_dir + "/finagent");

    // FINAGENT_RUNTIME_CACHE_SIZE: respect user override if already set; otherwise
    // leave unset so persona_registry.py uses its own default (8).
    // (No insert here by design.)

    // Base PYTHONPATH: scripts_dir on front, preserve whatever the user had.
#ifdef _WIN32
    const QChar kPathSep = ';';
#else
    const QChar kPathSep = ':';
#endif
    const QString existing_pypath = env.value("PYTHONPATH");
    QString new_pypath =
        existing_pypath.isEmpty() ? scripts_dir_ : (scripts_dir_ + kPathSep + existing_pypath);
    env.insert("PYTHONPATH", new_pypath);

    return env;
}

// ── Venv routing ─────────────────────────────────────────────────────────────
// Routes scripts to the correct venv based on their name.

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

// Fast, non-blocking detection: checks only file existence (no process spawns)
QString PythonRunner::find_python_sync() const {
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

    return {}; // No venv found — need async system python check
}

// Async detection: spawns `python --version` without blocking the UI thread
void PythonRunner::find_python_async() {
#ifdef _WIN32
    QString sys_python = "python";
#else
    QString sys_python = "python3";
#endif

    auto* proc = new QProcess(this);

#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, sys_python](int exit_code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (exit_code == 0) {
                    python_path_ = sys_python;
                    LOG_INFO("Python", "System Python found: " + sys_python);
                } else {
                    LOG_WARN("Python", "No Python interpreter found");
                }
                python_init_done_ = true;

                // Drain any requests that were queued while we were detecting
                start_next();
            });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        proc->deleteLater();
        LOG_WARN("Python", "No Python interpreter found (process error)");
        python_init_done_ = true;
    });

    proc->start(sys_python, {"--version"});
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

void PythonRunner::run(const QString& script, const QStringList& args, Callback cb, StreamCallback on_line) {
    if (python_init_done_ && python_path_.isEmpty()) {
        cb({false, {}, "Python not available — run first-time setup from the app", -1});
        return;
    }

    QString script_path = scripts_dir_ + "/" + script;
    if (!QFileInfo::exists(script_path)) {
        cb({false, {}, "Script not found: " + script_path, -1});
        return;
    }

    // Queue the request and start if under concurrency limit
    queue_.enqueue({script, args, std::move(cb), std::move(on_line)});
    start_next();
}

/// Run arbitrary Python code (for notebook/colab cells).
/// Creates a temp file, executes it, returns output.
void PythonRunner::run_code(const QString& code, Callback cb) {
    if (python_init_done_ && python_path_.isEmpty()) {
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
    // Don't start if Python detection is still in progress
    if (!python_init_done_)
        return;

    while (active_count_ < max_concurrent_ && !queue_.isEmpty()) {
        auto req = queue_.dequeue();

        if (python_path_.isEmpty()) {
            // Python became unavailable — fail the request
            req.cb({false, {}, "Python not available", -1});
            continue;
        }

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
            if (QFileInfo::exists(venv2))
                python_exe = venv2;
        } else {
            script_path = scripts_dir_ + "/" + req.script;
            // Route to correct venv based on script name
            QString venv_name = select_venv_for_script(req.script);
            QString venv_py = PythonSetupManager::instance().python_path(venv_name);
            if (QFileInfo::exists(venv_py))
                python_exe = venv_py;
        }

        auto* proc = new QProcess(this);
        QStringList full_args;

        // If the script is inside a sub-package (contains '/'), use -m <module>
        // so Python can resolve relative imports (from .core import ...).
        if (!is_code && req.script.contains('/')) {
            QString module = req.script;
            module.remove(".py");
            module.replace('/', '.');
            full_args << "-m" << module;
        } else {
            full_args << script_path;
        }
        if (!is_code) {
            // Spill large args to temp files to avoid Windows 32KB command-line limit.
            // Python scripts support "@/path/to/file" — they read and delete the file.
            static constexpr int kArgSpillThreshold = 8192; // 8 KB
            QStringList spilled_files;
            QStringList safe_args;
            for (const QString& arg : req.args) {
                if (arg.size() > kArgSpillThreshold) {
                    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
                    QString temp_path = temp_dir + "/fincept_arg_" +
                                       QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
                    QFile tf(temp_path);
                    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        tf.write(arg.toUtf8());
                        tf.close();
                        safe_args << ("@" + temp_path);
                        spilled_files << temp_path;
                    } else {
                        safe_args << arg; // fallback: pass as-is (may fail, but don't crash)
                    }
                } else {
                    safe_args << arg;
                }
            }
            full_args.append(safe_args);
            // Store spilled paths so we can clean them up if the process errors out
            // (Python scripts clean them up on success via resolve_arg)
            if (!spilled_files.isEmpty()) {
                // Attach to process via dynamic property for cleanup in error handler
                proc->setProperty("spilled_files", QVariant::fromValue(spilled_files));
            }
        }

        // Start from the shared base env (encoding pins, FINCEPT_DATA_DIR,
        // FINAGENT_DATA_DIR, base PYTHONPATH = scripts_dir_).
        QProcessEnvironment env = build_python_env();

        // Script-specific: if the script is inside a sub-package (contains '/'),
        // prepend the parent-of-pkg dir so relative imports resolve.
        if (!is_code && req.script.contains('/')) {
#ifdef _WIN32
            const QChar kPathSep = ';';
#else
            const QChar kPathSep = ':';
#endif
            QString script_dir = QFileInfo(scripts_dir_ + "/" + req.script).dir().absolutePath();
            QString parent_of_pkg = QFileInfo(script_dir).dir().absolutePath();
            QString pypath = env.value("PYTHONPATH");
            if (parent_of_pkg != scripts_dir_ && !pypath.contains(parent_of_pkg)) {
                pypath = parent_of_pkg + kPathSep + pypath;
                env.insert("PYTHONPATH", pypath);
            }
        }

        proc->setProcessEnvironment(env);
        proc->setWorkingDirectory(scripts_dir_);

#ifdef _WIN32
        proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        // Initialize incremental output buffers (carry stream callback from the request)
        proc_buffers_[proc] = {};
        proc_buffers_[proc].on_line = req.on_line;

        // Helper: drain complete \n-terminated lines from a buffer starting at `offset`,
        // invoke `on_line(line, is_stderr)` per line, advance `offset`. Trailing
        // partial line (no \n yet) stays in the buffer for the next read.
        auto drain_lines = [this, proc](bool is_stderr) {
            auto& bufs = proc_buffers_[proc];
            if (!bufs.on_line) return;
            QByteArray& buf = is_stderr ? bufs.stderr_buf : bufs.stdout_buf;
            int& off = is_stderr ? bufs.stderr_streamed : bufs.stdout_streamed;
            while (true) {
                int nl = buf.indexOf('\n', off);
                if (nl < 0) break;
                // Line is buf[off..nl) — exclude the trailing \n; also trim \r for CRLF
                QByteArray line = buf.mid(off, nl - off);
                if (line.endsWith('\r')) line.chop(1);
                off = nl + 1;
                bufs.on_line(QString::fromUtf8(line), is_stderr);
            }
        };

        // Buffer stdout/stderr incrementally. After each append, drain complete lines
        // to the optional stream callback. The full buffer is still available for the
        // `finished` handler's authoritative parse.
        connect(proc, &QProcess::readyReadStandardOutput, this,
                [this, proc, drain_lines]() {
                    proc_buffers_[proc].stdout_buf.append(proc->readAllStandardOutput());
                    drain_lines(false);
                });
        connect(proc, &QProcess::readyReadStandardError, this,
                [this, proc, drain_lines]() {
                    proc_buffers_[proc].stderr_buf.append(proc->readAllStandardError());
                    drain_lines(true);
                });

        auto cb = std::move(req.cb);
        auto script_name = std::move(req.script);

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, proc, cb = std::move(cb), script_name, is_code, temp_file](int exit_code, QProcess::ExitStatus) {
                    // Collect any remaining buffered data
                    auto& bufs = proc_buffers_[proc];
                    bufs.stdout_buf.append(proc->readAllStandardOutput());
                    bufs.stderr_buf.append(proc->readAllStandardError());

                    QString stdout_str = QString::fromUtf8(bufs.stdout_buf);
                    QString stderr_str = QString::fromUtf8(bufs.stderr_buf);

                    proc_buffers_.remove(proc);
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
                        result.output = std::move(stdout_str);
                        result.error = std::move(stderr_str);
                    } else {
                        // For scripts: extract JSON from output
                        QString json_out = extract_json(stdout_str);
                        result.success = (exit_code == 0 && !json_out.isEmpty());
                        result.output = json_out.isEmpty() ? std::move(stdout_str) : std::move(json_out);
                        result.error = std::move(stderr_str);
                    }

                    if (!result.success && !is_code) {
                        LOG_ERROR("Python", QString("Script %1 failed (exit=%2): %3")
                                                .arg(script_name)
                                                .arg(exit_code)
                                                .arg(result.error.left(200)));
                    }

                    cb(std::move(result));

                    --active_count_;
                    start_next(); // drain queue
                });

        connect(proc, &QProcess::errorOccurred, this, [this, proc, cb, is_code, temp_file](QProcess::ProcessError) {
            QString error_msg = proc->errorString();
            proc_buffers_.remove(proc);
            // Clean up any spilled arg temp files
            auto spilled = proc->property("spilled_files").toStringList();
            for (const QString& f : spilled)
                QFile::remove(f);
            proc->deleteLater();
            if (is_code && !temp_file.isEmpty())
                QFile::remove(temp_file);
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
    // Find the first '{' or '[' and return everything from that point.
    // Scripts print multi-line indented JSON, so we cannot take a single line.
    int brace = output.indexOf('{');
    int bracket = output.indexOf('[');

    int start = -1;
    if (brace >= 0 && bracket >= 0)
        start = qMin(brace, bracket);
    else if (brace >= 0)
        start = brace;
    else if (bracket >= 0)
        start = bracket;

    if (start < 0)
        return {};

    return output.mid(start).trimmed();
}

} // namespace fincept::python
