#pragma once
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQueue>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::python {

struct PythonResult {
    bool success = false;
    QString output;
    QString error;
    int exit_code = -1;
};

/// Runs Python scripts as subprocesses and returns JSON output.
/// Locates python from venv or system PATH.
/// Limits concurrent processes to avoid overwhelming the system.
class PythonRunner : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(PythonResult)>;
    /// Per-line streaming callback. Invoked on the thread that owns PythonRunner
    /// (main thread in practice) as each complete `\n`-terminated line arrives.
    /// `is_stderr` is true for stderr lines, false for stdout.
    using StreamCallback = std::function<void(QString line, bool is_stderr)>;

    static PythonRunner& instance();

    /// Run a script asynchronously. Callback invoked on Qt event loop.
    /// Requests are queued if max concurrency is reached.
    /// Optional `on_line` delivers each complete stdout/stderr line as it arrives.
    void run(const QString& script, const QStringList& args, Callback cb,
             StreamCallback on_line = {});

    /// Run arbitrary Python code (for notebook/colab cells).
    /// Creates a temp file, executes it, returns stdout/stderr.
    void run_code(const QString& code, Callback cb);

    /// Resolve path to python executable
    QString python_path() const;

    /// Resolve path to scripts directory
    QString scripts_dir() const;

    /// Build the standard Python process environment used by all finagent/PyFincept
    /// subprocess spawns. Sets PYTHONIOENCODING, PYTHONDONTWRITEBYTECODE,
    /// PYTHONUNBUFFERED, FINCEPT_DATA_DIR, FINAGENT_DATA_DIR,
    /// FINAGENT_RUNTIME_CACHE_SIZE, and PYTHONPATH (derived from scripts_dir()).
    /// Callers that run a script inside a sub-package may still need to prepend
    /// a "parent-of-pkg" directory to PYTHONPATH themselves — this helper only
    /// sets the shared base env.
    QProcessEnvironment build_python_env() const;

    /// Check if python is available
    bool is_available() const;

    /// Set max concurrent Python processes (default: 3)
    void set_max_concurrent(int n) { max_concurrent_ = n; }

  private:
    PythonRunner();
    QString find_python_sync() const;
    void find_python_async();
    QString find_scripts_dir() const;
    void start_next();

    QString python_path_;
    QString scripts_dir_;
    bool python_init_done_ = false;

    // Concurrency limiter
    static constexpr int DEFAULT_MAX_CONCURRENT = 3;
    int max_concurrent_ = DEFAULT_MAX_CONCURRENT;
    int active_count_ = 0;

    struct QueuedRequest {
        QString script;
        QStringList args;
        Callback cb;
        StreamCallback on_line;
    };
    QQueue<QueuedRequest> queue_;

    // Incremental output buffering per process
    struct ProcessBuffers {
        QByteArray stdout_buf;
        QByteArray stderr_buf;
        int stdout_streamed = 0;  // offset into stdout_buf already streamed
        int stderr_streamed = 0;
        StreamCallback on_line;
    };
    QHash<QProcess*, ProcessBuffers> proc_buffers_;
};

/// Extract last JSON line/block from Python output
QString extract_json(const QString& output);

} // namespace fincept::python
