#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <functional>

namespace fincept::python {

struct PythonResult {
    bool    success = false;
    QString output;
    QString error;
    int     exit_code = -1;
};

/// Runs Python scripts as subprocesses and returns JSON output.
/// Locates python from venv or system PATH.
/// Limits concurrent processes to avoid overwhelming the system.
class PythonRunner : public QObject {
    Q_OBJECT
public:
    using Callback = std::function<void(PythonResult)>;

    static PythonRunner& instance();

    /// Run a script asynchronously. Callback invoked on Qt event loop.
    /// Requests are queued if max concurrency is reached.
    void run(const QString& script, const QStringList& args, Callback cb);

    /// Resolve path to python executable
    QString python_path() const;

    /// Resolve path to scripts directory
    QString scripts_dir() const;

    /// Check if python is available
    bool is_available() const;

    /// Set max concurrent Python processes (default: 3)
    void set_max_concurrent(int n) { max_concurrent_ = n; }

private:
    PythonRunner();
    QString find_python() const;
    QString find_scripts_dir() const;
    void start_next();

    QString python_path_;
    QString scripts_dir_;

    // Concurrency limiter
    static constexpr int DEFAULT_MAX_CONCURRENT = 3;
    int max_concurrent_ = DEFAULT_MAX_CONCURRENT;
    int active_count_   = 0;

    struct QueuedRequest {
        QString     script;
        QStringList args;
        Callback    cb;
    };
    QQueue<QueuedRequest> queue_;
};

/// Extract last JSON line/block from Python output
QString extract_json(const QString& output);

} // namespace fincept::python
